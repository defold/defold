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


readonly PRODUCT=protobuf
readonly VERSION=34.0
readonly FILE_URL=protobuf-${VERSION}.tar.gz
readonly BASE_URL=https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}
readonly PLATFORM=$1

. ../common.sh

readonly SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly PACKAGE_DIR=${SCRIPTDIR}/package

set -e

export TMP_HOST=tmp_host
export TMP_TARGET=tmp_target
readonly HOST_BUILD_DIR=${SCRIPTDIR}/${TMP_HOST}/_build
readonly TARGET_BUILD_DIR=${SCRIPTDIR}/${TMP_TARGET}/_build

if [ -z "${PLATFORM}" ]; then
    echo "Usage: $0 <platform>"
    exit 1
fi

if [ "${PLATFORM}" == "armv7-android" ]; then
    ANDROID_VERSION=21
fi

if [ "${PLATFORM}" == "arm64-macos" ]; then
    MACOS_ARCHS=arm64
fi
if [ "${PLATFORM}" == "x86_64-macos" ]; then
    MACOS_ARCHS=x86_64
fi
case ${PLATFORM} in
    win32|x86_64-win32)
        PROTOC_SUFFIX=.exe
        ;;
    *)
        PROTOC_SUFFIX=
        ;;
esac

if [ ! -f "${SCRIPTDIR}/${FILE_URL}" ]; then
    echo "Downloading ${FILE_URL}"
    curl -L -o "${SCRIPTDIR}/${FILE_URL}" "${BASE_URL}/${FILE_URL}"
fi

function cmake_compiler() {
    local compiler=$1
    case "${compiler}" in
        "arch -x86_64 "*)
            echo "${compiler#arch -x86_64 }"
            ;;
        *)
            echo "${compiler}"
            ;;
    esac
}

function cmake_configure_protobuf() {
    local source_dir=$1
    local build_dir=$2
    local install_dir=$3
    local platform=$4
    shift 4

    local cmake_generator=${CMAKE_GENERATOR:-"Unix Makefiles"}
    local c_compiler
    local cxx_compiler
    c_compiler=$(cmake_compiler "${CC}")
    cxx_compiler=$(cmake_compiler "${CXX}")

    local cmake_args=(
        -G "${cmake_generator}"
        -S "${source_dir}"
        -B "${build_dir}"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="${install_dir}"
        -DCMAKE_INSTALL_BINDIR="bin/${platform}"
        -DCMAKE_INSTALL_LIBDIR="lib/${platform}"
        -DCMAKE_C_COMPILER_WORKS=1
        -DCMAKE_CXX_COMPILER_WORKS=1
        -DCMAKE_C_FLAGS="${FLAGS} ${CFLAGS}"
        -DCMAKE_CXX_FLAGS="${FLAGS} ${CXXFLAGS}"
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_CXX_EXTENSIONS=OFF
        -DABSL_PROPAGATE_CXX_STD=ON
        -Dprotobuf_BUILD_EXAMPLES=OFF
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_CONFORMANCE=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -Dprotobuf_DISABLE_RTTI=OFF
        -Dprotobuf_FORCE_FETCH_DEPENDENCIES=ON
        -Dprotobuf_WITH_ZLIB=OFF
    )

    if [ "${c_compiler}" != "" ]; then
        cmake_args+=(-DCMAKE_C_COMPILER="${c_compiler}")
    fi
    if [ "${cxx_compiler}" != "" ]; then
        cmake_args+=(-DCMAKE_CXX_COMPILER="${cxx_compiler}")
    fi
    if [ "${MACOS_ARCHS}" != "" ]; then
        cmake_args+=(
            -DCMAKE_OSX_ARCHITECTURES="${MACOS_ARCHS}"
            -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_MIN_SDK_VERSION}"
        )
    fi

    cmake "${cmake_args[@]}" "$@"
}

function package_common() {
    local package_name=${PRODUCT}-${VERSION}-common.tar.gz

    rm -f "${PACKAGE_DIR}/${package_name}"

    pushd "${INSTALL_HOST}" >/dev/null
        echo "Packaging '${package_name}'..."
        tar cfvz "${PACKAGE_DIR}/${package_name}" include
    popd >/dev/null
}

