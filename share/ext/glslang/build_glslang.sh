#!/usr/bin/env bash
# License: MIT
# Copyright 2022 The Defold Foundation

set -e

PLATFORM=$1
VERSION=16.6.0
PWD=$(pwd)
SOURCE_DIR=${PWD}/source-${VERSION}
BUILD_DIR=${PWD}/build/${VERSION}/${PLATFORM}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

eval "$(python "${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py" VERSION_MACOSX_MIN)"
OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLSLANG_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DENABLE_GLSLANG_BINARIES=ON ${CMAKE_FLAGS}"
# GLSL is the only input language. Bob optimizes SPIR-V with spirv-opt.
CMAKE_FLAGS="-DENABLE_HLSL=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DENABLE_OPT=OFF ${CMAKE_FLAGS}"
# static link on MSVC
CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        ;;
esac

# Follow the build instructions on https://github.com/KhronosGroup/glslang
if [ ! -d "$SOURCE_DIR" ]; then
    git clone --depth 1 --branch "${VERSION}" https://github.com/KhronosGroup/glslang.git "${SOURCE_DIR}"
fi

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake ${CMAKE_FLAGS} $SOURCE_DIR
cmake --build . --config Release --target glslang-standalone -j 8

mkdir -p ./bin/$PLATFORM

EXE_SUFFIX=
case $PLATFORM in
    x86_64-win32)
        EXE_SUFFIX=.exe
        if [ -f ./StandAlone/Release/glslang${EXE_SUFFIX} ]; then
            cp -v ./StandAlone/Release/glslang${EXE_SUFFIX} ./bin/$PLATFORM
        else
            cp -v ./StandAlone/glslang${EXE_SUFFIX} ./bin/$PLATFORM
        fi
        ;;
    *)
        cp -v ./StandAlone/glslang${EXE_SUFFIX} ./bin/$PLATFORM
        ;;
esac

case $PLATFORM in
    x86_64-win32)
        ;;
    *)
        strip ./bin/$PLATFORM/glslang${EXE_SUFFIX}
        ;;
esac

popd

# Package
echo VERSION=${VERSION}

PACKAGE=glslang-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz ${PACKAGE} bin
popd

echo "Wrote ${PACKAGE}"
