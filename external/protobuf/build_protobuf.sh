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

set -eo pipefail

readonly PRODUCT=protobuf
readonly VERSION=35.1
readonly PROTOBUF_FILE=protobuf-${VERSION}.tar.gz
readonly ABSEIL_VERSION=20250512.1
readonly ABSEIL_FILE=abseil-cpp-${ABSEIL_VERSION}.tar.gz
PROTOBUF_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
readonly PROTOBUF_SCRIPT_DIR
readonly PLATFORM=${1:-}

if [ -z "${PLATFORM}" ]; then
    echo "Usage: $0 <target-platform>" >&2
    exit 1
fi

# shellcheck source=/dev/null
. "${PROTOBUF_SCRIPT_DIR}/../../share/ext/common.sh"

readonly BUILD_ROOT="${PROTOBUF_BUILD_ROOT:-${PROTOBUF_SCRIPT_DIR}/build/${PLATFORM}}"
readonly PACKAGE_DIR="${PROTOBUF_PACKAGE_DIR:-${PROTOBUF_SCRIPT_DIR}/package}"
readonly BUILD_JOBS="${PROTOBUF_BUILD_JOBS:-8}"
readonly TMP_HOST="${BUILD_ROOT}/tmp_host"
readonly TMP_TARGET="${BUILD_ROOT}/tmp_target"
readonly SOURCE_HOST_PROTOBUF="${TMP_HOST}/protobuf"
readonly SOURCE_HOST_ABSEIL="${TMP_HOST}/abseil"
readonly SOURCE_TARGET_PROTOBUF="${TMP_TARGET}/protobuf"
readonly SOURCE_TARGET_ABSEIL="${TMP_TARGET}/abseil"
readonly INSTALL_HOST="${BUILD_ROOT}/install_host"
readonly INSTALL_TARGET="${BUILD_ROOT}/install_target"
readonly PACKAGE_STAGE="${BUILD_ROOT}/package"

case ${PLATFORM} in
    x86_64-macos|arm64-macos|x86_64-linux|arm64-linux|win32|x86_64-win32)
        IS_DESKTOP=1
        ;;
    *)
        echo "Unsupported protobuf target: ${PLATFORM}. Protobuf is packaged for desktop targets only." >&2
        exit 1
        ;;
esac

function unpack_sources() {
    local root=$1

    mkdir -p "${root}/protobuf" "${root}/abseil"
    tar xf "${PROTOBUF_SCRIPT_DIR}/${PROTOBUF_FILE}" --directory "${root}/protobuf" --strip-components=1
    tar xf "${PROTOBUF_SCRIPT_DIR}/${ABSEIL_FILE}" --directory "${root}/abseil" --strip-components=1
}

function normalize_cmake_compilers() {
    if [[ "${CC:-}" == "arch -x86_64 "* ]]; then
        export CC="${CC#arch -x86_64 }"
    fi
    if [[ "${CXX:-}" == "arch -x86_64 "* ]]; then
        export CXX="${CXX#arch -x86_64 }"
    fi
}

function configure_abseil() {
    local source=$1
    local install=$2
    local platform=$3
    local osx_arch=

    case ${platform} in
        arm64-macos)
            osx_arch=arm64
            ;;
        x86_64-macos)
            osx_arch=x86_64
            ;;
    esac

    cmake -S "${source}" -B "${source}/_build" -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install}" \
        -DCMAKE_INSTALL_LIBDIR="lib/${platform}" \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_EXTENSIONS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_OSX_ARCHITECTURES="${osx_arch}" \
        -DCMAKE_C_FLAGS="${FLAGS:-} ${CFLAGS:-}" \
        -DCMAKE_CXX_FLAGS="${FLAGS:-} ${CXXFLAGS:-}" \
        -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-}" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_STATIC_LIBRARY_PREFIX=lib \
        -DABSL_BUILD_TESTING=OFF \
        -DABSL_ENABLE_INSTALL=ON \
        -DABSL_MSVC_STATIC_RUNTIME=ON \
        -DABSL_PROPAGATE_CXX_STD=ON
}

