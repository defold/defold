#!/usr/bin/env bash
set -e

PLATFORM=$1
VERSION=vulkan-sdk-1.4.363.0
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
CMAKE_FLAGS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ${CMAKE_FLAGS}"

CMAKE_FLAGS="-DSPIRV_CROSS_STATIC=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_CLI=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_SHARED=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_TESTS=OFF ${CMAKE_FLAGS}"

CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_GLSL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_HLSL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_MSL=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_CPP=OFF ${CMAKE_FLAGS}"
# Disables the JSON writer, not the core reflection API used by shaderc.
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_REFLECT=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_C_API=ON ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DSPIRV_CROSS_ENABLE_UTIL=OFF ${CMAKE_FLAGS}"


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

# Follow the build instructions on https://github.com/KhronosGroup/SPIRV-Cross.git

if [ ! -d "${SOURCE_DIR}" ]; then
    git clone --depth 1 --branch "${VERSION}" https://github.com/KhronosGroup/SPIRV-Cross.git "${SOURCE_DIR}"
fi

# Build

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

echo "CMAKE_FLAGS: ${CMAKE_FLAGS}"

cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release

LIB_SUFFIX=.a
LIB_PREFIX=lib

case $PLATFORM in
    x86_64-win32)
        LIB_SUFFIX=.lib
        LIB_PREFIX=
        if [ -d Release ]; then
            LIB_PREFIX=Release/
        fi
        ;;
    *)
        ;;
esac

mkdir -p ./lib/$PLATFORM
mkdir -p ./include/$PLATFORM
mkdir -p ./include/$PLATFORM/spirv

cp -v ${SOURCE_DIR}/spirv_cross_c.h ./include/$PLATFORM/spirv/spirv_cross_c.h
cp -v ${SOURCE_DIR}/spirv.h         ./include/$PLATFORM/spirv/spirv.h

cp -v ${LIB_PREFIX}spirv-cross-c${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-c${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-core${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-core${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-glsl${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-glsl${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-hlsl${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-hlsl${LIB_SUFFIX}
cp -v ${LIB_PREFIX}spirv-cross-msl${LIB_SUFFIX} ./lib/$PLATFORM/libspirv-cross-msl${LIB_SUFFIX}

popd

# Package

echo VERSION=${VERSION}

PACKAGE=spirv-cross-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE lib include
echo "Wrote ${BUILD_DIR}/${PACKAGE}"
popd
