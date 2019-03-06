#!/bin/bash

readonly BASE_URL=http://luajit.org/download/
readonly FILE_URL=LuaJIT-2.0.5.tar.gz
readonly PRODUCT=luajit
readonly VERSION=2.0.5

function luajit_configure() {
	export MAKEFLAGS="-e"
	export BUILDMODE=static
	export PREFIX=`pwd`/build
	export INSTALL_LIB=$PREFIX/lib/$CONF_TARGET
	export INSTALL_BIN=$PREFIX/bin/$CONF_TARGET
	
	# These are where the .lua files go. Do not want them in default
	# folder as that includes the version, which complicates things
	# in bob, where LUAJIT_PATH must be matched to the location of
	# these files.
	export INSTALL_LJLIBD=$PREFIX/share/luajit

	case $CONF_TARGET in
		armv7-darwin)
			;;
		armv7-android)
			;;
		*)
			return
			;;
	esac

	# Time to second guess the luajit make file trickeries
	# for cross-compiles!

	# We pass in cross compilation set up in CFLAGS/etc. We need to
	# transfer those to TARGET* flags and then restore HOST* values
	# to what is used to not cross-compile.
	#
	# Host need to generate same pointer size as target, though.
	# Which means we do -m32 for that.

	# Flags set both for host & target compilation
	XFLAGS="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"

	# These will be used for the cross compiling
	export TARGET_TCFLAGS="$CFLAGS $XFLAGS"
	export TARGET_CFLAGS="$CFLAGS $XFLAGS"
	export TARGET_LDFLAGS="$CFLAGS"
	export TARGET_AR="$AR rcus"
	export TARGET_LD="$CC $CFLAGS"

	# These are used for host compiling
	export HOST_CC=gcc
	export HOST_LD=true
	export HOST_CFLAGS="$XFLAGS -m32 -I."
	export HOST_XCFLAGS=""
	export HOST_ALDFLAGS=-m32

	# Disable
	export TARGET_STRIP=true
	export CCOPTIONS=
}

# Use above function instead of shell scripts
CONFIGURE_WRAPPER="luajit_configure"
export -f luajit_configure

. ../common.sh

export CONF_TARGET=$1

case $1 in
	armv7-darwin)
		export TARGET_SYS=iOS
		;;
	armv7-android)
		export TARGET_SYS=Other
		;;
	linux)
		export TARGET_SYS=Linux
		;;
	x86_64-linux)
		export TARGET_SYS=Linux
		;;
	darwin)
		function cmi_make() {
                    set -e
                    make -j8
                    make install
                    set +e
                    cp src/lj.supp $PREFIX/share/luajit
		}
		;;
	win32|x86_64-win32)
        cmi_setup_vs2015_env $1
        
		function cmi_make() {
			cd src
			cmd "/C msvcbuild.bat static dummy ${CONF_TARGET} "
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/include/luajit-2.0
			mkdir -p $PREFIX/share
			mkdir -p $PREFIX/share/lua/5.1
			mkdir -p $PREFIX/share/luajit/jit
			mkdir -p $PREFIX/share/man/man1
			cp libluajit*.lib $PREFIX/lib/$CONF_TARGET
			cp luajit.exe $PREFIX/bin/$CONF_TARGET
			cp luajit.h lauxlib.h lua.h lua.hpp luaconf.h lualib.h $PREFIX/include/luajit-2.0
			cp lj.supp $PREFIX/share/luajit
			cp -r jit $PREFIX/share/luajit
			cp ../etc/luajit.1 $PREFIX/share/man/man1
		}
		;;
esac

download
cmi $1
