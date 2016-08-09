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
    echo "Usage: ${SCRIPT_NAME} <source>"
    echo "  source   - absolute filepath to the source apk to repack"
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

SOURCE="${1:-}" && [ ! -z "${SOURCE}" ] || terminate_usage

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
    cp "${ENGINE_LIB}" "lib/armeabi-v7a/lib${APPLICATION}.so"
    cp "${ENGINE_DEX}" "classes.dex"

    ${ZIP} -qr "${REPACKZIP}" "."
)

"${ZIPALIGN}" -v 4 "${REPACKZIP}" "${REPACKZIP_ALIGNED}" > /dev/null 2>&1
"${APKC}" --in="${REPACKZIP_ALIGNED}" --out="${TARGET}.apk" > /dev/null 2>&1

rm -rf "${ROOT}"


# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${TARGET}.apk"
exit 0
