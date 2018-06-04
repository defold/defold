#!/usr/bin/env bash
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
    echo "Usage: ${SCRIPT_NAME} <source> [<certificate>, <key>]"
    echo "  source       - absolute filepath to the source apk to repack"
    echo "  certificate  - (optional) absolute filepath to the certificate file (pem)"
    echo "  key          - (optional) absolute filepath to the keyfile (pk8)"
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

ANDROID_ROOT=~/android
ANDROID_NDK_VERSION=10e
ANDROID_NDK="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}"

[ -d "${ANDROID_ROOT}" ] || terminate "ANDROID_ROOT=${ANDROID_ROOT} does not exist"
[ -d "${ANDROID_NDK}" ] || terminate "ANDROID_NDK=${ANDROID_NDK} does not exist"

SOURCE="${1:-}" && [ ! -z "${SOURCE}" ] || terminate_usage
SOURCE="$(cd "$(dirname "${SOURCE}")"; pwd)/$(basename "${SOURCE}")"
CERTIFICATE="${2:-}"
KEYFILE="${3:-}"

ZIP="zip"
UNZIP="unzip"
ZIPALIGN="${DEFOLD_HOME}/com.dynamo.cr/com.dynamo.cr.bob/libexec/x86-darwin/zipalign"
APKC="${DEFOLD_HOME}/com.dynamo.cr/com.dynamo.cr.bob/libexec/x86-darwin/apkc"

ENGINE_LIB="${DYNAMO_HOME}/bin/armv7-android/libdmengine.so"
ENGINE_DEX="${DYNAMO_HOME}/share/java/classes.dex"

[ $(which "${ZIP}") ] || terminate "'${ZIP}' is not installed"
[ $(which "${UNZIP}") ] || terminate "'${UNZIP}' is not installed"
[ $(which "${ZIPALIGN}") ] || terminate "'${ZIPALIGN}' is not installed"
[ $(which "${APKC}") ] || terminate "'${APKC}' is not installed"

[ -f "${SOURCE}" ] || terminate "Source does not exist: ${SOURCE}"
[ -f "${ENGINE_LIB}" ] || terminate "Engine does not exist: ${ENGINE_LIB}"
[ -f "${ENGINE_DEX}" ] || terminate "Engine does not exist: ${ENGINE_DEX}"
if [ ! -z "${CERTIFICATE}" ] || [ ! -z "${KEYFILE}" ]; then
    [ ! -z "${KEYFILE}" ] || terminate "Keyfile required if certificate is specified."
    [ -f "${CERTIFICATE}" ] || terminate "Certificate does not exist: ${CERTIFICATE}"
    [ -f "${KEYFILE}" ] || terminate "Keyfile does not exist: ${KEYFILE}"

    CERTIFICATE="$(cd "$(dirname "${CERTIFICATE}")"; pwd)/$(basename "${CERTIFICATE}")"
    KEYFILE="$(cd "$(dirname "${KEYFILE}")"; pwd)/$(basename "${KEYFILE}")"
fi


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

"${UNZIP}" "${SOURCE}" -d "${BUILD}" > /dev/null 2>&1
(
    cd "${BUILD}"

    rm -rf "META-INF"
    cp -v "${ENGINE_LIB}" "lib/armeabi-v7a/lib${APPLICATION}.so"
    cp -v "${ENGINE_DEX}" "classes.dex"

    cp -v "${ANDROID_NDK}/prebuilt/android-arm/gdbserver/gdbserver" ./lib/armeabi-v7a/gdbserver

    ${ZIP} -qr "${REPACKZIP}" "."
)

"${ZIPALIGN}" -v 4 "${REPACKZIP}" "${REPACKZIP_ALIGNED}" > /dev/null 2>&1

if [ ! -z "${CERTIFICATE}" ]; then
    "${APKC}" --in="${REPACKZIP_ALIGNED}" --out="${TARGET}.apk" --cert="${CERTIFICATE}" --key="${KEYFILE}" > /dev/null 2>&1
else
    "${APKC}" --in="${REPACKZIP_ALIGNED}" --out="${TARGET}.apk" > /dev/null 2>&1
fi

rm -rf "${ROOT}"


# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${TARGET}.apk"
exit 0
