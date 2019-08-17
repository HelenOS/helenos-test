#!/bin/sh

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

# Find out the path to the script.
SOURCE_DIR=`which -- "$0" 2>/dev/null`
# Maybe we are running bash.
[ -z "$SOURCE_DIR" ] && SOURCE_DIR=`which -- "$BASH_SOURCE"`
[ -z "$SOURCE_DIR" ] && exit 1
SOURCE_DIR=`dirname -- "$SOURCE_DIR"`
SOURCE_DIR=`cd $SOURCE_DIR && echo $PWD`

CONFIG_RULES="${SOURCE_DIR}/HelenOS.config"
CONFIG_DEFAULTS="${SOURCE_DIR}/defaults"


test "$#" -eq 1 && { test "$1" = "-h" || test "$1" = "--help"; }
want_help="$?"

if [ "$#" -gt 1 ] || [ "$want_help" -eq 0 ]; then

	# Find all the leaf subdirectories in the defaults directory.
	PROFILES=`find ${CONFIG_DEFAULTS} -type d -links 2 -printf "%P\n" | sort`

	echo "Configures the current working directory as a HelenOS build directory."
	echo "In-tree build is not supported, you must create a separate directory for build."
	echo
	echo "Usage:"
	echo "	$0 -h|--help"
	echo "	$0 [PROFILE]"
	echo
	echo "If profile is not specified, a graphical configuration utility is launched."
	echo
	echo "Possible profiles:"
	printf "\t%s\n" $PROFILES
	echo

	exit "$want_help"
fi

if [ "$PWD" = "$SOURCE_DIR" ]; then
	echo "We don't support in-tree build."
	echo "Please create a build directory, cd into it, and run this script from there."
	echo "Or run \`$0 --help\` to see usage."
	exit 1
fi

ninja_help() {
	echo 'Run  `ninja config`      to adjust configuration.'
	echo 'Run  `ninja`             to build all program and library binaries, but not bootable image.'
	echo 'Run  `ninja image_path`  to build boot image. The file image_path will contain path to the boot image file.'
}

if [ -f build.ninja ]; then
	echo "This build directory was already configured."
	echo
	ninja_help
	exit 0
fi

# Link tools directory for convenience.
ln -s "${SOURCE_DIR}/tools" tools

# Run HelenOS config tool.
if [ "$#" -eq 1 ]; then
	"${SOURCE_DIR}/tools/config.py" "${CONFIG_RULES}" "${CONFIG_DEFAULTS}" hands-off "$1" || exit 1
else
	"${SOURCE_DIR}/tools/config.py" "${CONFIG_RULES}" "${CONFIG_DEFAULTS}" || exit 1
fi

PLATFORM=`sed -n '/^PLATFORM\b/p' Makefile.config | sed 's:[^=]*= ::'`
MACHINE=`sed -n '/^MACHINE\b/p' Makefile.config | sed 's:[^=]*= ::'`

cross_target="$PLATFORM"
if [ "$PLATFORM" = 'abs32le' ]; then
	cross_target='ia32'
fi
if [ "$MACHINE" = 'bmalta' ]; then
	cross_target='mips32eb'
fi

meson "${SOURCE_DIR}" '.' --cross-file "${SOURCE_DIR}/meson/cross/${cross_target}" || exit 1

echo
echo "Configuration for platform $PLATFORM finished."
echo
ninja_help