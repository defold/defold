#!/usr/bin/env bash
# Copyright 2020-2022 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
# 
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
# 
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.



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
EXTRA_FLAGS=""

case $1 in
    armv7-android)
    	MODE="arm-asm"
    	;;

    arm64-android)
        EXTRA_FLAGS="-static"
        ;;

    x86_64-macos)
        EXTRA_FLAGS="-mmacosx-version-min=10.7"
        ;;

    arm64-ios)
        EXTRA_FLAGS="-miphoneos-version-min=6.0"
        ;;

    x86_64-ios)
        EXTRA_FLAGS="-miphoneos-version-min=6.0"
        ;;
esac

case $MODE in
    arm-asm)
        function cmi_make() {
            export CFLAGS="${CFLAGS} -mimplicit-it=always -D_ARM_ASSEM_ -static -O2 -mtune=xscale"
            set -e
            make -j1 libTremolo006.lib
            set +e
            mkdir -p $PREFIX/lib/$CONF_TARGET
            cp libTremolo006.lib $PREFIX/lib/$CONF_TARGET/libtremolo.a
        }
        ;;

   c-only)
        function cmi_make() {
        	export CFLAGS="${CFLAGS} -O2 $EXTRA_FLAGS"
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
