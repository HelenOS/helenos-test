#!/usr/bin/env python3
#
# Copyright (c) 2013 Jakub Jermar
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# - The name of the author may not be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#


"""
Emulator wrapper for running HelenOS
"""

import inspect
import os
import platform
import re
import subprocess
import sys
import _thread
import time

overrides = {}

CONFIG = 'Makefile.config'

TOOLS_DIR = os.path.dirname(inspect.getabsfile(inspect.currentframe()))

def read_config():
	"Read HelenOS build configuration"

	inf = open(CONFIG, 'r')
	config = {}

	for line in inf:
		res = re.match(r'^(?:#!# )?([^#]\w*)\s*=\s*(.*?)\s*$', line)
		if (res):
			config[res.group(1)] = res.group(2)

	inf.close()
	return config

def is_override(str):
	if str in overrides.keys():
		return overrides[str]
	return False

def cfg_get(platform, machine, processor):
	if machine == "" or "run" in emulators[platform]:
		return emulators[platform]
	elif processor == "" or "run" in emulators[platform][machine]:
		return emulators[platform][machine]
	else:
		return emulators[platform][machine][processor]

def termemu_detect():
	emus = ['gnome-terminal', 'xfce4-terminal', 'xterm']
	for termemu in emus:
		try:
			subprocess.check_output('which ' + termemu, shell = True)
			return termemu
		except:
			pass

	print('Could not find any of the terminal emulators %s.'%(emus))
	sys.exit(1)

def run_in_console(cmd, title):
	temu = termemu_detect()
	if temu == 'gnome-terminal':
		cmdline = temu + ' -- ' + cmd
	else:
		ecmd = cmd.replace('"', '\\"')
		cmdline = temu + ' -T ' + '"' + title + '"' + ' -e "' + ecmd + '"'

	print(cmdline)
	if not is_override('dryrun'):
		subprocess.call(cmdline, shell = True)

def get_host_native_width():
	return int(platform.architecture()[0].strip('bit'))

def pc_options(guest_width):
	opts = ''

	# Do not enable KVM if running 64 bits HelenOS
	# on 32 bits host
	host_width = get_host_native_width()
	if guest_width <= host_width and not is_override('nokvm'):
		opts = opts + ' -enable-kvm'

	# Remove the leading space
	return opts[1:]

def malta_options():
	return '-cpu 4Kc -append "console=devices/\\hw\\pci0\\00:0a.0\\com1\\a"'

def find_firmware(name, environ_var, default_paths, extra_info=None):
	"""Find firmware image(s)."""

	if environ_var in os.environ:
		return os.environ[environ_var]

	for path in default_paths:
		if os.path.exists(path):
			return path

	sys.stderr.write("Cannot find %s binary image(s)!\n" % name)
	sys.stderr.write(
	    "Either set %s environment variable accordingly or place the image(s) in one of the default locations: %s.\n" %
	    (environ_var, ", ".join(default_paths)))
	if extra_info is not None:
		sys.stderr.write(extra_info)
	return None

def platform_to_qemu_options(platform, machine, processor):
	if platform == 'amd64':
		return 'system-x86_64', pc_options(64)
	elif platform == 'arm32':
		if machine == 'integratorcp':
			return 'system-arm', '-M integratorcp'
		elif machine == 'raspberrypi':
			return 'system-arm', '-M raspi1ap'
	elif platform == 'arm64':
		# Search for the EDK2 firmware image
		default_paths = (
			'/usr/local/qemu-efi-aarch64/QEMU_EFI.fd', # Custom
			'/usr/share/edk2/aarch64/QEMU_EFI.fd',     # Fedora
			'/usr/share/qemu-efi-aarch64/QEMU_EFI.fd', # Ubuntu
		)
		extra_info = ("Pre-compiled binary can be obtained from "
		    "http://snapshots.linaro.org/components/kernel/leg-virt-tianocore-edk2-upstream/latest/QEMU-AARCH64/RELEASE_GCC5/QEMU_EFI.fd.\n")
		efi_path = find_firmware(
		    "EDK2", 'EW_QEMU_EFI_AARCH64', default_paths, extra_info)
		if efi_path is None:
			raise Exception

		return 'system-aarch64', \
		    '-M virt -cpu cortex-a57 -m 1024 -bios %s' % efi_path
	elif platform == 'ia32':
		return 'system-i386', pc_options(32)
	elif platform == 'mips32':
		if machine == 'lmalta':
			return 'system-mipsel', malta_options()
		elif machine == 'bmalta':
			return 'system-mips', malta_options()
	elif platform == 'ppc32':
		return 'system-ppc', '-m 256'
	elif platform == 'sparc64':
		if machine != 'generic':
			raise Exception
		if processor == 'us':
			return 'system-sparc64', '-M sun4u --prom-env boot-args="console=devices/\\hw\\pci0\\01:01.0\\com1\\a"'

		# processor = 'sun4v'
		opensparc_bins = find_firmware(
		    "OpenSPARC", 'OPENSPARC_BINARIES',
		    ('/usr/local/opensparc/image/', ))
		if opensparc_bins is None:
			raise Exception

		return 'system-sparc64', '-M niagara -m 256 -L %s' % (opensparc_bins)


