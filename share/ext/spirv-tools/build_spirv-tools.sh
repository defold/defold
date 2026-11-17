#!/usr/bin/env bash
# License: MIT
# Copyright 2023 The Defold Foundation

set -e

PLATFORM=$1
VERSION=2026.4
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
CMAKE_FLAGS="-DSPIRV_SKIP_TESTS=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_FLAGS}"
        CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
esac

if [ ! -d "${SOURCE_DIR}" ]; then
    git clone --depth 1 --branch "v${VERSION}" https://github.com/KhronosGroup/SPIRV-Tools.git "${SOURCE_DIR}"
fi

python ${SOURCE_DIR}/utils/git-sync-deps

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

echo "CMAKE_FLAGS: '${CMAKE_FLAGS}"
cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release --target spirv-opt -j 8

EXE_SUFFIX=
case $PLATFORM in
    x86_64-win32)
        EXE_SUFFIX=.exe
        if [ -f ./tools/Release/spirv-opt${EXE_SUFFIX} ]; then
            SRC_EXE_SPIRV_OPT=./tools/Release/spirv-opt${EXE_SUFFIX}
        else
            SRC_EXE_SPIRV_OPT=./tools/spirv-opt${EXE_SUFFIX}
        fi
        ;;
    *)
        SRC_EXE_SPIRV_OPT=./tools/spirv-opt${EXE_SUFFIX}
        ;;
esac

TARGET_EXE_SPIRV_OPT=./bin/$PLATFORM/spirv-opt${EXE_SUFFIX}

mkdir -p ./bin/$PLATFORM

cp -v ${SRC_EXE_SPIRV_OPT} ${TARGET_EXE_SPIRV_OPT}
# Do not retain the unused linker when reusing a staging directory.
rm -f ./bin/$PLATFORM/spirv-link${EXE_SUFFIX}

case $PLATFORM in
    x86_64-win32)
        ;;
    *)
        strip ${TARGET_EXE_SPIRV_OPT}
        ;;
esac

popd

# Package

echo VERSION=${VERSION}

PACKAGE=spirv-tools-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE bin
echo "Wrote ${BUILD_DIR}/${PACKAGE}"
popd
