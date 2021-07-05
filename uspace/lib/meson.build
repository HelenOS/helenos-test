#
# Copyright (c) 2019 Jiří Zárevúcky
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

always_static = not CONFIG_BUILD_SHARED_LIBS

# Which libraries are installed when CONFIG_DEVEL_FILES is enabled
installed_libs = [
	'c',
	'math',
	'display',
	'pixconv',
	'posix',
	'clui',
	'pcm',
	'hound',
	'gfx',
	'gfximage',
	'ipcgfx',
	'congfx',
	'display',
	'ui',
]

# IMPORTANT: Dependencies must be listed before libs that depend on them.
libs = [
	'c',

	'inet',

	'device',

	'block',
	'clui',
	'compress',
	'conf',
	'cpp',
	'crypto',
	'dltest',
	'fbfont',
	'fdisk',
	'fmtutil',
	'fs',
	'gfx',
	'http',
	'label',
	'math',
	'minix',
	'nettl',
	'pcm',
	'pcut',
	'pixconv',
	'posix',
	'riff',
	'scsi',
	'sif',
	'trackmod',
	'untar',
	'uri',

	'bithenge',
	'congfx',
	'drv',
	'ext4',
	'gfxfont',
	'gfximage',
	'hound',
	'ipcgfx',
	'memgfx',
	'nic',
	'usb',
	'usbdev',
	'usbhid',
	'usbhost',
	'usbvirt',
	'virtio',

	'ieee80211',
	'ddev',
	'display',

	'ui',
]

# Generated list of include directory paths
include_paths = []

# Generated list of test sources
testfiles = []

# Text of the install script.
uspace_lib_devel_install_script_text = []

