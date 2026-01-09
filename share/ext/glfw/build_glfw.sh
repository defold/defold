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
# https://www.glfw.org/docs/latest/compile.html

readonly VERSION=3.4
readonly BASE_URL=https://github.com/glfw/glfw/releases/download/${VERSION}/
readonly FILE_BASE=glfw-${VERSION}
readonly FILE_URL=${FILE_BASE}.zip
readonly PRODUCT=glfw

. ../common.sh

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}
LIB_SUFFIX=a
LIB_PREFIX=lib
LIB_OUTPUT_PATH=
LIB_TARGET_NAME=${LIB_PREFIX}glfw3.${LIB_SUFFIX}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi


CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_EXAMPLES=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_DOCS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_USE_HYBRID_HPG=ON ${CMAKE_FLAGS}"

case $PLATFORM in
    win32)
        CMAKE_FLAGS="-A Win32 ${CMAKE_FLAGS}"
        ;;
    x86_64-win32)
        CMAKE_FLAGS="-A x64 ${CMAKE_FLAGS}"
        ;;
esac

case $PLATFORM in
    win32|x86_64-win32)
        LIB_SUFFIX=lib
        LIB_PREFIX=
        LIB_OUTPUT_PATH=Release/
        LIB_TARGET_NAME=libglfw3.${LIB_SUFFIX}

        CMAKE_FLAGS="-DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF ${CMAKE_FLAGS}"
        ;;
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    arm64-linux)
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
esac

function cmi_unpack() {
    echo "Unpacking $SCRIPTDIR/download/$FILE_URL"
    unzip -q -d ${SOURCE_DIR} $SCRIPTDIR/download/$FILE_URL

    mv $FILE_BASE/* .
}

function convert_line_endings() {
    if [[ "${PLATFORM}" == *win* ]]; then
        if [ -f "../patch_${VERSION}" ]; then
            echo "Converting patch file ../patch_${VERSION} to Unix line endings..."
            dos2unix ../patch_${VERSION}
        fi
    fi
}

function normalize_package_folders() {
    echo "Normalizing folder names..."
    # Rename folders to lowercase if incorrectly capitalized
    UPPER_PRODUCT=$(echo "$PRODUCT" | tr '[:lower:]' '[:upper:]')
    [ -d "include/${UPPER_PRODUCT}" ] && mv "include/${UPPER_PRODUCT}" "include/${PRODUCT}"
}

download

mkdir -p ${SOURCE_DIR}

pushd $SOURCE_DIR

cmi_unpack
convert_line_endings
cmi_patch

## BUILD
echo "CMAKE_FLAGS: '${CMAKE_FLAGS}"
cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release

## PACKAGE
SRC_LIB=src/$LIB_OUTPUT_PATH${LIB_PREFIX}glfw3.${LIB_SUFFIX}
TARGET_LIB=lib/$PLATFORM

echo $TARGET_LIB

# clean out anything previously built
rm -rf ./lib/
mkdir -p $TARGET_LIB

cp -v ${SRC_LIB} ${TARGET_LIB}/${LIB_TARGET_NAME}

PACKAGE=glfw-${VERSION}-${PLATFORM}.tar.gz

normalize_package_folders
tar cfvz $PACKAGE lib include

popd

## FINALIZE
mv $SOURCE_DIR/$PACKAGE .

# rm -rf $SOURCE_DIR

