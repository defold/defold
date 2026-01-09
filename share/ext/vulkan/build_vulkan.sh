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



set -e

readonly PLATFORM=$1
readonly PRODUCT=vulkan
readonly VERSION=v1.4.307
readonly PACKAGE_NAME=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz
readonly HEADERS_PACKAGE_NAME=${PRODUCT}-${VERSION}-common.tar.gz

readonly LOADER_URL=https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/${VERSION}.tar.gz
readonly HEADERS_URL=https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${VERSION}.tar.gz
readonly VALIDATION_URL=https://github.com/KhronosGroup/Vulkan-ValidationLayers/archive/refs/tags/vulkan-sdk-1.4.304.0.tar.gz

. ../common.sh

readonly SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly BUILDDIR=$(realpath ./build/${PLATFORM})
readonly INSTALLDIR=${BUILDDIR}/install

readonly LOADERDIR=$(realpath ./loader)
readonly HEADERSDIR=$(realpath ./headers)
readonly VALIDATIONDIR=$(realpath ./validation)

mkdir -p ${BUILDDIR}
mkdir -p ${BUILDDIR}/lib/$PLATFORM

OSX_MIN_SDK_VERSION=10.15
CMAKE_VALIDATION_FLAGS=

cmi_setup_cc $PLATFORM

function download {
    local url=$1
    local filename=$2
    local output=$3

    echo "**************************************************"
    echo "Download ${url}  ->  ${filename}"
    echo "**************************************************"

    if [ ! -e "${filename}" ]; then
        echo "${filename} not found. Downloading ${url} ..."
        wget ${url} -O ${filename}
    else
        echo "Found ${filename}"
    fi

    echo "**************************************************"
    echo "Extracting ${filename}  ->  ${output}"
    echo "**************************************************"

    if [ ! -d "${output}" ]; then
        echo "${output} not found. Extracting source"
        mkdir -p ${output}
        tar --strip-components=1 -xz -C ${output} -f ${filename}
        echo "Done."
    else
        echo "Found ${output}"
    fi
}

case $PLATFORM in
    arm64-macos)
        CMAKE_VALIDATION_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 ${CMAKE_VALIDATION_FLAGS}"
        CMAKE_VALIDATION_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_VALIDATION_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_VALIDATION_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 ${CMAKE_VALIDATION_FLAGS}"
        CMAKE_VALIDATION_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_VALIDATION_FLAGS}"
        ;;
esac

download ${LOADER_URL} loader.tar.gz ${LOADERDIR}
download ${VALIDATION_URL} validation.tar.gz ${VALIDATIONDIR}
download ${HEADERS_URL} headers.tar.gz ${HEADERSDIR}

echo "**************************************************"
echo "BUILD TARGET LIB for ${PLATFORM}"
echo "**************************************************"

readonly HEADERSINSTALLDIR=${BUILDDIR}/headers

# extra platform specific flags
#-DCMAKE_OSX_ARCHITECTURES=${MACOS_ARCHS} \

mkdir -p ${BUILDDIR}/headers
pushd ${BUILDDIR}/headers

    cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=${CC} \
        -DCMAKE_CXX_COMPILER=${CXX} \
        -DCMAKE_C_FLAGS="${FLAGS} ${CFLAGS}" \
        -DCMAKE_CXX_FLAGS="${FLAGS} ${CXXFLAGS}" \
        ${HEADERSDIR}
    cmake --build . --config Release
    cmake --install . --config Release --prefix ${INSTALLDIR}
popd


mkdir -p ${BUILDDIR}/loader
pushd ${BUILDDIR}/loader

    cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=${CC} \
        -DCMAKE_CXX_COMPILER=${CXX} \
        -DCMAKE_C_FLAGS="${FLAGS} ${CFLAGS}" \
        -DUPDATE_DEPS=ON \
        -DBUILD_TESTS=OFF \
        ${LOADERDIR}
    cmake --build . --config Release
    cmake --install . --config Release --prefix ${INSTALLDIR}

popd


mkdir -p ${BUILDDIR}/validation
pushd ${BUILDDIR}/validation

    cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=${CC} \
        -DCMAKE_CXX_COMPILER=${CXX} \
        -DCMAKE_C_FLAGS="${FLAGS} ${CFLAGS}" \
        -DUPDATE_DEPS=ON \
        -DBUILD_TESTS=OFF \
        ${CMAKE_VALIDATION_FLAGS} \
        ${VALIDATIONDIR}
    cmake --build . --config Release
    cmake --install . --config Release --prefix ${INSTALLDIR}
popd


echo "**************************************************"
echo "package ${PACKAGE_NAME}"
echo "**************************************************"

mkdir -p package
pushd package

    mkdir -p lib/${PLATFORM}
    mkdir -p bin/${PLATFORM}
    mkdir -p include/${PLATFORM}

    case ${PLATFORM} in
        *-linux)
            cp -v ${BUILDDIR}/install/lib/lib*.so* lib/${PLATFORM}/
            ;;
        *-macos)
            cp -v ${BUILDDIR}/install/lib/lib*.dylib* lib/${PLATFORM}/
            ;;
        *)
            cp -v ${BUILDDIR}/install/lib/lib*.a lib/${PLATFORM}/
            ;;
    esac

    tar cfvz ${PACKAGE_NAME} lib bin

    rm -rf lib bin

# package
popd

echo "Wrote ./package/${PACKAGE_NAME}"


echo "**************************************************"
echo "package ${HEADERS_PACKAGE_NAME}"
echo "**************************************************"

mkdir -p package
pushd package

    mkdir -p include

    cp -v -r ${BUILDDIR}/install/include ./

    tar cfvz ${HEADERS_PACKAGE_NAME} include

    rm -rf include

# package
popd

echo "Wrote ./package/${HEADERS_PACKAGE_NAME}"

echo "**************************************************"
echo "Cleanup"
echo "**************************************************"

#rm -rf ${BUILDDIR}/loader
#rm -rf ${BUILDDIR}/validation
#rm -rf ${BUILDDIR}/headers
