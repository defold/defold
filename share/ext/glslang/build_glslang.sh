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
# Copyright 2022 The Defold Foundation

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSHADERC_SKIP_TESTS=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSHADERC_SKIP_EXAMPLES=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSHADERC_SKIP_COPYRIGHT_CHECK=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSHADERC_SKIP_INSTALL=ON ${CMAKE_FLAGS}"
# static link on MSVC
CMAKE_FLAGS="-DSHADERC_ENABLE_SHARED_CRT=OFF ${CMAKE_FLAGS}"
# Size opt
CMAKE_FLAGS="-DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=ON ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        ;;
esac

# Follow the build instructions on https://github.com/google/shaderc
if [ ! -d "$SOURCE_DIR" ]; then
    git clone https://github.com/KhronosGroup/glslang.git $SOURCE_DIR

    pushd $SOURCE_DIR
    python update_glslang_sources.py
    popd
fi

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake ${CMAKE_FLAGS} $SOURCE_DIR
cmake --build . --config Release -j 8

mkdir -p ./bin/$PLATFORM

EXE_SUFFIX=
case $PLATFORM in
    win32|x86_64-win32)
        EXE_SUFFIX=.exe
        cp -v ./StandAlone/Release/glslang${EXE_SUFFIX} ./bin/$PLATFORM
        ;;
    *)
        cp -v ./StandAlone/glslang${EXE_SUFFIX} ./bin/$PLATFORM
        ;;
esac

case $PLATFORM in
    win32|x86_64-win32)
        ;;
    *)
        strip ./bin/$PLATFORM/glslang${EXE_SUFFIX}
        ;;
esac

popd

# Package
VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=glslang-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz ${PACKAGE} bin
popd

echo "Wrote ${PACKAGE}"

