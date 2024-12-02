#!/usr/bin/env bash
# Copyright 2020-2024 The Defold Foundation
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

# License: MIT
# Copyright 2024 The Defold Foundation

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}

. ../common.sh


ASTC_EXE_NAME=
ASTC_PLATFORM=


# From the ASTC readme regarding x86_64 binaries:
#
# For x86-64 we provide, in order of increasing performance:
#
# astcenc-sse2 - uses SSE2
# astcenc-sse4.1 - uses SSE4.1 and POPCNT
# astcenc-avx2 - uses AVX2, SSE4.2, POPCNT, and F16C
# The x86-64 SSE2 builds will work on all x86-64 machines, but it is the slowest of the three. The other two require extended CPU instruction set support which is not universally available, but each step gains ~15% more performance.
case $PLATFORM in
    arm64-macos|x86_64-macos)
        ASTC_EXE_NAME=astcenc
        ASTC_PLATFORM=macos-universal
        ;;
    x86_64-win32)
        ASTC_EXE_NAME=astcenc-sse2.exe
        ASTC_PLATFORM=windows-x64
        ;;
    x86_64-linux)
        ASTC_EXE_NAME=astcenc-sse2
        ASTC_PLATFORM=linux-x64
        ;;
    *)
        echo "Platform unsupported"
        exit 1
        ;;
esac

ASTC_VERSION=5.1.0
ASTC_ZIP_NAME=astcenc-${ASTC_VERSION}-${ASTC_PLATFORM}.zip

readonly BASE_URL=https://github.com/ARM-software/astc-encoder/releases/download/${ASTC_VERSION}/
readonly FILE_URL=astcenc-${ASTC_VERSION}-${ASTC_PLATFORM}.zip

download

cmi_unpack

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

mkdir -p ./bin/$PLATFORM

case $PLATFORM in
    arm64-macos)
        lipo -thin arm64 ../../${ASTC_EXE_NAME} -o ./bin/$PLATFORM/astcenc
        ;;
    x86_64-macos)
        lipo -thin x86_64 ../../${ASTC_EXE_NAME} -o ./bin/$PLATFORM/astcenc
        ;;
    *)
        cp -v ../../${ASTC_EXE_NAME} ./bin/$PLATFORM
        ;;
esac

PACKAGE=astcenc-${ASTC_VERSION}-${PLATFORM}.tar.gz

tar cfvz ${PACKAGE} bin
popd

echo "Wrote ${PACKAGE}"
