#!/usr/bin/env bash

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
    mkdir -p $PREFIX/bin
    cp $PRODUCT $PREFIX/bin
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
        darwin)
            export CPPFLAGS="-m32"
            export CXXFLAGS="${CXXFLAGS} -m32"
            export CFLAGS="${CFLAGS} -m32"
            export LDFLAGS="-m32"
            cmi_buildplatform $PLATFORM
            ;;

        x86_64-darwin)
            cmi_buildplatform $1
            ;;

        linux)
            export CPPFLAGS="-m32"
            export CXXFLAGS="${CXXFLAGS} -m32"
            export CFLAGS="${CFLAGS} -m32"
            export LDFLAGS="-m32"
            cmi_buildplatform $PLATFORM
            ;;

        x86_64-linux)
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
                mkdir -p $PREFIX/bin/$CONF_TARGET
                cp Release/$PRODUCT.exe $PREFIX/bin/$CONF_TARGET
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
