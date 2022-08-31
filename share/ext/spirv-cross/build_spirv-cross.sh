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



readonly PRODUCT=spirv-cross
readonly VERSION=2018-08-07
readonly FILE_URL=${VERSION}.tar.gz
readonly BASE_URL=https://github.com/KhronosGroup/SPIRV-Cross/archive/

. ../common.sh

function cmi_configure() {
	echo "No configure exists"
}

function cmi_make() {
    set -e
    make -f $MAKEFILE -j8
    mkdir -p $PREFIX/bin/$PLATFORM
    cp $PRODUCT $PREFIX/bin/$PLATFORM
    cmi_strip
    set +e
}

function cmi_buildplatform() {
    cmi_do $PLATFORM ""

    local TGZ="$PRODUCT-$VERSION-$PLATFORM.tar.gz"

    pushd $PREFIX  >/dev/null
    tar cfvz $TGZ bin

    popd >/dev/null
    popd >/dev/null

    mkdir ../build

    mv -v $PREFIX/$TGZ ../build
    echo "../build/$TGZ created"

    rm -rf tmp
    rm -rf $PREFIX
}

function cmi() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    case $PLATFORM in
        x86_64-macos)
            function cmi_strip() {
                strip -S -x $PREFIX/bin/$PLATFORM/$PRODUCT
            }
            cmi_buildplatform $1
            ;;

        x86_64-linux)
            function cmi_strip() {
                strip -s $PREFIX/bin/$PLATFORM/$PRODUCT
            }
            cmi_buildplatform $PLATFORM
            ;;

    	win32|x86_64-win32)
            function cmi_configure() {
                case $PLATFORM in
                    win32)
                        CMAKE_GENERATOR="Visual Studio 14 2015"
                        ;;
                    x86_64-win32)
                        CMAKE_GENERATOR="Visual Studio 14 2015 Win64"
                        ;;
                esac

                set -e
                mkdir -p build >/dev/null
                pushd build >/dev/null
                cmake -G"${CMAKE_GENERATOR}" ..
                popd >/dev/null
            }

            function cmi_make() {
                set -e

                pushd build >/dev/null
                cmake --build . --config Release
                mkdir -p $PREFIX/bin/$PLATFORM
                cp Release/$PRODUCT.exe $PREFIX/bin/$PLATFORM
                popd >/dev/null

                set +e
            }
            cmi_setup_vs2015_env $PLATFORM
            cmi_buildplatform $PLATFORM
            ;;
        *)
            echo "Unknown target $PLATFORM" && exit 1
            ;;
    esac
}

download
cmi $1