def hdisk_mk():
	if not os.path.exists('hdisk.img'):
		subprocess.call(TOOLS_DIR + '/mkfat.py 1048576 dist/data hdisk.img', shell = True)

def qemu_bd_options():
	if is_override('nohdd'):
		return ''

	hdisk_mk()

	hdd_options = ''
	if 'hdd' in overrides.keys():
		if 'ata' in overrides['hdd'].keys():
			hdd_options += ''
		elif 'virtio-blk' in overrides['hdd'].keys():
			hdd_options += ',if=virtio'

	return ' -drive file=hdisk.img,index=0,media=disk,format=raw' + hdd_options

def qemu_nic_ne2k_options():
	return ' -device ne2k_isa,irq=5,netdev=n1'

def qemu_nic_e1k_options():
	return ' -device e1000,netdev=n1'

def qemu_nic_rtl8139_options():
	return ' -device rtl8139,netdev=n1'

def qemu_nic_virtio_options():
	return ' -device virtio-net,netdev=n1'

def qemu_net_options():
	if is_override('nonet'):
		return ''

	nic_options = ''
	if 'net' in overrides.keys():
		if 'e1k' in overrides['net'].keys():
			nic_options += qemu_nic_e1k_options()
		if 'rtl8139' in overrides['net'].keys():
			nic_options += qemu_nic_rtl8139_options()
		if 'ne2k' in overrides['net'].keys():
			nic_options += qemu_nic_ne2k_options()
		if 'virtio-net' in overrides['net'].keys():
			nic_options += qemu_nic_virtio_options()
	else:
		# Use the default NIC
		nic_options += qemu_nic_e1k_options()

	return nic_options + ' -netdev user,id=n1,hostfwd=udp::8080-:8080,hostfwd=udp::8081-:8081,hostfwd=tcp::8080-:8080,hostfwd=tcp::8081-:8081,hostfwd=tcp::2223-:2223'

def qemu_usb_options():
	if is_override('nousb'):
		return ''
	return ' -usb'

def qemu_xhci_options():
	if is_override('noxhci'):
		return ''
	return ' -device nec-usb-xhci,id=xhci'

def qemu_tablet_options():
	if is_override('notablet') or (is_override('nousb') and is_override('noxhci')):
		return ''
	return ' -device usb-tablet'

def qemu_audio_options():
	if is_override('nosnd'):
		return ''
	return ' -device intel-hda -device hda-duplex'