function package_platform() {
    local package_name=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz

    rm -f "${PACKAGE_DIR}/${package_name}"

    case ${PLATFORM} in
        x86_64-macos|arm64-macos|x86_64-linux|arm64-linux|win32|x86_64-win32)
            IS_DESKTOP=1
            ;;
        *)
            IS_DESKTOP=
            ;;
    esac

    rm -rf "${SCRIPTDIR}/package_root"
    mkdir -p "${SCRIPTDIR}/package_root/lib"
    cp -R "${INSTALL_TARGET}/lib/${PLATFORM}" "${SCRIPTDIR}/package_root/lib/"

    if [ "${IS_DESKTOP}" != "" ]; then
        mkdir -p "${SCRIPTDIR}/package_root/bin/${PLATFORM}"
        cp -L "${INSTALL_HOST}/bin/${PLATFORM}/protoc${PROTOC_SUFFIX}" "${SCRIPTDIR}/package_root/bin/${PLATFORM}/protoc${PROTOC_SUFFIX}"
        chmod +x "${SCRIPTDIR}/package_root/bin/${PLATFORM}/protoc${PROTOC_SUFFIX}"

        case ${PLATFORM} in
            x86_64-macos|arm64-macos|x86_64-linux|arm64-linux)
                echo "Stripping protoc executable"
                strip "${SCRIPTDIR}/package_root/bin/${PLATFORM}/protoc"
                ;;
        esac
    fi

    pushd "${SCRIPTDIR}/package_root" >/dev/null
        echo "Packaging '${package_name}'..."
        if [ "${IS_DESKTOP}" != "" ]; then
            tar cfvz "${PACKAGE_DIR}/${package_name}" lib bin
        else
            tar cfvz "${PACKAGE_DIR}/${package_name}" lib
        fi
    popd >/dev/null

    rm -rf "${SCRIPTDIR}/package_root"
}

echo "**************************************************"
echo "BUILD HOST TOOLS"
echo "**************************************************"

export SOURCE_HOST=${SCRIPTDIR}/${TMP_HOST}/${PACKAGEDIR}
export INSTALL_HOST=${SCRIPTDIR}/install_host

HOST_TOOLS_PLATFORM=${HOST_PLATFORM}
if [ "${HOST_PLATFORM}" == "arm64-macos" ] && [ "${PLATFORM}" == "x86_64-macos" ]; then
    HOST_TOOLS_PLATFORM=x86_64-macos
fi

cmi_setup_cc ${HOST_TOOLS_PLATFORM}

rm -rf "${TMP_HOST}" "${INSTALL_HOST}"
mkdir -p "${INSTALL_HOST}"
echo "Unpack to ${TMP_HOST}"

mkdir -p "${TMP_HOST}"
tar xf "${FILE_URL}" --directory "${TMP_HOST}" --strip-components=1

echo "SOURCE_HOST:" ${SOURCE_HOST}
cmake_configure_protobuf "${SOURCE_HOST}" "${HOST_BUILD_DIR}" "${INSTALL_HOST}" "${PLATFORM}" \
    -Dprotobuf_BUILD_PROTOC_BINARIES=ON
cmake --build "${HOST_BUILD_DIR}" --config Release --target install --parallel 8

echo "**************************************************"
echo "BUILD TARGET LIB for ${PLATFORM}"
echo "**************************************************"

unset CFLAGS
unset CXXFLAGS
unset CPPFLAGS
unset FLAGS
unset MACOSX_DEPLOYMENT_TARGET
unset SDKROOT

cmi_setup_cc ${PLATFORM}

TARGET_CMAKE_ARGS=()
case ${PLATFORM} in
    arm64-ios|x86_64-ios)
        TARGET_CMAKE_ARGS+=(
            -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
            -DCMAKE_HAVE_LIBC_PTHREAD=1
            -DCMAKE_USE_PTHREADS_INIT=1
            -DThreads_FOUND=1
            -Dprotobuf_HAVE_BUILTIN_ATOMICS=1
        )
        ;;
    arm64-android|armv7-android)
        TARGET_CMAKE_ARGS+=(
            -DCMAKE_HAVE_LIBC_PTHREAD=1
            -DCMAKE_USE_PTHREADS_INIT=1
            -DThreads_FOUND=1
        )
        ;;
esac

export SOURCE_TARGET=${SCRIPTDIR}/${TMP_TARGET}/${PACKAGEDIR}
export INSTALL_TARGET=${SCRIPTDIR}/install_target

rm -rf "${TMP_TARGET}" "${INSTALL_TARGET}"
mkdir -p "${INSTALL_TARGET}"

echo "Unpack to ${TMP_TARGET}"

mkdir -p "${TMP_TARGET}"
tar xf "${FILE_URL}" --directory "${TMP_TARGET}" --strip-components=1

echo "SOURCE_TARGET:" ${SOURCE_TARGET}

export FLAGS="-fPIC"

cmake_configure_protobuf "${SOURCE_TARGET}" "${TARGET_BUILD_DIR}" "${INSTALL_TARGET}" "${PLATFORM}" \
    -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}" \
    -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
    -Dprotobuf_BUILD_LIBUPB=OFF \
    "${TARGET_CMAKE_ARGS[@]}" \
    -DWITH_PROTOC="${INSTALL_HOST}/bin/${PLATFORM}/protoc${PROTOC_SUFFIX}"
cmake --build "${TARGET_BUILD_DIR}" --config Release --target install --parallel 8


echo "**************************************************"
echo "Package protobuf for ${PLATFORM}"
echo "**************************************************"

mkdir -p "${PACKAGE_DIR}"
package_common
package_platform

echo "Wrote ./package/${PRODUCT}-${VERSION}-common.tar.gz"
echo "Wrote ./package/${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz"

rm -rf "${TMP_HOST}"
rm -rf "${TMP_TARGET}"
rm -rf "${INSTALL_HOST}"
rm -rf "${INSTALL_TARGET}"
