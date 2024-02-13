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


# Compilation guide:
# https://www.glfw.org/docs/latest/compile.html

readonly VERSION=3.3.9
readonly BASE_URL=https://github.com/glfw/glfw/releases/download/${VERSION}/
readonly FILE_URL=glfw-${VERSION}.zip
readonly PRODUCT=glfw

. ../common.sh

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_EXAMPLES=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_DOCS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_VULKAN_STATIC=OFF ${CMAKE_FLAGS}"

CMAKE_FLAGS_VULKAN="-DGLFW_VULKAN_STATIC=ON"

# TODO: Investigate if we want this or our own loader.
# CMAKE_FLAGS="-DGLFW_VULKAN_STATIC=OFF ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        ;;
esac

download

mkdir -p ${SOURCE_DIR}

pushd $SOURCE_DIR

cmi_unpack

cmi_patch

## BUILD
echo "CMAKE_FLAGS: '${CMAKE_FLAGS}"
cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release

## PACKAGE
SRC_LIB=./src/libglfw3.a
TARGET_LIB=./lib/$PLATFORM

# clean out anything previously built
rm -rf ./lib/
mkdir -p $TARGET_LIB

cp -v ${SRC_LIB} ${TARGET_LIB}

## BUILD with vulkan support
echo "CMAKE_FLAGS: '${CMAKE_FLAGS} ${CMAKE_FLAGS_VULKAN}"
cmake ${CMAKE_FLAGS} ${CMAKE_FLAGS_VULKAN} ${SOURCE_DIR}
cmake --build . --config Release

cp -v ${SRC_LIB} ${TARGET_LIB}/libglfw3-vulkan.a

PACKAGE=glfw-${VERSION}-${PLATFORM}.tar.gz

tar cfvz $PACKAGE lib include

popd

## FINALIZE
mv $SOURCE_DIR/$PACKAGE .

rm -rf $SOURCE_DIR

