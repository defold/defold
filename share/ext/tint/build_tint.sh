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


# Compilation guide:
# https://dawn.googlesource.com/tint/+/refs/heads/main/README.md
#
PRODUCT=tint
PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}
SHA1=7bd151a780126e54de1ca00e9c1ab73dedf96e59

if [ "" == "${DEFOLD_HOME}" ]; then
    echo "You must run this under a Defold shell"
    echo "Run: ./scripts/build.py shell"
    exit 1
fi

. ../common.sh

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

set -e

eval $(python ${DEFOLD_HOME}/build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

CMAKE_FLAGS="-DTINT_BUILD_DOCS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DTINT_BUILD_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DTINT_BUILD_SPV_READER=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DTINT_ENABLE_INSTALL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DTINT_BUILD_MSL_WRITER=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DBUILD_SHARED_LIBS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DDAWN_FETCH_DEPENDENCIES=ON ${CMAKE_FLAGS}"

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
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-linux)
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-win32)

        ;;
esac

# Clone the repo
if [ ! -d "$SOURCE_DIR" ]; then
	git clone https://dawn.googlesource.com/dawn $SOURCE_DIR
	(cd $SOURCE_DIR && git reset --hard $SHA1)
fi

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake ${CMAKE_FLAGS} $SOURCE_DIR
## cmd targets are executables
cmake --build . --target tint_cmd_tint_cmd -j 8

mkdir -p ./bin/$PLATFORM

EXE_SUFFIX=
case $PLATFORM in
    win32|x86_64-win32)
        EXE_SUFFIX=.exe
        cp -v ./tint${EXE_SUFFIX} ./bin/$PLATFORM
        ;;
    *)
        cp -v ./tint${EXE_SUFFIX} ./bin/$PLATFORM
        ;;
esac

case $PLATFORM in
    win32|x86_64-win32)
        ;;
    *)
        if [ "" != "$(which strip)" ]; then
            # The strip tool may not work on the target architecture
            set +e
            strip ./bin/$PLATFORM/tint${EXE_SUFFIX}
            set -e
        fi
        ;;
esac

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz

pushd ${BUILD_DIR}
tar cfvz ${PACKAGE} bin
popd

echo "Wrote ${PACKAGE}"

cp -v ${BUILD_DIR}/${PACKAGE} ${DEFOLD_HOME}/packages
