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


# License: MIT
# Copyright 2022 The Defold Foundation

PRODUCT=lipo
PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

# Follow the build instructions on https://github.com/konoui/lipo
if [ ! -d "$SOURCE_DIR" ]; then
    git clone https://github.com/konoui/lipo $SOURCE_DIR
fi

if [ -d "${SOURCE_DIR}/bin" ]; then
    (cd ${SOURCE_DIR} && make clean)
fi

# Build

mkdir -p ${BUILD_DIR}

if [ -d "${BUILD_DIR}/bin" ]; then
    rm -rf ${BUILD_DIR}/bin
fi

pushd $BUILD_DIR

case $PLATFORM in
    x86_64-macos)
        export GOOS=darwin
        export GOARCH=amd64
        ;;
    arm64-macos)
        export GOOS=darwin
        export GOARCH=arm64
        ;;
    x86_64-win32)
        export GOOS=windows
        export GOARCH=amd64
        ;;
    x86_64-linux)
        export GOOS=linux
        export GOARCH=amd64
        ;;
    arm64-linux)
        export GOOS=linux
        export GOARCH=arm64
        ;;
    *)
        ;;
esac

(cd ${SOURCE_DIR} && make -j8)

BIN_DIR=${BUILD_DIR}/bin/${PLATFORM}
mkdir -p ${BIN_DIR}

case $PLATFORM in
    win32|x86_64-win32)
        mv ${SOURCE_DIR}/bin/lipo ${BIN_DIR}/lipo.exe
        ;;
    *)
        mv ${SOURCE_DIR}/bin/lipo ${BIN_DIR}/lipo
        ;;
esac

popd

# Package

VERSION=$(cd $SOURCE_DIR && git rev-parse --short HEAD)
echo VERSION=${VERSION}

PACKAGE=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz

pushd $BUILD_DIR
tar cfvz ${PACKAGE} bin
popd

mv ${BUILD_DIR}/${PACKAGE} ${DEFOLD_HOME}/packages/${PACKAGE}
echo "Wrote ${DEFOLD_HOME}/packages/${PACKAGE}"
