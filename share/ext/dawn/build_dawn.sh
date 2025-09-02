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


# Compilation guide:
# https://dawn.googlesource.com/tint/+/refs/heads/main/README.md
#
PRODUCT=dawn
PLATFORM=$1
PWD=$(pwd)

SHA1=6bab1bd9fd23c03ab2b0f26e9b489e0b6b1844f2
URL=https://github.com/google/dawn/archive/${SHA1}.zip

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

SOURCE_DIR=${PWD}/dawn-${SHA1}
BUILD_DIR=${PWD}/build/${PLATFORM}
INSTALL_DIR=${PWD}/install/${PLATFORM}

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

CMAKE_FLAGS="-DDAWN_ENABLE_INSTALL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DDAWN_FETCH_DEPENDENCIES=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DDAWN_BUILD_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DTINT_BUILD_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DBUILD_SHARED_LIBS=OFF ${CMAKE_FLAGS}"

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
    win32|x86_64-win32)
        CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"
        ;;
esac

# Download the repo
if [ ! -e "${SHA1}.zip" ]; then
    wget wget ${URL}
fi

# Unpack the repo
if [ ! -d "${SOURCE_DIR}" ]; then
    unzip -q ${SHA1}.zip
fi

# Config + Build
(cd ${SOURCE_DIR} && cmake -S . -B ${BUILD_DIR} ${CMAKE_FLAGS} )
(cd ${SOURCE_DIR} && cmake --build ${BUILD_DIR})

# Install
if [ -d "${INSTALL_DIR}" ]; then
    rm -rf ${INSTALL_DIR}
fi

(cd ${SOURCE_DIR} && cmake --install ${BUILD_DIR} --prefix ${INSTALL_DIR})

# Restructure for our format, with a platform subfolder
(cd ${INSTALL_DIR} && mv -v lib ${PLATFORM})
(cd ${INSTALL_DIR} && mkdir -v lib)
(cd ${INSTALL_DIR} && mv -v ${PLATFORM} lib)

# Strip
case $PLATFORM in
    win32|x86_64-win32)
        ;;
    *)
        if [ "" != "$(which strip)" ]; then
            # The strip tool may not work on the target architecture
            set +e
            strip ${INSTALL_DIR}/lib/libwebgpu_dawn.a
            ls -la ${INSTALL_DIR}/lib/libwebgpu_dawn.a
            set -e
        fi
        ;;
esac

# Package
#
VERSION=${SHA1:0:7}
echo VERSION=${VERSION}

PACKAGE=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz

pushd ${INSTALL_DIR}
tar cfvz ${PACKAGE} include lib
popd

echo "Wrote ${PACKAGE}"

cp -v ${INSTALL_DIR}/${PACKAGE} ${DEFOLD_HOME}/packages