function configure_protobuf() {
    local source=$1
    local install=$2
    local platform=$3
    local build_protoc=$4
    local protoc=${5:-}
    local build=${6:-"${source}/_build"}
    local osx_arch=

    case ${platform} in
        arm64-macos)
            osx_arch=arm64
            ;;
        x86_64-macos)
            osx_arch=x86_64
            ;;
    esac

    local protoc_args=(-Dprotobuf_BUILD_PROTOC_BINARIES="${build_protoc}")
    if [ -n "${protoc}" ]; then
        protoc_args+=(-DWITH_PROTOC="${protoc}")
    fi

    cmake -S "${source}" -B "${build}" -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install}" \
        -DCMAKE_INSTALL_LIBDIR="lib/${platform}" \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_EXTENSIONS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_OSX_ARCHITECTURES="${osx_arch}" \
        -DCMAKE_C_FLAGS="${FLAGS:-} ${CFLAGS:-}" \
        -DCMAKE_CXX_FLAGS="${FLAGS:-} ${CXXFLAGS:-}" \
        -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-}" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_STATIC_LIBRARY_PREFIX=lib \
        -Dabsl_DIR="${install}/lib/${platform}/cmake/absl" \
        -Dprotobuf_BUILD_CONFORMANCE=OFF \
        -Dprotobuf_BUILD_EXAMPLES=OFF \
        -Dprotobuf_BUILD_LIBUPB="${build_protoc}" \
        -Dprotobuf_BUILD_SHARED_LIBS=OFF \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_DISABLE_RTTI=ON \
        -Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON \
        -Dprotobuf_WITH_ZLIB=OFF \
        "${protoc_args[@]}"
}

function merge_protobuf_dependencies() {
    local libdir="${INSTALL_TARGET}/lib/${PLATFORM}"
    local output
    local archives=()

    while IFS= read -r archive; do
        archives+=("${archive}")
    done < <(find "${libdir}" -maxdepth 1 -type f \( -name 'libabsl_*.a' -o -name 'libabsl_*.lib' \) -print | sort)
    if [ "${PLATFORM}" = "win32" ] || [ "${PLATFORM}" = "x86_64-win32" ]; then
        output="${libdir}/libprotobuf_deps.lib"
        local response="${SOURCE_TARGET_PROTOBUF}/_build/protobuf_deps.rsp"
        printf '"%s"\n' "${libdir}/libutf8_validity.lib" "${archives[@]}" > "${response}"
        lib.exe "/OUT:${output}" "@${response}"
    elif [[ "${PLATFORM}" == *-macos ]]; then
        output="${libdir}/libprotobuf_deps.a"
        libtool -static -o "${output}" "${libdir}/libutf8_validity.a" "${archives[@]}"
    else
        output="${libdir}/libprotobuf_deps.a"
        local mri="${SOURCE_TARGET_PROTOBUF}/_build/protobuf_deps.mri"
        {
            printf 'CREATE %s\n' "${output}"
            printf 'ADDLIB %s\n' "${libdir}/libutf8_validity.a" "${archives[@]}"
            printf 'SAVE\nEND\n'
        } > "${mri}"
        "${AR}" -M < "${mri}"
    fi
}

function create_archive() {
    local archive=$1
    shift

    if [ "${PLATFORM}" = "win32" ] || [ "${PLATFORM}" = "x86_64-win32" ]; then
        # GNU tar treats the drive-letter colon in paths such as D:/... as a
        # remote archive separator unless explicitly told that the path is local.
        tar --force-local -czvf "${archive}" "$@"
    else
        tar -czvf "${archive}" "$@"
    fi
}

rm -rf "${TMP_HOST}" "${TMP_TARGET}" "${INSTALL_HOST}" "${INSTALL_TARGET}" "${PACKAGE_STAGE}"
mkdir -p "${BUILD_ROOT}" "${PACKAGE_DIR}"
unpack_sources "${TMP_HOST}"
unpack_sources "${TMP_TARGET}"

echo "**************************************************"
echo "BUILD HOST TOOLS"
echo "**************************************************"

