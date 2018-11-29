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

download
cmi $1
