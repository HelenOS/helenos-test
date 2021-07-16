#
# Copyright (c) 2005 Martin Decky
# Copyright (c) 2007 Jakub Jermar
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

deps = [ 'clui', 'fmtutil' ]
includes += include_directories('.', 'cmds', 'cmds/builtins', 'cmds/modules')
src = files(
	'cmds/builtin_cmds.c',
	'cmds/builtins/batch/batch.c',
	'cmds/builtins/builtin_aliases.c',
	'cmds/builtins/builtins.c',
	'cmds/builtins/cd/cd.c',
	'cmds/builtins/exit/exit.c',
	'cmds/mod_cmds.c',
	'cmds/modules/alias/alias.c',
	'cmds/modules/cat/cat.c',
	'cmds/modules/cmp/cmp.c',
	'cmds/modules/cp/cp.c',
	'cmds/modules/echo/echo.c',
	'cmds/modules/help/help.c',
	'cmds/modules/kcon/kcon.c',
	'cmds/modules/ls/ls.c',
	'cmds/modules/mkdir/mkdir.c',
	'cmds/modules/mkfile/mkfile.c',
	'cmds/modules/module_aliases.c',
	'cmds/modules/modules.c',
	'cmds/modules/mount/mount.c',
	'cmds/modules/mv/mv.c',
	'cmds/modules/printf/printf.c',
	'cmds/modules/pwd/pwd.c',
	'cmds/modules/rm/rm.c',
	'cmds/modules/sleep/sleep.c',
	'cmds/modules/touch/touch.c',
	'cmds/modules/unalias/unalias.c',
	'cmds/modules/unmount/unmount.c',
	'cmds/modules/grep/grep.c',
	'compl.c',
	'errors.c',
	'exec.c',
	'input.c',
	'scli.c',
	'tok.c',
	'util.c',
)

test_src = files(
	'tok.c',
	'test/toktest.c',
)

# TODO: install this file somewhere sane
installed_data += { 'name': 'demo.txt', 'dir': '/' }
