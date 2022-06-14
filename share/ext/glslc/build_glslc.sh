#!/usr/bin/env bash

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

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
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

if [ -z "$SOURCE_DIR" ]; then
    git clone https://github.com/google/shaderc $SOURCE_DIR

    pushd $SOURCE_DIR
    ./utils/git-sync-deps
    popd
fi

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake ${CMAKE_FLAGS} $SOURCE_DIR
make -j8

mkdir -p ./bin/$PLATFORM
cp -v ./glslc/glslc ./bin/$PLATFORM
strip ./bin/$PLATFORM/glslc

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=glslc-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE bin
popd
