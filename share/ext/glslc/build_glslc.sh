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
