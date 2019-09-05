#!/usr/bin/env bash

readonly PRODUCT=jctest
readonly VERSION=0.4
readonly BASE_URL=https://github.com/JCash/jctest/archive/
readonly FILE_URL=v${VERSION}.tar.gz

export CONF_TARGET=$1

. ../common.sh

function cmi_configure() {
    echo "skipping cmi_configure"
}

function cmi_make() {
    echo "cmi_make"
    local include=${PREFIX}/include/

    mkdir ${PREFIX}/include/
    mkdir ${PREFIX}/include/jc_test
    mkdir ${PREFIX}/share
    mkdir ${PREFIX}/bin
    mkdir ${PREFIX}/lib

    pwd
    cp -v src/jc_test.h ${PREFIX}/include/jc_test
}

function cmi_package_platform() {
    echo "skipping cmi_package_platform"
}

download
cmi $1
