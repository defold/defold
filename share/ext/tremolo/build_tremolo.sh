#!/bin/bash

readonly BASE_URL=http://wss.co.uk/pinknoise/tremolo
readonly FILE_URL=Tremolo008.zip
readonly PRODUCT=tremolo
readonly VERSION=0.0.8

. ../common.sh

function cmi_unpack() {
    unzip ../../download/$FILE_URL
}

function tremolo_configure() {
    export MAKEFLAGS="-e"
}

export -f tremolo_configure

export CONF_TARGET=$1

MODE="c-only"

case $1 in
        armv7-android)
        	MODE="arm-asm"
        	;;
esac

case $MODE in
	arm-asm)
            function cmi_make() {
                export CFLAGS="${CFLAGS} -Wa,-mimplicit-it=always -D_ARM_ASSEM_ -static -O2 -mtune=xscale"
                set -e
                make -j1 libTremolo006.lib
                set +e
                mkdir -p $PREFIX/lib/$CONF_TARGET
                cp libTremolo006.lib $PREFIX/lib/$CONF_TARGET/libtremolo.a
            }
            ;;
            
       c-only)
            function cmi_make() {
            	export CFLAGS="${CFLAGS} -O2"
            	set -e
            	make libTremolo006-c.lib
            	set +e
                mkdir -p $PREFIX/lib/$CONF_TARGET
                mkdir -p $PREFIX/include/tremolo
                mkdir -p $PREFIX/bin/$CONF_TARGET
                mkdir -p $PREFIX/share/$CONF_TARGET
                cp *.h $PREFIX/include/tremolo
                cp libTremolo006-c.lib $PREFIX/lib/$CONF_TARGET/libtremolo.a
            }            
            ;;
            
       *)
           ;;
esac

export CONFIGURE_WRAPPER="tremolo_configure"

download
cmi $1
