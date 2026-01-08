#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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
# Copyright 2023 The Defold Foundation

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

# eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
# OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN
# Until we've updated our main version, we need to use at least 10.15 due to features used in the tools
OSX_MIN_SDK_VERSION=10.15

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_SKIP_TESTS=ON ${CMAKE_FLAGS}"

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

if [ ! -d "${SOURCE_DIR}" ]; then
    git clone https://github.com/KhronosGroup/SPIRV-Tools.git ${SOURCE_DIR}
fi

python ${SOURCE_DIR}/utils/git-sync-deps

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

echo "CMAKE_FLAGS: '${CMAKE_FLAGS}"
cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release

EXE_SUFFIX=
case $PLATFORM in
    win32|x86_64-win32)
        EXE_SUFFIX=.exe
        SRC_EXE_SPIRV_OPT=./tools/Release/spirv-opt${EXE_SUFFIX}
        SRC_EXE_SPIRV_LINK=./tools/Release/spirv-link${EXE_SUFFIX}
        ;;
    *)
        SRC_EXE_SPIRV_OPT=./tools/spirv-opt${EXE_SUFFIX}
        SRC_EXE_SPIRV_LINK=./tools/spirv-link${EXE_SUFFIX}
        ;;
esac

TARGET_EXE_SPIRV_OPT=./bin/$PLATFORM/spirv-opt${EXE_SUFFIX}
TARGET_EXE_SPIRV_LINK=./bin/$PLATFORM/spirv-link${EXE_SUFFIX}

mkdir -p ./bin/$PLATFORM

cp -v ${SRC_EXE_SPIRV_OPT} ${TARGET_EXE_SPIRV_OPT}
cp -v ${SRC_EXE_SPIRV_LINK} ${TARGET_EXE_SPIRV_LINK}

case $PLATFORM in
    win32|x86_64-win32)
        ;;
    *)
        strip ${TARGET_EXE_SPIRV_OPT}
        strip ${TARGET_EXE_SPIRV_LINK}
        ;;
esac

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=spirv-tools-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE bin
echo "Wrote ${BUILD_DIR}/${PACKAGE}"
popd