def qemu_run(platform, machine, processor):
	cfg = cfg_get(platform, machine, processor)
	suffix, options = platform_to_qemu_options(platform, machine, processor)
	cmd = 'qemu-' + suffix

	if cpus:
        cmd += ' -smp cpus=' + cpus + ' '

	cmdline = cmd
	if 'qemu_path' in overrides.keys():
		cmdline = overrides['qemu_path'] + cmd

	if options != '':
		cmdline += ' ' + options

	if (not 'hdd' in cfg.keys() or cfg['hdd']):
		cmdline += qemu_bd_options()
	if (not 'net' in cfg.keys()) or cfg['net']:
		cmdline += qemu_net_options()
	if (not 'usb' in cfg.keys()) or cfg['usb']:
		cmdline += qemu_usb_options()
	if (not 'xhci' in cfg.keys()) or cfg['xhci']:
		cmdline += qemu_xhci_options()
	if (not 'tablet' in cfg.keys()) or cfg['tablet']:
		cmdline += qemu_tablet_options()
	if (not 'audio' in cfg.keys()) or cfg['audio']:
		cmdline += qemu_audio_options()

	console = ('console' in cfg.keys() and cfg['console'])

	if (is_override('nographic')):
		cmdline += ' -nographic'

	if (not console and (not is_override('nographic')) and not is_override('noserial')):
		cmdline += ' -serial stdio'

	if (is_override('bigmem')):
		cmdline += ' -m 4G'

	if cfg['image'] == 'image.iso':
		cmdline += ' -boot d -cdrom image.iso'
	elif cfg['image'] == 'image.iso@arm64':
		# Define image.iso cdrom backend.
		cmdline += ' -drive if=none,file=image.iso,id=cdrom,media=cdrom'
		# Define scsi bus.
		cmdline += ' -device virtio-scsi-device'
		# Define cdrom frontend connected to this scsi bus.
		cmdline += ' -device scsi-cd,drive=cdrom'
	elif cfg['image'] == 'image.boot':
		cmdline += ' -kernel image.boot'
	elif cfg['image'] == 'kernel.img@rpi':
		cmdline += ' -bios boot/image.boot.bin'
	else:
		cmdline += ' ' + cfg['image']

	if console:
		cmdline += ' -nographic'

		title = 'HelenOS/' + platform
		if machine != '':
			title += ' on ' + machine
		if 'expect' in cfg.keys():
			cmdline = 'expect -c \'spawn %s; expect "%s" { send "%s" } timeout exp_continue; interact\'' % (cmdline, cfg['expect']['src'], cfg['expect']['dst'])
		run_in_console(cmdline, title)
	else:
		print(cmdline)
		if not is_override('dryrun'):
			subprocess.call(cmdline, shell = True)

def ski_run(platform, machine, processor):
	run_in_console('ski -i ' + TOOLS_DIR + '/conf/ski.conf', 'HelenOS/ia64 on ski')

def msim_run(platform, machine, processor):
	hdisk_mk()
	run_in_console('msim -c ' + TOOLS_DIR + '/conf/msim.conf', 'HelenOS/mips32 on msim')

def spike_run(platform, machine, processor):
	run_in_console('spike -m1073741824:1073741824 image.boot', 'HelenOS/risvc64 on Spike')

emulators = {
	'amd64' : {
		'run' : qemu_run,
		'image' : 'image.iso'
	},
	'arm32' : {
		'integratorcp' : {
			'run' : qemu_run,
			'image' : 'image.boot',
			'net' : False,
			'audio' : False,
			'xhci' : False,
			'tablet' : False
		},
		'raspberrypi' : {
			'run' : qemu_run,
			'image' : 'kernel.img@rpi',
			'audio' : False,
			'console' : True,
			'hdd' : False,
			'net' : False,
			'tablet' : False,
			'usb' : False,
			'xhci' : False
		},
	},
	'arm64' : {
		'virt' : {
			'run' : qemu_run,
			'image' : 'image.iso@arm64',
			'audio' : False,
			'console' : True,
			'hdd' : False,
			'net' : False,
			'tablet' : False,
			'usb' : False,
			'xhci' : False
		}
	},
	'ia32' : {
		'run' : qemu_run,
		'image' : 'image.iso'
	},
	'ia64' : {
		'ski' : {
			'run' : ski_run
		}
	},
	'mips32' : {
		'msim' : {
			'run' : msim_run
		},
		'lmalta' : {
			'run' : qemu_run,
			'image' : 'image.boot',
			'console' : True
		},
		'bmalta' : {
			'run' : qemu_run,
			'image' : 'image.boot',
			'console' : True
		},
	},
	'ppc32' : {
		'run' : qemu_run,
		'image' : 'image.iso',
		'audio' : False
	},
	'riscv64' : {
		'run' : spike_run,
		'image' : 'image.boot'
	},
	'sparc64' : {
		'generic' : {
			'us' : {
				'run' : qemu_run,
				'image' : 'image.iso',
				'audio' : False,
				'console' : True,
				'net' : False,
				'usb' : False,
				'xhci' : False,
				'tablet' : False
			},
			'sun4v' : {
				'run' : qemu_run,
				'image' : '-drive if=pflash,readonly=on,file=image.iso',
				'audio' : False,
				'console' : True,
				'net' : False,
				'usb' : False,
				'xhci' : False,
				'tablet' : False,
				'expect' : {
					'src' : 'ok ',
					'dst' : 'boot\n'
				},
			}
		}
	},
}

