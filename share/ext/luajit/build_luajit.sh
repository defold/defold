#!/bin/bash

readonly BASE_URL=http://luajit.org/download/
readonly FILE_URL=LuaJIT-2.1.0-beta3.tar.gz
readonly PRODUCT=luajit
readonly VERSION=2.1.0-beta3

# Whether to skip including /bin/ in the package, since it's only needed for desktop platforms
TAR_SKIP_BIN=0

function luajit_configure() {
	echo "************** begin luajit_configure"
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
			TAR_SKIP_BIN=1
			XFLAGS+="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
			export HOST_CC="clang -m32"
			# export HOST_CC="clang -arch i386"
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
		armv7-android)
			TAR_SKIP_BIN=1
			XFLAGS="-DLUAJIT_NUMMODE=2 -DLUAJIT_ENABLE_JIT"
			CROSS="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/toolchains/arm-linux-androideabi-${ANDROID_GCC_VERSION}/prebuilt/${platform}-x86_64/bin/arm-linux-androideabi-"
			export HOST_CC="gcc -m32"
			export HOST_CFLAGS="$XFLAGS -m32 -I."
			export HOST_ALDFLAGS="-m32"
			;;
		arm64-android)
			TAR_SKIP_BIN=1
			XFLAGS="-DLUAJIT_ENABLE_GC64 -DLUAJIT_NUMMODE=2 -DLUAJIT_ENABLE_JIT"
			# XCFLAGS+="-DLUAJIT_ENABLE_GC64 -DLUAJIT_NUMMODE=2 -DLUAJIT_ENABLE_JIT"
			CROSS="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/toolchains/aarch64-linux-android-${ANDROID_64_GCC_VERSION}/prebuilt/${platform}-x86_64/bin/aarch64-linux-android-"
			export HOST_CC="gcc -m64"
			export HOST_CFLAGS="$XFLAGS -m64 -I."
			export HOST_ALDFLAGS="-m64"
			export TARGET_FLAGS="$CFLAGS"
			;;
		x86_64-linux)
			XFLAGS="-DLUAJIT_ENABLE_GC64 -DLUAJIT_NUMMODE=2 -DLUAJIT_ENABLE_JIT"
			return
			# export HOST_CC="gcc -m64"
			# export HOST_CFLAGS="$XFLAGS -m64 -I."
			# export HOST_ALDFLAGS="-m64"
			# export TARGET_FLAGS="$CFLAGS"
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
	# XFLAGS="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
	# XFLAGS="-DLUAJIT_NUMMODE=2"
	# XFLAGS=


	# These will be used for the cross compiling
	export TARGET_TCFLAGS="$CFLAGS $XFLAGS"
	export TARGET_CFLAGS="$CFLAGS $XFLAGS"
	# export TARGET_LDFLAGS="$CFLAGS"
	# export TARGET_AR="$AR rcus"
	# export TARGET_AR="ar"
	export TARGET_LD="$CC $CFLAGS"

	# These are used for host compiling
	# export HOST_CC="gcc -m64"
	export HOST_LD=true
	# export HOST_CFLAGS="$XFLAGS -m64 -I."
	export HOST_XCFLAGS=""
	# export HOST_ALDFLAGS="-m64"
	export HOST_LDFLAGS="${BUILD_LDFLAGS}"

	# make CC="gcc" HOST_CC="gcc -m64" CROSS=$NDKP TARGET_FLAGS="$NDKF $NDKARCH" TARGET_SYS=Android clean

	# Disable
	export TARGET_STRIP=true
	export CCOPTIONS=

	echo "************** end luajit_configure"
}

# Use above function instead of shell scripts
CONFIGURE_WRAPPER="luajit_configure"
export -f luajit_configure

. ../common.sh

export CONF_TARGET=$1

