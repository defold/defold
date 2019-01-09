#!/usr/bin/env bash

# https://www.webmproject.org/code/
# Steps to update archive:
# 1: Clone the repo:  		$ git clone https://chromium.googlesource.com/webm/libvpx
# 2: Checkout the tag: 		$ (cd libvpx && git checkout v1.7.0)
# 3: Remove the git folder:	$ rm -rf libvpx/.git
# 4: Archive the folder: 	$ tar -czvf $DEFOLD_HOME/share/ext/vpx/libvpx-1.7.0.tar.gz libvpx/

readonly PRODUCT=vpx
readonly VERSION=1.7.0
readonly BASE_URL=https://chromium.googlesource.com/webm/libvpx/+archive
readonly FILE_URL=libvpx-${VERSION}.tar.gz

export CONF_TARGET=$1

. ../common.sh

# Also see "./configure --help" for a full list of options
export CONFIGURE_ARGS="--disable-examples --disable-unit-tests --disable-docs --disable-tools --enable-pic --enable-vp8 --disable-vp9 --disable-vp8-decoder"

function download() {
	cp $FILE_URL ../download
}

function cmi_configure() {
    ${CONFIGURE_WRAPPER} ./configure $CONFIGURE_ARGS $2 \
        --disable-shared \
        --prefix=${PREFIX}
}


old_cmi_package_platform=
save_function cmi_package_platform old_cmi_package_platform

function cmi_package_platform() {
    local libdir=${PREFIX}/lib/$1
	pushd $PREFIX  >/dev/null

		case $1 in
			win32)
				# it has wrong casing: Win32
				mv lib/Win32 lib/tmp
				mv lib/tmp ${libdir}
				;;
			x86_64-win32)
				mv lib/x64 ${libdir}
				;;
			*)
				mkdir ${libdir}
				;;
		esac

		mkdir bin
		mkdir share

		case $1 in
		*win32)
			mv ${libdir}/vpxmd.lib ${libdir}/vpx.lib
			;;
		*)
			mv lib/*.a ${libdir}/libvpx.a
			;;
		esac

		[ -e lib/pkgconfig ] && rm -rf lib/pkgconfig
    popd >/dev/null

    # now call the old packager
	old_cmi_package_platform $1
}

case $CONF_TARGET in
	darwin)
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --target=generic-gnu"
		;;
	x86_64-darwin)
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --target=x86_64-darwin15-gcc"
		;;
	x86_64-linux)
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --target=x86_64-linux-gcc"
		;;
	linux)
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --target=x86-linux-gcc"
		;;
	win32)
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --target=x86-win32-vs14"
		PATH="/c/Program Files (x86)/MSBuild/14.0/Bin:$PATH"
		;;
	x86_64-win32)
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --target=x86_64-win64-vs14"
		PATH="/c/Program Files (x86)/MSBuild/14.0/Bin:$PATH"
		;;
	js-web)
		CONFIGURE_WRAPPER="ARFLAGS=crs $EMSCRIPTEN/emconfigure"
		;;
esac

download
cmi $1