cmi_setup_cc "${HOST_PLATFORM}"
normalize_cmake_compilers
configure_abseil "${SOURCE_HOST_ABSEIL}" "${INSTALL_HOST}" "${HOST_PLATFORM}"
cmake --build "${SOURCE_HOST_ABSEIL}/_build" --target install --parallel "${BUILD_JOBS}"
configure_protobuf "${SOURCE_HOST_PROTOBUF}" "${INSTALL_HOST}" "${HOST_PLATFORM}" ON
cmake --build "${SOURCE_HOST_PROTOBUF}/_build" --target protoc --parallel "${BUILD_JOBS}"

echo "**************************************************"
echo "BUILD TARGET LIBRARIES FOR ${PLATFORM}"
echo "**************************************************"

unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS FLAGS CMAKE_TOOLCHAIN_FILE SDKROOT MACOSX_DEPLOYMENT_TARGET
cmi_setup_cc "${PLATFORM}"
normalize_cmake_compilers
if [ "${PLATFORM}" = "win32" ] || [ "${PLATFORM}" = "x86_64-win32" ]; then
    FLAGS=
else
    FLAGS=-fPIC
fi

configure_abseil "${SOURCE_TARGET_ABSEIL}" "${INSTALL_TARGET}" "${PLATFORM}"
cmake --build "${SOURCE_TARGET_ABSEIL}/_build" --target install --parallel "${BUILD_JOBS}"

HOST_PROTOC="${SOURCE_HOST_PROTOBUF}/_build/protoc"
if [ "${HOST_PLATFORM}" = "win32" ] || [ "${HOST_PLATFORM}" = "x86_64-win32" ]; then
    HOST_PROTOC="${HOST_PROTOC}.exe"
fi
readonly HOST_PROTOC
configure_protobuf "${SOURCE_TARGET_PROTOBUF}" "${INSTALL_TARGET}" "${PLATFORM}" OFF "${HOST_PROTOC}"
cmake --build "${SOURCE_TARGET_PROTOBUF}/_build" --target install --parallel "${BUILD_JOBS}"
merge_protobuf_dependencies

if [ -n "${IS_DESKTOP:-}" ]; then
    configure_protobuf "${SOURCE_TARGET_PROTOBUF}" "${INSTALL_TARGET}" "${PLATFORM}" ON "" "${SOURCE_TARGET_PROTOBUF}/_protoc_build"
    cmake --build "${SOURCE_TARGET_PROTOBUF}/_protoc_build" --target protoc --parallel "${BUILD_JOBS}"
fi

echo "**************************************************"
echo "PACKAGE PROTOBUF FOR ${PLATFORM}"
echo "**************************************************"

case ${PLATFORM} in
    win32|x86_64-win32)
        SUFFIX=.exe
        ;;
    x86_64-macos|arm64-macos|x86_64-linux|arm64-linux)
        strip "${SOURCE_TARGET_PROTOBUF}/_protoc_build/protoc"
        ;;
esac

mkdir -p "${PACKAGE_STAGE}"
pushd "${PACKAGE_STAGE}" >/dev/null

mkdir -p "lib/${PLATFORM}" "bin/${PLATFORM}"
cp -R "${INSTALL_TARGET}/lib/${PLATFORM}/." "lib/${PLATFORM}/"
if [ -n "${IS_DESKTOP:-}" ]; then
    cp "${SOURCE_TARGET_PROTOBUF}/_protoc_build/protoc${SUFFIX:-}" "bin/${PLATFORM}/"
fi

create_archive "${PACKAGE_DIR}/${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz" lib bin
create_archive "${PACKAGE_DIR}/${PRODUCT}-${VERSION}-common.tar.gz" -C "${INSTALL_TARGET}" include

rm -rf lib bin
popd >/dev/null

echo "Wrote ${PACKAGE_DIR}/${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz"
echo "Wrote ${PACKAGE_DIR}/${PRODUCT}-${VERSION}-common.tar.gz"

rm -rf "${TMP_HOST}" "${TMP_TARGET}" "${INSTALL_HOST}" "${INSTALL_TARGET}" "${PACKAGE_STAGE}"
