#!/usr/bin/env bash
# Copyright 2020-2023 The Defold Foundation
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

if ! [[ $PLATFORM == "x86_64-win32" || $PLATFORM == "win32" ]]; then
   echo "Only x86_64-win32 and win32 platforms are supported!"
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
