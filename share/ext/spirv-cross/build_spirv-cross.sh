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
    cmi_do $1 ""

    local TGZ="$PRODUCT-$VERSION-$1.tar.gz"

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

export PREFIX=`pwd`/build

function cmi() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    case $1 in
        darwin)
            export CPPFLAGS="-m32"
            export CXXFLAGS="${CXXFLAGS} -m32"
            export CFLAGS="${CFLAGS} -m32"
            export LDFLAGS="-m32"
            cmi_buildplatform $1
            ;;

        x86_64-darwin)
            cmi_buildplatform $1
            ;;

        linux)
            export CPPFLAGS="-m32"
            export CXXFLAGS="${CXXFLAGS} -m32"
            export CFLAGS="${CFLAGS} -m32"
            export LDFLAGS="-m32"
            cmi_buildplatform $1
            ;;

        x86_64-linux)
            cmi_buildplatform $1
            ;;

    	win32|x86_64-win32)
            if [ "$1" == "win32" ]; then
                CMAKE_ARCH=""
            else
                CMAKE_ARCH=" Win64"
            fi
            function cmi_make() {
                set -e

                mkdir -p build >/dev/null
                pushd build >/dev/null
                cmake -G"Visual Studio 14 2015${CMAKE_ARCH}" ..
                cmake --build . --config Release
                mkdir -p $PREFIX/bin
                cp Release/$PRODUCT.exe $PREFIX/bin
                popd >/dev/null

                set +e
            }
            cmi_setup_vs2015_env $1
            cmi_buildplatform $1
            ;;
        *)
            echo "Unknown target $1" && exit 1
            ;;
    esac
}

download
cmi $1
