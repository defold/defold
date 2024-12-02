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

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

CMAKE_FLAGS="-DASTCENC_CLI=OFF ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
esac

# Follow the build instructions on https://github.com/ARM-software/astc-encoder
if [ ! -d "$SOURCE_DIR" ]; then
    git clone https://github.com/ARM-software/astc-encoder.git $SOURCE_DIR
fi

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake ${CMAKE_FLAGS} $SOURCE_DIR
cmake --build . --config Release -j 8

LIB_NAME=

case $PLATFORM in
    arm64-macos)
        LIB_NAME=libastcenc-neon-static.a
        ;;
    x86_64-macos)
        LIB_NAME=TODO
        ;;
    x86_64-linux)
        LIB_NAME=TODO
        ;;
    x86_64-win32)
        LIB_NAME=TODO
        ;;
    *)
        ;;
esac

#mkdir -p ./bin/$PLATFORM
mkdir -p ./lib/$PLATFORM
mkdir -p ./include/$PLATFORM
mkdir -p ./include/$PLATFORM/astcenc

cp -v Source/${LIB_NAME} ./lib/$PLATFORM/${LIB_NAME}

cp -v ../../source/Source/astcenc.h ./include/$PLATFORM/astcenc/astcenc.h

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=astcenc-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE lib include
echo "Wrote ${BUILD_DIR}/${PACKAGE}"
popd

echo "Done!"
