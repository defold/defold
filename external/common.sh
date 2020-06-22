#!/usr/bin/env bash

# for win32/msys, try "wget --no-check-certificate -O $FILE_URL"
CURL="curl -L -O"

TAR_SKIP_BIN=0
TAR_INCLUDES=0

set -e

function cmi_download() {
    echo cmi_download
    if [ ! -f ../download/$FILE_URL ]; then
        mkdir -p ../download
        $CURL $BASE_URL/$FILE_URL && mv $FILE_URL ../download
    fi
}

function cmi_unpack() {
    echo cmi_unpack
    tar xfz ../../download/$FILE_URL --strip-components=1
}

function cmi_patch() {
    echo cmi_patch
    if [ -f ../patch_$VERSION ]; then
        echo "Applying patch ../patch_$VERSION" && patch --binary -p1 < ../patch_$VERSION
    fi
}


function cmi_install() {
    echo cmi_install...
    cmi_download
    mkdir -p package
    pushd package
    cmi_unpack
    cmi_patch
    popd
}

function cmi_package_common() {
    local TGZ_COMMON="$PRODUCT-$VERSION-common.tar.gz"
    pushd $PREFIX  >/dev/null
    tar cfvz $TGZ_COMMON include share
    popd >/dev/null

    [ ! -d ../build ] && mkdir ../build
    mv -v $PREFIX/$TGZ_COMMON ../build
    echo "../build/$TGZ_COMMON created"
}

function cmi_package_platform() {
    local TGZ="$PRODUCT-$VERSION-$1.tar.gz"
    pushd $PREFIX  >/dev/null
    if [ ${TAR_SKIP_BIN} -eq "1" ]; then
        tar cfvz $TGZ lib
    elif [ ${TAR_INCLUDES} -eq "1" ]; then
        tar cfvz $TGZ lib bin include
    else
        tar cfvz $TGZ lib bin
    fi
    popd >/dev/null

    [ ! -d ../build ] && mkdir ../build
    mv -v $PREFIX/$TGZ ../build
    echo "../build/$TGZ created"
}
