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
# Copyright 2024 The Defold Foundation

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
SHA1=30aabb3f42406df45a910d8496f9bee17eeba9bb
BUILD_DIR=${PWD}/build/${PLATFORM}

unset CFLAGS
unset CXXFLAGS
unset CPPFLAGS

. ../common.sh

function join_by() {
    local IFS="$1"
    shift
    echo "$*"
}

function to_windows_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        echo "$1"
    fi
}

function to_unix_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$1"
    else
        echo "$1"
    fi
}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_MACOSX_MIN)
OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN

CONFIG=Release

CMAKE_FLAGS=()
CMAKE_GENERATOR_ARGS=()

CMAKE_FLAGS+=(-DASTCENC_CLI=OFF)
CMAKE_FLAGS+=(-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded)
# The default Unix Makefile/Ninja generators build Debug unless build type is set explicitly.
# Our macOS and Linux builds use single-config generators, so make sure we get an optimized lib.
CMAKE_FLAGS+=(-DCMAKE_BUILD_TYPE=${CONFIG})
BUILD_CMD=(cmake --build . --config $CONFIG -j 8)

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS+=(-DCMAKE_OSX_ARCHITECTURES=arm64)
        CMAKE_FLAGS+=(-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION})
        CMAKE_FLAGS+=(-DASTCENC_ISA_NEON=ON)
        CMAKE_FLAGS+=(-DASTCENC_PACKAGE=arm64)
        ;;
    x86_64-macos)
        CMAKE_FLAGS+=(-DCMAKE_OSX_ARCHITECTURES=x86_64)
        CMAKE_FLAGS+=(-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION})
        CMAKE_FLAGS+=(-DASTCENC_ISA_SSE41=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_NATIVE=OFF)
        CMAKE_FLAGS+=(-DASTCENC_PACKAGE=x64)
        ;;
    arm64-linux)
        CMAKE_FLAGS+=(-DARCH=aarch64)
        CMAKE_FLAGS+=(-DASTCENC_ISA_NEON=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_SVE_128=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_SVE_256=ON)
        CMAKE_FLAGS+=(-DASTCENC_PACKAGE=arm64)
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-linux)
        CMAKE_FLAGS+=(-DASTCENC_ISA_AVX2=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_SSE41=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_SSE2=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_NATIVE=OFF)
        CMAKE_FLAGS+=(-DASTCENC_PACKAGE=x64)
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-win32)
        eval $(python ${DYNAMO_HOME}/../../build_tools/set_sdk_vars.py VERSION_WINDOWS_MSVC_2022 VERSION_WINDOWS_SDK_10)
        MSVC_ROOT="${DYNAMO_HOME}/ext/SDKs/Win32/MicrosoftVisualStudio14.0/VC/Tools/MSVC/${VERSION_WINDOWS_MSVC_2022}"
        WINSDK_ROOT="${DYNAMO_HOME}/ext/SDKs/Win32/WindowsKits/10"
        MSVC_BIN=$(to_unix_path "${MSVC_ROOT}/bin/Hostx64/x64")
        WINSDK_BIN=$(to_unix_path "${WINSDK_ROOT}/bin/${VERSION_WINDOWS_SDK_10}/x64")
        export PATH="${MSVC_BIN}:${WINSDK_BIN}:$PATH"

        include_paths=(
            "${MSVC_ROOT}/ATLMFC/include"
            "${MSVC_ROOT}/include"
            "${WINSDK_ROOT}/Include/${VERSION_WINDOWS_SDK_10}/ucrt"
            "${WINSDK_ROOT}/Include/${VERSION_WINDOWS_SDK_10}/shared"
            "${WINSDK_ROOT}/Include/${VERSION_WINDOWS_SDK_10}/um"
            "${WINSDK_ROOT}/Include/${VERSION_WINDOWS_SDK_10}/winrt"
        )
        lib_paths=(
            "${MSVC_ROOT}/ATLMFC/lib/x64"
            "${MSVC_ROOT}/lib/x64"
            "${WINSDK_ROOT}/Lib/${VERSION_WINDOWS_SDK_10}/ucrt/x64"
            "${WINSDK_ROOT}/Lib/${VERSION_WINDOWS_SDK_10}/um/x64"
        )

        declare -a include_paths_win=()
        declare -a lib_paths_win=()
        for p in "${include_paths[@]}"; do
            include_paths_win+=("$(to_windows_path "$p")")
        done
        for p in "${lib_paths[@]}"; do
            lib_paths_win+=("$(to_windows_path "$p")")
        done
        export INCLUDE="$(join_by ';' "${include_paths_win[@]}")"
        export LIB="$(join_by ';' "${lib_paths_win[@]}")"
        export LIBPATH="$LIB"

        CMAKE_GENERATOR_ARGS=(-G "NMake Makefiles")
        CMAKE_FLAGS+=(-DCMAKE_C_COMPILER="$(to_unix_path "${MSVC_ROOT}/bin/Hostx64/x64/cl.exe")")
        CMAKE_FLAGS+=(-DCMAKE_CXX_COMPILER="$(to_unix_path "${MSVC_ROOT}/bin/Hostx64/x64/cl.exe")")
        CMAKE_FLAGS+=(-DASTCENC_ISA_SSE41=ON)
        CMAKE_FLAGS+=(-DASTCENC_ISA_NATIVE=OFF)
        CMAKE_FLAGS+=(-DASTCENC_PACKAGE=x64)
        BUILD_CMD=(cmake --build .)
        ;;
esac

# Follow the build instructions on https://github.com/ARM-software/astc-encoder
if [ ! -d "$SOURCE_DIR" ]; then
    git clone https://github.com/ARM-software/astc-encoder.git $SOURCE_DIR
    (cd $SOURCE_DIR && git reset --hard $SHA1)
fi

mkdir -p ${BUILD_DIR}

pushd $BUILD_DIR

cmake "${CMAKE_GENERATOR_ARGS[@]}" "${CMAKE_FLAGS[@]}" "$SOURCE_DIR"
"${BUILD_CMD[@]}"

LIB_NAME=
LIB_EXT=.a

case $PLATFORM in
    arm64-macos)
        LIB_NAME=libastcenc-neon-static.a
        ;;
    x86_64-macos)
        LIB_NAME=libastcenc-sse4.1-static.a
        ;;
    arm64-linux)
        LIB_NAME=libastcenc-neon-static.a
        ;;
    x86_64-linux)
        LIB_NAME=libastcenc-sse4.1-static.a
        ;;
    x86_64-win32)
        LIB_NAME=astcenc-sse4.1-static.lib
        LIB_EXT=.lib
        ;;
    *)
        ;;
esac

mkdir -p ./lib/$PLATFORM
mkdir -p ./include/$PLATFORM
mkdir -p ./include/$PLATFORM/astcenc

cp -v Source/${LIB_NAME} ./lib/$PLATFORM/libastcenc${LIB_EXT}

cp -v ../../source/Source/astcenc.h ./include/$PLATFORM/astcenc/astcenc.h

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=astcenc-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz $PACKAGE lib include
echo "Wrote ${BUILD_DIR}/${PACKAGE}"
popd

echo "Done!"