def usage():
	print("%s - emulator wrapper for running HelenOS\n" % os.path.basename(sys.argv[0]))
	print("%s [$] [-d] [-h] [-net e1k|rtl8139|ne2k|virtio-net] [-hdd ata|virtio-blk] [-nohdd] [-nokvm] [-nonet] [-nosnd] [-nousb] [-noxhci] [-notablet]\n" %
	    os.path.basename(sys.argv[0]))
	print("$\t to run multiple($) cpus")
	print("-d\tDry run: do not run the emulation, just print the command line.")
	print("-h\tPrint the usage information and exit.")
	print("-nohdd\tDisable hard disk, if applicable.")
	print("-nokvm\tDisable KVM, if applicable.")
	print("-nonet\tDisable networking support, if applicable.")
	print("-nosnd\tDisable sound, if applicable.")
	print("-nousb\tDisable USB support, if applicable.")
	print("-noxhci\tDisable XHCI support, if applicable.")
	print("-notablet\tDisable USB tablet (use only relative-position PS/2 mouse instead), if applicable.")
	print("-nographic\tDisable graphical output. Serial port output must be enabled for this to be useful.")
	print("-noserial\tDisable serial port output in the terminal.")
	print("-bigmem\tSets maximum RAM size to 4GB.")

def fail(platform, machine):
	print("Cannot start emulation for the chosen configuration. (%s/%s)" % (platform, machine))


def run():
	expect_nic = False
	expect_hdd = False
	expect_qemu = False
	global cpus
    cpus = ''

	for i in range(1, len(sys.argv)):

		if expect_nic:
			expect_nic = False
			if not 'net' in overrides.keys():
				overrides['net'] = {}
			if sys.argv[i] == 'e1k':
				overrides['net']['e1k'] = True
			elif sys.argv[i] == 'rtl8139':
				overrides['net']['rtl8139'] = True
			elif sys.argv[i] == 'ne2k':
				overrides['net']['ne2k'] = True
			elif sys.argv[i] == 'virtio-net':
				overrides['net']['virtio-net'] = True
			else:
				usage()
				exit()
			continue

		if expect_hdd:
			expect_hdd = False
			if not 'hdd' in overrides.keys():
				overrides['hdd'] = {}
			if sys.argv[i] == 'ata':
				overrides['hdd']['ata'] = True
			elif sys.argv[i] == 'virtio-blk':
				overrides['hdd']['virtio-blk'] = True
			else:
				usage()
				exit()
			continue

		if expect_qemu:
			expect_qemu = False
			overrides['qemu_path'] = sys.argv[i]

		elif sys.argv[i] == '-h':
			usage()
			exit()
		elif sys.argv[i].isdigit():
            cpus = sys.argv[i]
		elif sys.argv[i] == '-d':
			overrides['dryrun'] = True
		elif sys.argv[i] == '-net' and i < len(sys.argv) - 1:
			expect_nic = True
		elif sys.argv[i] == '-hdd' and i < len(sys.argv) - 1:
			expect_hdd = True
		elif sys.argv[i] == '-nohdd':
			overrides['nohdd'] = True
		elif sys.argv[i] == '-nokvm':
			overrides['nokvm'] = True
		elif sys.argv[i] == '-nonet':
			overrides['nonet'] = True
		elif sys.argv[i] == '-nosnd':
			overrides['nosnd'] = True
		elif sys.argv[i] == '-nousb':
			overrides['nousb'] = True
		elif sys.argv[i] == '-noxhci':
			overrides['noxhci'] = True
		elif sys.argv[i] == '-notablet':
			overrides['notablet'] = True
		elif sys.argv[i] == '-nographic':
			overrides['nographic'] = True
		elif sys.argv[i] == '-bigmem':
			overrides['bigmem'] = True
		elif sys.argv[i] == '-noserial':
			overrides['noserial'] = True
		elif sys.argv[i] == '-qemu_path' and i < len(sys.argv) - 1:
			expect_qemu = True
		else:
			usage()
			exit()

	config = read_config()

	if 'PLATFORM' in config.keys():
		platform = config['PLATFORM']
	else:
		platform = ''

	if 'MACHINE' in config.keys():
		mach = config['MACHINE']
	else:
		mach = ''

	if 'PROCESSOR' in config.keys():
		processor = config['PROCESSOR']
	else:
		processor = ''

	try:
		emu_run = cfg_get(platform, mach, processor)['run']
		emu_run(platform, mach, processor)
	except:
		fail(platform, mach)
		return

run()