case $1 in
	armv7-darwin)
		echo "############# armv7-darwin"
		# export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
		# export XCFLAGS+="-DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_JIT"
		# export LUAJIT_TARGET="LUAJIT_ARCH_ARM"
		# export TARGET_LJARCH="arm"
		export TARGET_SYS=iOS
		# function cmi_make() {
		# 	# export HOST_CC+=" -m32"
		# 	# export HOST_CC="clang -arch i386"
		# 	# export CROSS=armeabi-v7a
		# 	# export TARGET_CFLAGS+=" -arch armv7 -m32"
		# 	# export LUAJIT_TARGET="LJ_TARGET_ARM"
		# 	echo "XCFLAGS: ${XCFLAGS}"
		# 	echo "###### armv7-darwin cmi_make!"
		# 	echo "HOST_CC: ${HOST_CC}"
		# 	echo "CROSS: ${CROSS}"
		# 	which "${HOST_CC}"
		# 	pwd
		#     set -e
		#     make -f $MAKEFILE -j8
		#     make install
		#     set +e
		# }
		;;
	arm64-darwin)
		# export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
		export TARGET_SYS=iOS
		# function cmi_make() {
		# 	echo "###### arm64-darwin cmi_make!"
		# 	echo "HOST_CC: ${HOST_CC}"
		# 	echo "CROSS: ${CROSS}"
		# 	# which "${HOST_CC}"
		# 	# pwd
		#     set -e
		#     make -f $MAKEFILE -j8
		#     make install
		#     set +e
		# }
		;;
	armv7-android)
		export TARGET_SYS=Linux
		# XFLAGS+="-DLUAJIT_DISABLE_JIT"
		# function cmi_make() {
		# 	# export HOST_CC="gcc -m32"
		# 	# export HOST_CFLAGS="$XFLAGS -m32 -I."
		# 	# export HOST_ALDFLAGS="-m32"
		# 	# # export TARGET_CC="-arch armv7"
		# 	echo "###### armv7-android cmi_make!"
		# 	echo "HOST_CC: ${HOST_CC}"
		# 	echo "XFLAGS: ${XFLAGS}"
		# 	echo "CROSS: ${CROSS}"
		# 	# which "${HOST_CC}"
		# 	# pwd
		#     set -e
		#     make clean
		#     make -f $MAKEFILE -j8
		#     make install
		#     set +e
		# }
		;;
	arm64-android)
		echo "############# arm64-android"
		# export HOST_CC="gcc -m64"
		# export CROSS=""
		# export CXXFLAGS="-DLJ_ABI_SOFTFP=0 -DLJ_ARCH_HASFPU=1 -DLUAJIT_ENABLE_GC64=1"
		export TARGET_SYS=Android
		# function cmi_make() {
		# 	# XFLAGS="-DLUAJIT_NUMMODE=2 -DLUAJIT_ENABLE_JIT"
		# 	# export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
		# 	echo "###### arm64-android cmi_make!"
		# 	echo "HOST_CC: ${HOST_CC}"
		# 	echo "XFLAGS: ${XFLAGS}"
		# 	echo "XCFLAGS: ${XCFLAGS}"
		# 	echo "CROSS: ${CROSS}"
		#     set -e
		#     make clean
		#     echo "#### MAKE!"
		#     make -f $MAKEFILE -j8
		#     echo "#### INSTALL!"
		#     make install
		#     set +e
		# }
		;;
	x86_64-linux)
		export TARGET_SYS=Linux
		function cmi_make() {
					TAR_SKIP_BIN=0
					echo "Building x86_64-linux with LUAJIT_ENABLE_GC64=0"
					export DEFOLD_ARCH="32"
					# CROSS=
					# export TARGET_AR="$AR rcus"
					echo "TARGET_SYS: ${TARGET_SYS}"
					echo "TARGET_AR: ${TARGET_AR}"
					echo "HOST_CC: ${HOST_CC}"
					echo "XFLAGS: ${XFLAGS}"
					echo "CROSS: ${CROSS}"
					echo "DEFOLD_ARCH: ${DEFOLD_ARCH}"
                    set -e
                    make -j8
                    make install
                    make clean
                    set +e
                    echo "Building x86_64-linux with LUAJIT_ENABLE_GC64=1"
                    export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
                    export DEFOLD_ARCH="64"
					echo "XFLAGS: ${XFLAGS}"
					echo "XCFLAGS: ${XCFLAGS}"
					echo "DEFOLD_ARCH: ${DEFOLD_ARCH}"
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
					export DEFOLD_ARCH="32"
					echo "HOST_CC: ${HOST_CC}"
					# echo "CC: ${CC}"
					echo "XFLAGS: ${XFLAGS}"
					echo "CROSS: ${CROSS}"
					echo "DEFOLD_ARCH: ${DEFOLD_ARCH}"
                    set -e
                    make -j8
                    make install
                    make clean
                    set +e
                    echo "Building x86_64-darwin with LUAJIT_ENABLE_GC64=1"
                    export XCFLAGS+="-DLUAJIT_ENABLE_GC64"
                    export DEFOLD_ARCH="64"
                    # echo "CC: ${CC}"
					echo "XFLAGS: ${XFLAGS}"
					echo "XCFLAGS: ${XCFLAGS}"
					echo "DEFOLD_ARCH: ${DEFOLD_ARCH}"
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
