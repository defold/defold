#!/usr/bin/env bash
# Compilation guide:
# https://www.glfw.org/docs/latest/compile.html

readonly VERSION=1.611.0
readonly BASE_URL=https://github.com/microsoft/DirectX-Headers/archive/refs/tags/
readonly FILE_URL=v${VERSION}.zip
readonly PRODUCT=directx-headers

. ../common.sh

function cmi_unpack() {
    echo "Unpacking $SCRIPTDIR/download/$FILE_URL"
    unzip $SCRIPTDIR/download/$FILE_URL
}

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}
DIRECTX_BASE_DIR=DirectX-Headers-${VERSION}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

if [[ $PLATFORM != "x86_64-win32" ]]; then
   echo "Only x86_64-win32 is supported!"
   exit 1
fi

download

mkdir -p ${SOURCE_DIR}

pushd $SOURCE_DIR

cmi_unpack

pushd $DIRECTX_BASE_DIR

PACKAGE=directx-headers-${VERSION}-${PLATFORM}.tar.gz

tar cfvz $PACKAGE include/directx

popd
popd

## FINALIZE
mv $SOURCE_DIR/$DIRECTX_BASE_DIR/$PACKAGE ../build

rm -rf $SOURCE_DIR
