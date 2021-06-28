#!/usr/bin/env bash

readonly SHA1=3f9389edc6cdf3f78a6896d550c236860aed62b2
readonly BASE_URL=https://github.com/LuaJIT/LuaJIT/archive/
readonly FILE_URL=${SHA1}.zip
readonly PRODUCT=luajit
readonly VERSION=2.1.0-beta3

# Whether to skip including /bin/ in the package, since it's only needed for desktop platforms
TAR_SKIP_BIN=0

function luajit_configure() {
	export MAKEFLAGS="-e -j8"
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

	COMMON_FLAGS_32="-DLUAJIT_DISABLE_JIT -DLUAJIT_NUMMODE=LJ_NUMMODE_DUAL -DLUAJIT_DISABLE_GC64 "
	COMMON_FLAGS_64="-DLUAJIT_DISABLE_JIT -DLUAJIT_NUMMODE=LJ_NUMMODE_DUAL "

	case $CONF_TARGET in
		armv7-darwin)
			TAR_SKIP_BIN=1
			XFLAGS="$COMMON_FLAGS_32"
			export CROSS=""
			export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
			export HOST_CC="clang"
			export HOST_CFLAGS="$XFLAGS -m32 -isysroot $OSX_SDK_ROOT -I."
			export HOST_ALDFLAGS="-m32"
			export TARGET_FLAGS="$CFLAGS"
			export XCFLAGS="-DLUAJIT_TARGET=LUAJIT_ARCH_ARM"
            export SKIPLUAJIT=true
			;;
		arm64-darwin)
			TAR_SKIP_BIN=1
			XFLAGS="$COMMON_FLAGS_64"
			export CROSS=""
			export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
			export HOST_CC="clang"
			export HOST_CFLAGS="$XFLAGS -m64 -isysroot $OSX_SDK_ROOT -I."
			export HOST_ALDFLAGS="-m64"
			export TARGET_FLAGS="$CFLAGS"
			export XCFLAGS="-DLUAJIT_TARGET=LUAJIT_ARCH_ARM64"
			;;
		x86_64-ios)
			TAR_SKIP_BIN=1
			XFLAGS="$COMMON_FLAGS_64"
			export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
			export HOST_CC="clang"
			export HOST_CFLAGS="$XFLAGS -m64 -isysroot $OSX_SDK_ROOT -I."
			export HOST_ALDFLAGS="-m64"
			export TARGET_FLAGS="$CFLAGS"
			;;
		armv7-android)
			TAR_SKIP_BIN=1
			XFLAGS="$COMMON_FLAGS_32"
			export CROSS=""
			export HOST_CC="clang"
			export HOST_CFLAGS="$XFLAGS -m32 -I."
			export HOST_ALDFLAGS="-m32"
			export XCFLAGS="-DLUAJIT_TARGET=LUAJIT_ARCH_ARM"
			;;
		arm64-android)
			TAR_SKIP_BIN=1
			XFLAGS="$COMMON_FLAGS_64"
			export CROSS=""
			export HOST_CC="clang"
			export HOST_CFLAGS="$XFLAGS -m64 -I."
			export HOST_ALDFLAGS="-m64"
			export TARGET_FLAGS="$CFLAGS"
			export XCFLAGS="-DLUAJIT_TARGET=LUAJIT_ARCH_ARM64"
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


function cmi_unpack() {
    echo cmi_unpack
    # simulate the "--strip-components=1" of the tar unpack command
    local dest=$(pwd)
    local temp=$(mktemp -d)
    unzip -q -d "$temp" $SCRIPTDIR/download/$FILE_URL
	mv "$temp"/*/* "$dest"
	rm -rf "$temp"
}

function cmi_patch() {
    echo cmi_patch
    if [ -f ../patch_$VERSION ]; then
        echo "Applying patch ../patch_$VERSION" && patch -l --binary -p1 < ../patch_$VERSION
    fi
}


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
					export DEFOLD_ARCH="32"
					export XCFLAGS="-DLUAJIT_DISABLE_GC64"
					echo "Building $CONF_TARGET ($DEFOLD_ARCH) with '$XCFLAGS'"
					set -e
					make -j8
					make install
					make clean
					set +e

					export DEFOLD_ARCH="64"
					export XCFLAGS=""
					echo "Building $CONF_TARGET ($DEFOLD_ARCH) with '$XCFLAGS'"
					set -e
					make -j8
					make install
					set +e
		}
		;;
	x86_64-darwin)
		export TARGET_SYS=Darwin
		function cmi_make() {
					# Note: Luajit sets this to 10.4, which is less than what we support.
					#       This value is set to the same as MIN_OSX_SDK_VERSION in waf_dynamo.py
					export MACOSX_DEPLOYMENT_TARGET="10.7"

					# Since GC32 mode isn't supported on macOS, in the new version.
					# We'll just use the old built executable from the previous package

					# mkdir foo
					# cd foo
					# tar xvf ../../../../packages/luajit-2.1.0-beta3-x86_64-darwin.tar.gz
					# tar xvf ../../build/luajit-2.1.0-beta3-x86_64-darwin.tar.gz
					# tar zcvf ../luajit-2.1.0-beta3-x86_64-darwin.tar.gz .

					# export DEFOLD_ARCH="32"
					# export TARGET_CFLAGS="-DLUAJIT_DISABLE_GC64"
					# echo "Building $CONF_TARGET ($DEFOLD_ARCH) with '$TARGET_CFLAGS'"
					# set -e
					# make -j8
					# make install
					# make clean
					# set +e

					export DEFOLD_ARCH="64"
					export TARGET_CFLAGS=""
					echo "Building $CONF_TARGET ($DEFOLD_ARCH) with '$TARGET_CFLAGS'"
					set -e
					make -j8
					make install
					set +e
		}
		;;
	win32)
		export TARGET_SYS=Windows
		cmi_setup_vs2019_env $1

		function cmi_make() {
			cd src
			export DEFOLD_ARCH="32"
			cmd "/C echo PATH=%Path% "

			cmd "/C msvcbuild.bat nogc64 static dummy ${CONF_TARGET} "
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/include/luajit-2.0
			mkdir -p $PREFIX/share
			mkdir -p $PREFIX/share/lua/5.1
			mkdir -p $PREFIX/share/luajit/jit
			mkdir -p $PREFIX/share/man/man1
			cp libluajit*.lib $PREFIX/lib/$CONF_TARGET
			cp luajit.h lauxlib.h lua.h lua.hpp luaconf.h lualib.h $PREFIX/include/luajit-2.0
			cp -r jit $PREFIX/share/luajit
		}
		;;
	x86_64-win32)
		export TARGET_SYS=Windows
		cmi_setup_vs2019_env $1

		function cmi_make() {
			cd src
			export DEFOLD_ARCH="32"
			cmd "/C echo PATH=%Path% "

			cmd "/C msvcbuild.bat nogc64 static dummy ${CONF_TARGET} "
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
			cp -r jit $PREFIX/share/luajit
			cp ../etc/luajit.1 $PREFIX/share/man/man1
		}
		;;
esac

download
cmi $1
