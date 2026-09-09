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
    echo "Usage: ${SCRIPT_NAME} <source> [<keystore> <keystore_pass>] [--reinstall]"
    echo "  source        - absolute filepath to the source apk to repack"
    echo "  keystore      - optional absolute filepath to the keystore"
    echo "  keystore pass - optional absolute filepath to the keystore password"
    echo "                  if omitted, a temporary debug keystore is generated"
    echo "  --reinstall   - uninstall installed package with same id and install repacked apk"
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
DEFOLD_HOME="$(cd "${SCRIPT_PATH}/../.."; pwd)"

eval "$(python3 "${DEFOLD_HOME}/build_tools/set_sdk_vars.py" ANDROID_NDK_VERSION)"
if [ -z "${ANDROID_BUILD_TOOLS_VERSION:-}" ]; then
    eval "$(python3 "${DEFOLD_HOME}/build_tools/set_sdk_vars.py" ANDROID_BUILD_TOOLS_VERSION)"
fi

case "$(uname -s)" in
    Darwin) PLATFORM="darwin-x86_64" ;;
    Linux) PLATFORM="linux-x86_64" ;;
    *) terminate "ASAN repacking requires a macOS or Linux host" ;;
esac
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-${DYNAMO_HOME}/ext/SDKs/android-ndk-r${ANDROID_NDK_VERSION}}}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-${DYNAMO_HOME}/ext/SDKs/android-sdk}}"

[ -d "${ANDROID_NDK_ROOT}" ] || terminate "ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT} does not exist"
[ -d "${ANDROID_SDK_ROOT}" ] || terminate "ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT} does not exist"

SOURCE="${1:-}" && [ ! -z "${SOURCE}" ] || terminate_usage
SOURCE="$(cd "$(dirname "${SOURCE}")"; pwd)/$(basename "${SOURCE}")"
KEYSTORE=""
KEYSTORE_PASS=""
REINSTALL=0

shift
while [ $# -gt 0 ]; do
    case "${1}" in
        --reinstall)
            REINSTALL=1
            ;;
        *)
            if [ -z "${KEYSTORE}" ]; then
                KEYSTORE="${1}"
            elif [ -z "${KEYSTORE_PASS}" ]; then
                KEYSTORE_PASS="${1}"
            else
                terminate_usage
            fi
            ;;
    esac
    shift
done

if [ -z "${KEYSTORE}" ] && [ ! -z "${KEYSTORE_PASS}" ]; then
    terminate_usage
fi

if [ ! -z "${KEYSTORE}" ] && [ -z "${KEYSTORE_PASS}" ]; then
    terminate_usage
fi

ZIP="zip"
UNZIP="unzip"
ZIPALIGN="${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/zipalign"
APKSIGNER="${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/apksigner"
AAPT2="${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/aapt2"
KEYTOOL="keytool"
ADB="${ANDROID_SDK_ROOT}/platform-tools/adb"
GDBSERVER=${ANDROID_NDK_ROOT}/prebuilt/android-arm/gdbserver/gdbserver

