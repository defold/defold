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
CMAKE_FLAGS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"

CMAKE_FLAGS="-DSPIRV_CROSS_STATIC=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_CLI=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_SHARED=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_TESTS=OFF ${CMAKE_FLAGS}"

CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_GLSL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_HLSL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_MSL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_CPP=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_REFLECT=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_C_API=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_UTIL=OFF ${CMAKE_FLAGS}"


case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-win32)
        # We might have to force MT on windows, something like this:
        # CMAKE_FLAGS="-DCMAKE_C_FLAGS_RELEASE=\"/MT /O2\" -DCMAKE_CXX_FLAGS_RELEASE=\"/MT /O2\" ${CMAKE_FLAGS}"
        ;;
esac

# Follow the build instructions on https://github.com/KhronosGroup/SPIRV-Cross.git

if [ ! -d "${SOURCE_DIR}" ]; then
    git clone https://github.com/KhronosGroup/SPIRV-Cross.git ${SOURCE_DIR}
fi

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

echo "CMAKE_FLAGS: ${CMAKE_FLAGS}"

cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release

LIB_SUFFIX=.a
LIB_PREFIX=lib

case $PLATFORM in
    win32|x86_64-win32)
        LIB_SUFFIX=.lib
        LIB_PREFIX=Release/
        ;;
    *)
        ;;
esac

mkdir -p ./lib/$PLATFORM
mkdir -p ./include/$PLATFORM
mkdir -p ./include/$PLATFORM/spirv

cp -v ../../source/spirv_cross_c.h ./include/$PLATFORM/spirv/spirv_cross_c.h
cp -v ../../source/spirv.h         ./include/$PLATFORM/spirv/spirv.h

cp -v ${LIB_PREFIX}spirv-cross-c${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-c${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-core${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-core${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-glsl${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-glsl${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-hlsl${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-hlsl${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-msl${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-msl${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-util${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-util${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-reflect${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-reflect${LIB_SUFFIX}

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=spirv-cross-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE lib include
echo "Wrote ${BUILD_DIR}/${PACKAGE}"
popd
