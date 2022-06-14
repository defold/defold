#!/usr/bin/env bash
# Copyright 2020-2022 The Defold Foundation
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

case $PLATFORM in
    arm64-macos)
        CMAKE_OSX_ARCHITECTURES=arm64
        ;;
    x86_64-macos)
        CMAKE_OSX_ARCHITECTURES=x86_64
        ;;
esac

# Follow the build instructions on https://github.com/google/shaderc

git clone https://github.com/google/shaderc $SOURCE_DIR
pushd $SOURCE_DIR
./utils/git-sync-deps
popd

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES} $SOURCE_DIR
make -j8

mkdir -p ./bin/$PLATFORM
cp -v ./glslc/glslc ./bin/$PLATFORM

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=glslc-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE bin
popd