OBJDUMP=${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${PLATFORM}/bin/llvm-objdump

[ $(which "${ZIP}") ] || terminate "'${ZIP}' is not installed"
[ $(which "${UNZIP}") ] || terminate "'${UNZIP}' is not installed"
[ $(which "${ZIPALIGN}") ] || terminate "'${ZIPALIGN}' is not installed"
[ $(which "${APKSIGNER}") ] || terminate "'${APKSIGNER}' is not installed"
[ $(which "${AAPT2}") ] || terminate "'${AAPT2}' is not installed"
[ $(which "${KEYTOOL}") ] || terminate "'${KEYTOOL}' is not installed"
[ $(which "${OBJDUMP}") ] || terminate "'${OBJDUMP}' is not installed"

[ -f "${SOURCE}" ] || terminate "Source does not exist: ${SOURCE}"

function should_store_native_libs() {
    "${AAPT2}" dump xmltree --file AndroidManifest.xml "${SOURCE}" | grep -q 'extractNativeLibs.*=false'
}

function create_debug_keystore() {
    local keystore="${1}"
    local keystore_pass="${2}"
    local keystore_password="android"

    echo "No keystore specified. Generating temporary debug keystore."
    "${KEYTOOL}" \
        -genkey \
        -v \
        -noprompt \
        -dname "CN=Android, OU=Debug, O=Android, L=Unknown, ST=Unknown, C=US" \
        -keystore "${keystore}" \
        -storepass "${keystore_password}" \
        -alias "androiddebugkey" \
        -keyalg "RSA" \
        -validity "14000" > /dev/null
    printf "%s" "${keystore_password}" > "${keystore_pass}"
}

function get_package_name() {
    "${AAPT2}" dump badging "${1}" | sed -n "s/^package: name='\\([^']*\\)'.*/\\1/p" | head -n 1
}

function reinstall_package() {
    local target_apk="${1}"
    local package_name="$(get_package_name "${target_apk}")"
    local package_status

    [ -n "${package_name}" ] || terminate "Failed to determine package name from ${target_apk}"
    [ -x "${ADB}" ] || terminate "adb does not exist: ${ADB}"

    package_status="$("${ADB}" shell pm path "${package_name}" 2>/dev/null || true)"
    if printf "%s" "${package_status}" | grep -q '^package:'; then
        echo "Uninstalling ${package_name}"
        "${ADB}" uninstall "${package_name}" || terminate "Failed to uninstall ${package_name}"
    else
        echo "Package ${package_name} is not installed"
    fi

    echo "Installing ${package_name}"
    "${ADB}" install "${target_apk}" || terminate "Failed to install ${target_apk}"
}


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

if [ -z "${KEYSTORE}" ]; then
    KEYSTORE="${ROOT}/debug.keystore"
    KEYSTORE_PASS="${ROOT}/debug.keystore.pass.txt"
    create_debug_keystore "${KEYSTORE}" "${KEYSTORE_PASS}"
else
    [ -f "${KEYSTORE}" ] || terminate "Keystore does not exist: ${KEYSTORE}"
    [ -f "${KEYSTORE_PASS}" ] || terminate "Keystore password file does not exist: ${KEYSTORE_PASS}"
    KEYSTORE="$(cd "$(dirname "${KEYSTORE}")"; pwd)/$(basename "${KEYSTORE}")"
    KEYSTORE_PASS="$(cd "$(dirname "${KEYSTORE_PASS}")"; pwd)/$(basename "${KEYSTORE_PASS}")"
fi

WRAP_ASAN=${SCRIPT_PATH}/android-wrap-asan.sh

"${UNZIP}" -q "${SOURCE}" -d "${BUILD}"
(
    cd "${BUILD}"

    for abi in armeabi-v7a arm64-v8a x86_64; do
        [ -d "lib/${abi}" ] || continue
        for file in "lib/${abi}/"*.so; do
            [ -f "${file}" ] || continue
            ASAN_DEPENDENCY=$("${OBJDUMP}" -p "${file}" | awk '/NEEDED.*libclang_rt.asan/ {print $2}')
            if [ -n "${ASAN_DEPENDENCY}" ]; then
                ASAN_PATH=$(find "${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${PLATFORM}/lib" -name "${ASAN_DEPENDENCY}")
                [ -f "${ASAN_PATH}" ] || terminate "Matching ASAN runtime not found: ${ASAN_DEPENDENCY}"
                echo "Found ASAN dependency in ${file}"
                cp -v "${ASAN_PATH}" "lib/${abi}/"
                cp -v "${WRAP_ASAN}" "lib/${abi}/wrap.sh"
                break
            fi
        done
    done

    rm -rf "META-INF"

    if [ -e "$GDBSERVER" ]; then
        cp -v "${ANDROID_NDK_ROOT}/prebuilt/android-arm/gdbserver/gdbserver" ./lib/armeabi-v7a/gdbserver
    fi

    STORED_FILES=("resources.arsc")
    if should_store_native_libs; then
        while IFS= read -r file; do
            STORED_FILES+=("${file#./}")
        done < <(find ./lib -type f -name '*.so' | sort)
    fi

    ZIP_EXCLUDES=()
    for file in "${STORED_FILES[@]}"; do
        ZIP_EXCLUDES+=(-x "${file}")
    done

    "${ZIP}" -qr "${REPACKZIP}" "." "${ZIP_EXCLUDES[@]}"
    # Targeting R+ (version 30 and above) requires the resources.arsc of installed
    # APKs to be stored uncompressed and aligned on a 4-byte boundary
    "${ZIP}" -q -0 "${REPACKZIP}" "${STORED_FILES[@]}"
)

"${ZIPALIGN}" -p -v 4 "${REPACKZIP}" "${REPACKZIP_ALIGNED}" > /dev/null 2>&1

"${APKSIGNER}" sign --verbose --in="${REPACKZIP_ALIGNED}" --out="${TARGET}.apk" --ks "${KEYSTORE}" --ks-pass file:"${KEYSTORE_PASS}"

if [ "${REINSTALL}" = "1" ]; then
    reinstall_package "${TARGET}.apk"
fi

rm -rf "${ROOT}"


# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${TARGET}.apk"
exit 0
