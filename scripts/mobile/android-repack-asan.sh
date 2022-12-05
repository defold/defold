#!/usr/bin/env bash
# Copyright 2020-2022 The Defold Foundation
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


# Mathias Westerdahl, Jakob Pogulis
set -eu

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"


# ----------------------------------------------------------------------------
# Script functions
# ----------------------------------------------------------------------------
function terminate() {
    echo "TERM - ${1:-}" && exit ${2:-1}
}

function terminate_usage() {
    echo "Usage: ${SCRIPT_NAME} <source> <keystore> <keystore_pass>"
    echo "  source        - absolute filepath to the source apk to repack"
    echo "  keystore      - absolute filepath to the keystore"
    echo "  keystore pass - absolute filepath to the keystore password"
    exit 1
}

function terminate_trap() {
    trap - SIGHUP SIGINT SIGTERM
    find "${ROOT}"
    [ ! -z "${ROOT:-}" ] && rm -rf "${ROOT}"
    terminate "An unexpected error occurred."
}


# ----------------------------------------------------------------------------
# Script environment
# ----------------------------------------------------------------------------
[ ! -z "${DYNAMO_HOME:-}" ] || terminate "DYNAMO_HOME is not set"
DEFOLD_HOME="$(cd "${DYNAMO_HOME}/../.."; pwd)"

ANDROID_NDK_VERSION=20
ANDROID_BUILD_TOOLS_VERSION="32.0.0"
PLATFORM="darwin-x86_64"
ANDROID_NDK_ROOT="${DYNAMO_HOME}/ext/SDKs/android-ndk-r${ANDROID_NDK_VERSION}"
ANDROID_SDK_ROOT="${DYNAMO_HOME}/ext/SDKs/android-sdk"

[ -d "${ANDROID_NDK_ROOT}" ] || terminate "ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT} does not exist"
[ -d "${ANDROID_SDK_ROOT}" ] || terminate "ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT} does not exist"

SOURCE="${1:-}" && [ ! -z "${SOURCE}" ] || terminate_usage
SOURCE="$(cd "$(dirname "${SOURCE}")"; pwd)/$(basename "${SOURCE}")"
KEYSTORE="${2:-}" && [ ! -z "${KEYSTORE}" ] || terminate_usage
KEYSTORE_PASS="${3:-}" && [ ! -z "${KEYSTORE_PASS}" ] || terminate_usage

ZIP="zip"
UNZIP="unzip"
ZIPALIGN="${DEFOLD_HOME}/com.dynamo.cr/com.dynamo.cr.bob/libexec/x86_64-macos/zipalign"
APKSIGNER="${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/apksigner"
GDBSERVER=${ANDROID_NDK_ROOT}/prebuilt/android-arm/gdbserver/gdbserver

OBJDUMP_32=${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${PLATFORM}/bin/arm-linux-androideabi-objdump
OBJDUMP_64=${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${PLATFORM}/bin/aarch64-linux-android-objdump

[ $(which "${ZIP}") ] || terminate "'${ZIP}' is not installed"
[ $(which "${UNZIP}") ] || terminate "'${UNZIP}' is not installed"
[ $(which "${ZIPALIGN}") ] || terminate "'${ZIPALIGN}' is not installed"
[ $(which "${APKSIGNER}") ] || terminate "'${APKSIGNER}' is not installed"
[ $(which "${OBJDUMP_32}") ] || terminate "'${OBJDUMP_32}' is not installed"
[ $(which "${OBJDUMP_64}") ] || terminate "'${OBJDUMP_64}' is not installed"

[ -f "${SOURCE}" ] || terminate "Source does not exist: ${SOURCE}"
[ -f "${KEYSTORE}" ] || terminate "Keystore does not exist: ${KEYSTORE}"
[ -f "${KEYSTORE_PASS}" ] || terminate "Keystore password file does not exist: ${KEYSTORE_PASS}"

KEYSTORE="$(cd "$(dirname "${KEYSTORE}")"; pwd)/$(basename "${KEYSTORE}")"
KEYSTORE_PASS="$(cd "$(dirname "${KEYSTORE_PASS}")"; pwd)/$(basename "${KEYSTORE_PASS}")"


# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
ROOT="$(mktemp -d)"
trap 'terminate_trap' SIGHUP SIGINT SIGTERM EXIT

BUILD="${ROOT}/build"
REPACKZIP="${ROOT}/repack.zip"
REPACKZIP_ALIGNED="${ROOT}/repack.aligned.zip"

APPLICATION="$(basename "${SOURCE}" ".apk")"
TARGET="$(cd "$(dirname "${SOURCE}")"; pwd)/${APPLICATION}.repack"

ASAN_PATH_32=$(find ${ANDROID_NDK_ROOT} -iname "libclang_rt.asan-arm-android.so")
ASAN_PATH_64=$(find ${ANDROID_NDK_ROOT} -iname "libclang_rt.asan-aarch64-android.so")

WRAP_ASAN=${SCRIPT_PATH}/android-wrap-asan.sh

"${UNZIP}" -q "${SOURCE}" -d "${BUILD}"
(
    cd "${BUILD}"

    for file in `ls ./lib/armeabi-v7a/*.so`; do
        ASAN_DEPENDENCY=$(${OBJDUMP_32} -p ${file} | grep NEEDED | grep libclang_rt.asan | awk '{print $2;}')
        if [ "$ASAN_DEPENDENCY" != "" ]; then
            echo "Found ASAN dependency in $file"
            cp -v ${ASAN_PATH_32} "lib/armeabi-v7a/"
            echo "Copying wrapper script"
            cp -v ${WRAP_ASAN} "lib/armeabi-v7a/wrap.sh"
            break
        fi
    done

    for file in `ls ./lib/arm64-v8a/*.so`; do
        ASAN_DEPENDENCY=$(${OBJDUMP_64} -p ${file} | grep NEEDED | grep libclang_rt.asan | awk '{print $2;}')
        if [ "$ASAN_DEPENDENCY" != "" ]; then
            echo "Found ASAN dependency in $file"
            cp -v ${ASAN_PATH_64} "lib/arm64-v8a/"
            echo "Copying wrapper script"
            cp -v ${WRAP_ASAN} "lib/arm64-v8a/wrap.sh"
            break
        fi
    done

    rm -rf "META-INF"

    if [ -e "$GDBSERVER" ]; then
        cp -v "${ANDROID_NDK_ROOT}/prebuilt/android-arm/gdbserver/gdbserver" ./lib/armeabi-v7a/gdbserver
    fi


    ${ZIP} -qr "${REPACKZIP}" "." -x "resources.arsc"
    # Targeting R+ (version 30 and above) requires the resources.arsc of installed
    # APKs to be stored uncompressed and aligned on a 4-byte boundary
    ${ZIP} -q -Z store "${REPACKZIP}" "resources.arsc"
)

"${ZIPALIGN}" -v 4 "${REPACKZIP}" "${REPACKZIP_ALIGNED}" > /dev/null 2>&1

"${APKSIGNER}" sign --verbose --in="${REPACKZIP_ALIGNED}" --out="${TARGET}.apk" --ks "${KEYSTORE}" --ks-pass file:"${KEYSTORE_PASS}"

rm -rf "${ROOT}"


# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${TARGET}.apk"
exit 0
