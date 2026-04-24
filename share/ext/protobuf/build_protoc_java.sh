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

readonly PRODUCT=protobuf-java-protoc
readonly VERSION=34.0
readonly PLATFORM=$1
readonly BASE_URL=https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}

case ${PLATFORM} in
    arm64-linux)
        ARCHIVE=protoc-${VERSION}-linux-aarch_64.zip
        ;;
    x86_64-linux)
        ARCHIVE=protoc-${VERSION}-linux-x86_64.zip
        ;;
    arm64-macos)
        ARCHIVE=protoc-${VERSION}-osx-aarch_64.zip
        ;;
    x86_64-macos)
        ARCHIVE=protoc-${VERSION}-osx-x86_64.zip
        ;;
    win32)
        ARCHIVE=protoc-${VERSION}-win32.zip
        SUFFIX=.exe
        ;;
    x86_64-win32)
        ARCHIVE=protoc-${VERSION}-win64.zip
        SUFFIX=.exe
        ;;
    *)
        echo "Unsupported platform: ${PLATFORM}"
        exit 1
        ;;
esac

readonly SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly DOWNLOAD_DIR=${SCRIPTDIR}/download
readonly TMP_DIR=${SCRIPTDIR}/tmp_protoc_java
readonly PACKAGE_DIR=${SCRIPTDIR}/package
readonly PACKAGE_NAME=${PRODUCT}-${VERSION}-${PLATFORM}.tar.gz

mkdir -p "${DOWNLOAD_DIR}" "${PACKAGE_DIR}"

if [ ! -f "${DOWNLOAD_DIR}/${ARCHIVE}" ]; then
    curl -L -o "${DOWNLOAD_DIR}/${ARCHIVE}" "${BASE_URL}/${ARCHIVE}"
fi

rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}/bin/${PLATFORM}"

unzip -q "${DOWNLOAD_DIR}/${ARCHIVE}" "bin/protoc${SUFFIX}" -d "${TMP_DIR}/unpacked"
cp -v "${TMP_DIR}/unpacked/bin/protoc${SUFFIX}" "${TMP_DIR}/bin/${PLATFORM}/protoc-java${SUFFIX}"
chmod +x "${TMP_DIR}/bin/${PLATFORM}/protoc-java${SUFFIX}"

pushd "${TMP_DIR}" >/dev/null
    tar cfvz "${PACKAGE_DIR}/${PACKAGE_NAME}" bin
popd >/dev/null

rm -rf "${TMP_DIR}"

echo "Wrote ${PACKAGE_DIR}/${PACKAGE_NAME}"
