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

        win32)
            cmi_buildplatform $1
            ;;

        x86_64-win32)
            cmi_buildplatform $1
            ;;

        i586-mingw32msvc)
            export CPP=i586-mingw32msvc-cpp
            export CC=i586-mingw32msvc-gcc
            export CXX=i586-mingw32msvc-g++
            export AR=i586-mingw32msvc-ar
            export RANLIB=i586-mingw32msvc-ranlib
            cmi_cross $1 $1
            ;;

        *)
            echo "Unknown target $1" && exit 1
            ;;
    esac
}

download
cmi $1
