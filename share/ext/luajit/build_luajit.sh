#!/bin/bash

readonly BASE_URL=http://luajit.org/download/
readonly FILE_URL=LuaJIT-2.1.0-beta3.tar.gz
readonly PRODUCT=luajit
readonly VERSION=2.1.0-beta3

# Whether to skip including /bin/ in the package, since it's only needed for desktop platforms
TAR_SKIP_BIN=0

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

	# Some architectures (e.g. PPC) can use either single-number (1) or
	# dual-number (2) mode. For clarity we explicitly use LUAJIT_NUMMODE=2
	# for mobile architectures.
	case $CONF_TARGET in
		armv7-darwin)
			TAR_SKIP_BIN=1
			XFLAGS+="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
			export HOST_CC="clang -m32"
			export HOST_CFLAGS="$XFLAGS -m32 -I."
			export HOST_ALDFLAGS="-m32"
			;;
		arm64-darwin)
			TAR_SKIP_BIN=1
			XFLAGS+="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
			export HOST_CC="clang -m64"
			export HOST_CFLAGS="$XFLAGS -m64 -I."
			export HOST_ALDFLAGS="-m64"
			;;
		x86_64-ios)
			TAR_SKIP_BIN=1
			XFLAGS+="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT -DLJ_NO_SYSTEM"
			export HOST_CC="clang -m64"
			export HOST_CFLAGS="$XFLAGS -m64 -I."
			export HOST_ALDFLAGS="-m64"
			;;
		armv7-android)
			TAR_SKIP_BIN=1
			XFLAGS="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
			export CROSS="" # is this needed for clang? -> "${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/toolchains/arm-linux-androideabi-${ANDROID_GCC_VERSION}/prebuilt/${platform}-x86_64/bin/arm-linux-androideabi-"
			export HOST_CC="clang -m32"
			export HOST_CFLAGS="$XFLAGS -m32 -I."
			export HOST_ALDFLAGS="-m32"
			;;
		arm64-android)
			TAR_SKIP_BIN=1
			XFLAGS="-DLUAJIT_ENABLE_GC64 -DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
			export CROSS="" # is this needed for clang? -> "${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/toolchains/llvm/prebuilt/${platform}-x86_64/bin/aarch64-linux-android${ANDROID_64_VERSION}-"
			export HOST_CC="clang -m64"
			export HOST_CFLAGS="$XFLAGS -m64 -I."
			export HOST_ALDFLAGS="-m64"
			export TARGET_FLAGS="$CFLAGS"
			;;
		x86_64-linux)
			return
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

	# These will be used for the cross compiling
	export TARGET_TCFLAGS="$CFLAGS $XFLAGS"
	export TARGET_CFLAGS="$CFLAGS $XFLAGS"

	# These are used for host compiling
	export HOST_LD=true
	export HOST_XCFLAGS=""
	export HOST_LDFLAGS="${BUILD_LDFLAGS}"

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
	arm64-darwin)
		export TARGET_SYS=iOS
		;;
	x86_64-ios)
		export TARGET_SYS=iOS
		;;
	armv7-android)
		export TARGET_SYS=Android
		;;
	arm64-android)
		export TARGET_SYS=Android
		;;
	x86_64-linux)
		export TARGET_SYS=Linux
		function cmi_make() {
					TAR_SKIP_BIN=0
					echo "Building x86_64-linux with LUAJIT_ENABLE_GC64=0"
					export DEFOLD_ARCH="32"
					set -e
					make -j8
					make install
					make clean
					set +e
					echo "Building x86_64-linux with LUAJIT_ENABLE_GC64=1"
					export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
					export DEFOLD_ARCH="64"
					set -e
					make -j8
					make install
					set +e
					cp src/lj.supp $PREFIX/share/luajit
		}
		;;
	x86_64-darwin)
		function cmi_make() {
					TAR_SKIP_BIN=0
					echo "Building x86_64-darwin with LUAJIT_ENABLE_GC64=0"
					# Note: Luajit sets this to 10.4, which is less than what we support.
					#       This value is set to the same as MIN_OSX_SDK_VERSION in waf_dynamo.py
					export MACOSX_DEPLOYMENT_TARGET="10.7"
					export DEFOLD_ARCH="32"
					set -e
					make -j8
					make install
					make clean
					set +e
					echo "Building x86_64-darwin with LUAJIT_ENABLE_GC64=1"
					export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
					export DEFOLD_ARCH="64"
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
			export DEFOLD_ARCH="32"
			cmd "/C msvcbuild.bat static dummy ${CONF_TARGET} "
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/include/luajit-2.0
			mkdir -p $PREFIX/share
			mkdir -p $PREFIX/share/lua/5.1
			mkdir -p $PREFIX/share/luajit/jit
			mkdir -p $PREFIX/share/man/man1
			cp libluajit*.lib $PREFIX/lib/$CONF_TARGET
			cp luajit-${DEFOLD_ARCH}.exe $PREFIX/bin/$CONF_TARGET
			cp luajit.h lauxlib.h lua.h lua.hpp luaconf.h lualib.h $PREFIX/include/luajit-2.0
			cp lj.supp $PREFIX/share/luajit
			cp -r jit $PREFIX/share/luajit
			cp ../etc/luajit.1 $PREFIX/share/man/man1

			rm -rf tmp/build

			export DEFOLD_ARCH="64"
			cmd "/C msvcbuild.bat static gc64 ${CONF_TARGET} "
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/include/luajit-2.0
			mkdir -p $PREFIX/share
			mkdir -p $PREFIX/share/lua/5.1
			mkdir -p $PREFIX/share/luajit/jit
			mkdir -p $PREFIX/share/man/man1
			cp libluajit*.lib $PREFIX/lib/$CONF_TARGET
			cp luajit-${DEFOLD_ARCH}.exe $PREFIX/bin/$CONF_TARGET
			cp luajit.h lauxlib.h lua.h lua.hpp luaconf.h lualib.h $PREFIX/include/luajit-2.0
			cp lj.supp $PREFIX/share/luajit
			cp -r jit $PREFIX/share/luajit
			cp ../etc/luajit.1 $PREFIX/share/man/man1
		}
		;;
esac

download
cmi $1
