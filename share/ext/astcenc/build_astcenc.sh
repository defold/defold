#!/usr/bin/env bash
# Copyright 2020-2025 The Defold Foundation
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
SHA1=8b0aa018558028b6475a83463e29d4b8cf27de8f
BUILD_DIR=${PWD}/build/${PLATFORM}

. ../common.sh

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

CONFIG=Release

CMAKE_FLAGS="-DASTCENC_CLI=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    arm64-linux)
        CMAKE_FLAGS="-DARCH=aarch64 -DASTCENC_ISA_NEON=ON ${CMAKE_FLAGS}"
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-linux)
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-win32)

        # NOTE! For windows we need to use the exact same version of the toolset as we use for the engine
        #       However, I could not get the cmake setup to work, so I just installed the same version via the visual studio installer.
        #       Leaving the almost-working-code here for posterity..
        #
        # eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_WINDOWS_MSVC_2022)
        # MSVC_CL_EXE=$(cygpath -u "${DYNAMO_HOME}/ext/SDKs/Win32/MicrosoftVisualStudio14.0/VC/Tools/MSVC/${VERSION_WINDOWS_MSVC_2022}/bin/Hostx64/x64/cl.exe")
        # CMAKE_FLAGS="-DCMAKE_C_COMPILER=${MSVC_CL_EXE} -DCMAKE_CXX_COMPILER=${MSVC_CL_EXE} $CMAKE_FLAGS"
        ;;
esac

# Follow the build instructions on https://github.com/ARM-software/astc-encoder
if [ ! -d "$SOURCE_DIR" ]; then
    git clone https://github.com/ARM-software/astc-encoder.git $SOURCE_DIR
    (cd $SOURCE_DIR && git reset --hard $SHA1)
fi

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake ${CMAKE_FLAGS} $SOURCE_DIR
cmake --build . --config $CONFIG -j 8

LIB_NAME=
LIB_EXT=.a

case $PLATFORM in
    arm64-macos)
        LIB_NAME=libastcenc-neon-static.a
        ;;
    x86_64-macos)
        LIB_NAME=libastcenc-sse4.1-static.a
        ;;
    arm64-linux)
        LIB_NAME=libastcenc-neon-static.a
        ;;
    x86_64-linux)
        LIB_NAME=libastcenc-native-static.a
        ;;
    x86_64-win32)
        LIB_NAME=$CONFIG/astcenc-native-static.lib
        LIB_EXT=.lib
        ;;
    *)
        ;;
esac

mkdir -p ./lib/$PLATFORM
mkdir -p ./include/$PLATFORM
mkdir -p ./include/$PLATFORM/astcenc

cp -v Source/${LIB_NAME} ./lib/$PLATFORM/libastcenc${LIB_EXT}

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
