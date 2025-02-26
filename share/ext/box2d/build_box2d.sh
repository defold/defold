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
# Copyright 2022 The Defold Foundation

PLATFORM=$1
PWD=$(pwd)
BUILD_DIR=${PWD}/build/${PLATFORM}

readonly PRODUCT=box2d
readonly VERSION=28adacf82377d4113f2ed00586141463244b9d10 # v3.0.0
readonly PACKAGE_NAME=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz
readonly HEADERS_PACKAGE_NAME=${PRODUCT}-${VERSION}-common.tar.gz

readonly BOX2D_URL=https://github.com/erincatto/box2d/archive/${VERSION}.zip
readonly BOX2D_DIR=$(realpath ./box2d)

. ../common.sh

cmi_setup_cc $PLATFORM

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

echo "PLATFORM: ${PLATFORM}"

CMAKE_FLAGS="-DBOX2D_BUILD_DOCS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DBOX2D_SAMPLES=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DBOX2D_UNIT_TESTS=OFF ${CMAKE_FLAGS}"

# CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Debug ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"

CMAKE_BUILD_FLAGS=

case $PLATFORM in
    x86_64-ios)
        CMAKE_FLAGS="-DCMAKE_OSX_SYSROOT=iphonesimulator ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_SYSTEM_NAME=iOS ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${IOS_MIN_SDK_VERSION} ${CMAKE_FLAGS}"

        CXXFLAGS="-DDEFOLD_USE_POSIX_MEMALIGN ${CXXFLAGS}"
        CFLAGS="-DDEFOLD_USE_POSIX_MEMALIGN ${CFLAGS}"
        ;;
    arm64-ios)
        CMAKE_FLAGS="-DCMAKE_OSX_SYSROOT=iphoneos ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_SYSTEM_NAME=iOS ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${IOS_MIN_SDK_VERSION} ${CMAKE_FLAGS}"

        CXXFLAGS="-DDEFOLD_USE_POSIX_MEMALIGN ${CXXFLAGS}"
        CFLAGS="-DDEFOLD_USE_POSIX_MEMALIGN ${CFLAGS}"
        ;;
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        ;;
esac

# eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
# OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

function download {
    local url=$1
    local filename=$2
    local output=$3

    echo "**************************************************"
    echo "Download ${url}  ->  ${filename}"
    echo "**************************************************"

    if [ ! -e "${filename}" ]; then
        echo "${filename} not found. Downloading ${url} ..."
        wget ${url} -O ${filename}
    else
        echo "Found ${filename}"
    fi

    echo "**************************************************"
    echo "Extracting ${filename}  ->  ${output}"
    echo "**************************************************"

    if [ ! -d "${output}" ]; then
        echo "${output} not found. Extracting source"
        mkdir -p ${output}
        tar --strip-components=1 -xz -C ${output} -f ${filename}
        echo "Done."
    else
        echo "Found ${output}"
    fi
}

download ${BOX2D_URL} box2d.tar.gz ${BOX2D_DIR}

pushd ${BOX2D_DIR}

cmi_patch

popd

# Build

mkdir -p ${BUILD_DIR}
pushd $BUILD_DIR

echo "**************************************************"
echo "CMAKE_FLAGS: ${CMAKE_FLAGS}"
echo "**************************************************"

cmake ${CMAKE_FLAGS} $BOX2D_DIR
#cmake --build . --config Debug -j 8
cmake --build . --config Release ${CMAKE_BUILD_FLAGS} -j 8

mkdir -p ./lib/$PLATFORM
mkdir -p ./include
mkdir -p ./include/box2d
mkdir -p ./include/box2d/src

cp -v ./src/*.a ./lib/$PLATFORM

tar cfvz ${PACKAGE_NAME} lib

cp -v -r ${BOX2D_DIR}/include/box2d ./include
cp -v -r ${BOX2D_DIR}/src/*.h ./include/box2d/src

tar cfvz ${HEADERS_PACKAGE_NAME} include

popd