foreach l : libs
	# Variables that might be defined in library meson files:

	# List of source files.
	# To set it correctly, use files('list.c', 'of.c', 'files.c')
	# Often this is the only variable that needs to be set.
	src = files()

	# Public include directories. These are used by everyone who depends
	# on this library.
	# Use include_directories('include_dir1', 'include_dir2') to set this.
	# If the variable is omitted, it defaults to 'include' subdirectory
	# if it exists, or the source directory otherwise.
	# If the library has no headers, you can use
	# includes += include_directories()
	includes = []

	# Private include directories.
	# Unnecessary if you always include private headers using relative
	# paths, but may be useful if you need to override system headers.
	private_includes = []

	# List of dependency names.
	# E.g. to use libnic and libuntar use
	# `deps = [ 'nic', 'untar' ]`
	deps = []

	# Extra arguments for the C compiler.
	# Don't use for arguments involving file paths.
	c_args = []

	# Shared object version of the library.
	version = '0.0'

	# Sources of unit tests.
	# Automatically get compiled into a test binary,
	# but not into the library itself.
	test_src = []

	# Language of the library.
	# Currently supported options are 'c' and 'cpp', with 'c' being default.
	language = 'c'

	# Whether the library can be dynamically linked.
	# Eventually, all libraries will be shared by default and this will go away.
	allow_shared = false

	subdir(l)

	# Add `include` or `.` subdirectory to include dirs if needed.
	if includes.length() == 0
		incdir = join_paths(l, 'include')
		if run_command('[', '-d', incdir, ']').returncode() == 0
			includes += include_directories(incdir)

			if installed_libs.contains(l)
				_sdir = meson.current_source_dir() / l / 'include'
				uspace_lib_devel_install_script_text += 'cp -R -L -T "@0@" "${DESTDIR}include/lib@1@"'.format(_sdir, l)
			endif
		else
			includes += include_directories(l)

			if installed_libs.contains(l)
				_sdir = meson.current_source_dir() / l
				uspace_lib_devel_install_script_text += 'mkdir -p "${DESTDIR}include/lib@0@"'.format(l)
				uspace_lib_devel_install_script_text += 'cp -L -t "${DESTDIR}include/lib@0@" "@1@"/*.h || true'.format(l, _sdir)
			endif
		endif
	endif

	# Pretty much everything depends on libc.
	if l != 'c'
		deps += 'c'
	endif

	if language == 'cpp' and l != 'cpp'
		deps += 'cpp'
	endif

	mapfile = meson.current_build_dir() / 'lib' + l + '.map'

	link_args = [ '-Wl,--no-undefined,--no-allow-shlib-undefined' ]
	# We want linker to generate link map for debugging.
	link_args += [ '-Wl,-Map,' + mapfile ]

	# Convert strings to dependency objects
	# TODO: this dependency business is way too convoluted due to issues in current Meson
	_any_deps = []
	_static_deps = []
	_shared_deps = []
	_include_deps = []
	foreach s : deps
		_any_deps += get_variable('lib' + s).get('any')
		_static_deps += get_variable('lib' + s).get('static')
		if not always_static
			_shared_deps += get_variable('lib' + s).get('shared')
		endif
		_include_deps += get_variable('lib' + s).get('include')
	endforeach

	install_static_lib = CONFIG_DEVEL_FILES and installed_libs.contains(l)
	install_shared_lib = allow_shared

	_shared_dep = disabler()

	if src.length() > 0
		if not always_static
			_libname = 'lib' + l + '.so.' + version.split('.')[0]

			_shared_lib = shared_library(l, src,
				# TODO: Include private headers using #include "quoted",
				#       and get rid of private_includes.
				include_directories: [ private_includes, includes ],
				dependencies: _shared_deps,
				c_args: arch_uspace_c_args + c_args,
				cpp_args: arch_uspace_c_args + c_args,
				link_args: arch_uspace_c_args + arch_uspace_link_args + link_args,
				version: version,
				build_by_default: true,
			)

			if install_shared_lib
				install_files += [[ 'lib', _shared_lib.full_path(), _libname ]]
				install_deps += [ _shared_lib ]
			endif

			if install_shared_lib and install_debug_files
				install_files += [[ 'debug/lib', mapfile, _libname + '.map' ]]
			endif

			if disassemble
				_disasm = custom_target('lib' + l + '.disasm',
					command: [ objdump, '-S', '@INPUT@' ],
					input: _shared_lib,
					output: 'lib' + l + '.disasm',
					capture: true,
					build_by_default: true,
				)

				if install_shared_lib and install_debug_files
					install_files += [[ 'debug/lib', _disasm.full_path(), _libname + '.disasm' ]]
					install_deps += [ _disasm ]
				endif
			endif

			_shared_dep = declare_dependency(
				link_with: _shared_lib,
				# TODO: Always use `include` subdirectory for public headers,
				#       and ditch the variable.
				include_directories: includes,
				dependencies: _shared_deps,
			)
		endif

		_static_lib = static_library(l, src,
			# TODO: Include private headers using #include "quoted",
			#       and get rid of private_includes.
			include_directories: [ private_includes, includes ],
			dependencies: _include_deps,
			c_args: arch_uspace_c_args + c_args,
			cpp_args: arch_uspace_c_args + c_args,
		)

		if install_static_lib
			install_files += [[ 'lib', _static_lib.full_path(), 'lib' + l + '.a' ]]
			install_deps += [ _static_lib ]
		endif

		_static_dep = declare_dependency(
			link_with: _static_lib,
			# TODO: Always use `include` subdirectory for public headers,
			#       and ditch the variable.
			include_directories: includes,
			dependencies: _static_deps,
		)

		if always_static or not allow_shared
			_any_dep = declare_dependency(
				link_with: _static_lib,
				# TODO: Always use `include` subdirectory for public headers,
				#       and ditch the variable.
				include_directories: includes,
				dependencies: _any_deps,
			)
		else
			_any_dep = declare_dependency(
				link_with: _shared_lib,
				# TODO: Always use `include` subdirectory for public headers,
				#       and ditch the variable.
				include_directories: includes,
				dependencies: _any_deps,
			)
		endif

		_include_dep = declare_dependency(
			# TODO: Always use `include` subdirectory for public headers,
			#       and ditch the variable.
			include_directories: includes,
			dependencies: _include_deps,
		)
	else
		# Header-only library.
		_any_dep = declare_dependency(
			include_directories: includes,
			dependencies: _any_deps,
		)
		_static_dep = declare_dependency(
			include_directories: includes,
			dependencies: _static_deps,
		)
	endif

	if test_src.length() > 0
		testfiles += [ {
			'name': l,
			'src': test_src,
			'includes': [ private_includes, includes ],
		} ]
	endif

	set_variable('lib' + l, {
		'any': _any_dep,
		'static': _static_dep,
		'shared': _shared_dep,
		'include': _include_dep,
	})
endforeach

if not CONFIG_PCUT_TESTS
	subdir_done()
endif

# Test binaries
foreach test : testfiles
	_testname = test['name']
	_libname = _testname.split('-')[0]
	_ldargs = []
	_test_binname = 'test-lib' + _testname

	if link_map
		# We want linker to generate link map for debugging.
		_ldargs += [ '-Wl,-Map,' + meson.current_build_dir() / _test_binname + '.map' ]
	endif

	_bin = executable(_test_binname, test.get('src'),
		include_directories: test.get('includes'),
		dependencies: [ get_variable('lib' + _libname).get('any'), libpcut.get('any') ],
		link_whole: libstartfiles,
		c_args: arch_uspace_c_args,
		cpp_args: arch_uspace_c_args,
		link_args: arch_uspace_c_args + arch_uspace_link_args + _ldargs,
		build_by_default: true,
	)

	install_files += [[ 'test', _bin.full_path(), _test_binname ]]
	install_deps += [ _bin ]

	if disassemble
		_disasm = custom_target(_test_binname + '.disasm',
			command: [ objdump, '-S', '@INPUT@' ],
			input: _bin,
			output: _test_binname + '.disasm',
			capture: true,
			build_by_default: true,
		)

		if install_debug_files
			install_files += [[ 'debug/test', _disasm.full_path(), _test_binname + '.disasm' ]]
			install_deps += [ _disasm ]
		endif
	endif
endforeach
