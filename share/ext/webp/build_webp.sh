#!/usr/bin/env bash

readonly PRODUCT=webp
readonly VERSION=0.5.0
readonly BASE_URL=https://github.com/webmproject/libwebp/archive/
readonly FILE_URL=v${VERSION}.tar.gz

export CONF_TARGET=$1

. ../common.sh
old_configure=cmi_configure

# Trick to override function
save_function() {
    local ORIG_FUNC=$(declare -f $1)
    local NEWNAME_FUNC="$2${ORIG_FUNC#$1}"
    eval "$NEWNAME_FUNC"
}
save_function cmi_configure old_configure

case $CONF_TARGET in
	*win32)
		echo "Windows not implemented yet"
		exit 1
		;;
	*)
		function cmi_configure() {
			set -e
		    ./autogen.sh
			set +e

			old_configure
		}

    	export CFLAGS=-fPIC

    	function cmi_make() {
			set -e
			pushd .
		    make -j8
			set +e

			# "install"
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/share/$CONF_TARGET
			mkdir -p $PREFIX/include/

			cp -v ./src/.libs/libwebp.a $PREFIX/lib/$CONF_TARGET/
			cp -v ./src/webp/*.h $PREFIX/include

			popd
			set +e
		}
		;;
esac

download
cmi $1