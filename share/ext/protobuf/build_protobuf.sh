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
readonly VERSION=3.20.1
readonly FILE_URL=protobuf-${VERSION}.tar.gz
readonly PLATFORM=$1

. ../common.sh

readonly SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cmi_setup_cc ${HOST_PLATFORM}

echo off
set -e

export TMP_HOST=tmp_host
export TMP_TARGET=tmp_target

if [ "${PLATFORM}" == "arm64-macos" ]; then
    MACOS_ARCHS=arm64
fi
if [ "${PLATFORM}" == "x86_64-macos" ]; then
    MACOS_ARCHS=x86_64
fi


echo "**************************************************"
echo "BUILD HOST TOOLS"
echo "**************************************************"

export SOURCE_HOST=${SCRIPTDIR}/${TMP_HOST}/${PACKAGEDIR}
export INSTALL_HOST=${SCRIPTDIR}/install

mkdir -p ${INSTALL_HOST}
echo "Unpack to ${TMP_HOST}"

mkdir -p ${TMP_HOST}
tar xf ${FILE_URL} --directory ${TMP_HOST} --strip-components=1

echo "SOURCE_HOST:" ${SOURCE_HOST}
pushd ${SOURCE_HOST}

mkdir -p _build
pushd _build

    cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_HOST} \
        -DCMAKE_OSX_ARCHITECTURES=${MACOS_ARCHS} \
        -DCMAKE_C_COMPILER=${CC} \
        -DCMAKE_CXX_COMPILER=${CXX} \
        -DCMAKE_C_FLAGS="${FLAGS} ${CFLAGS}" \
        -DCMAKE_CXX_FLAGS="${FLAGS} ${CXXFLAGS}" \
        -Dprotobuf_BUILD_EXAMPLES=OFF \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_BUILD_CONFORMANCE=OFF \
        -Dprotobuf_BUILD_LIBPROTOC=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -Dprotobuf_DISABLE_RTTI=OFF \
        ../cmake
    cmake --build . --config Release

# _build
popd

# host
popd

echo "**************************************************"
echo "BUILD TARGET LIB for ${PLATFORM}"
echo "**************************************************"

unset CFLAGS
unset CXXFLAGS

cmi_setup_cc ${PLATFORM}

export SOURCE_TARGET=${SCRIPTDIR}/${TMP_TARGET}/${PACKAGEDIR}
export INSTALL_TARGET=${SCRIPTDIR}/install

mkdir -p ${INSTALL_TARGET}/${PLATFORM}

echo "Unpack to ${TMP_TARGET}"

mkdir -p ${TMP_TARGET}
tar xf ${FILE_URL} --directory ${TMP_TARGET} --strip-components=1

echo "SOURCE_TARGET:" ${SOURCE_TARGET}
pushd ${SOURCE_TARGET}

# To not have cmake cached compile flags/settings
# if [ -e ./_build ]; then
#     rm -rf ./_build
#     echo "Removed old dir"
# fi

mkdir -p _build
pushd _build

    export FLAGS="-fPIC"

    cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_TARGET} \
        -DCMAKE_INSTALL_LIBDIR="${INSTALL_TARGET}/${PLATFORM}" \
        -DCMAKE_OSX_ARCHITECTURES=${MACOS_ARCHS} \
        -DCMAKE_C_COMPILER=${CC} \
        -DCMAKE_CXX_COMPILER=${CXX} \
        -DCMAKE_C_COMPILER_WORKS=1 \
        -DCMAKE_CXX_COMPILER_WORKS=1 \
        -DCMAKE_C_FLAGS="${FLAGS} ${CFLAGS}" \
        -DCMAKE_CXX_FLAGS="${FLAGS} ${CXXFLAGS}" \
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
        -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
        -Dprotobuf_BUILD_EXAMPLES=OFF \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_BUILD_CONFORMANCE=OFF \
        -Dprotobuf_BUILD_LIBPROTOC=OFF \
        -Dprotobuf_BUILD_SHARED_LIBS=OFF \
        -Dprotobuf_DISABLE_RTTI=OFF \
        -DWITH_PROTOC=${SOURCE_HOST}/_build/Release/protoc \
        ../cmake

    cmake --build . --config Release --target libprotobuf -- -j8


# _build
popd

# target
popd


echo "**************************************************"
echo "Package protobuf for ${PLATFORM}"
echo "**************************************************"

PACKAGE_NAME=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz

case ${PLATFORM} in
    x86_64-macos|arm64-macos|x86_64-linux|arm64-linux|x86_64-win32)
        IS_DESKTOP=1
        ;;
esac

case ${PLATFORM} in
    x86_64-win32)
        SUFFIX=.exe
        ;;

    x86_64-macos|arm64-macos|x86_64-linux|arm64-linux)
        echo "Stripping executable"
        strip "${SOURCE_HOST}/_build/protoc"
        ;;
esac

mkdir -p package
pushd package

    mkdir -p lib/${PLATFORM}
    mkdir -p bin/${PLATFORM}

    cp -v "${SOURCE_TARGET}/_build/libprotobuf.a" lib/${PLATFORM}/
    if [ "${IS_DESKTOP}" != "" ]; then
        cp -v "${SOURCE_HOST}/_build/protoc${SUFFIX}" bin/${PLATFORM}/
    fi

    echo "Packaging '${PACKAGE_NAME}'..."
    tar cfvz ${PACKAGE_NAME} lib bin

    rm -rf lib bin

# package
popd

echo "Wrote ./package/${PACKAGE_NAME}"

rm -rf ${TMP_HOST}
rm -rf ${TMP_TARGET}
