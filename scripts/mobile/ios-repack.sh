#!/usr/bin/env bash
# http://stackoverflow.com/questions/6896029/re-sign-ipa-iphone
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
    echo "Usage: ${SCRIPT_NAME} <platform> <identity> <profile> <source>"
    echo "  platform - 'armv7-darwin' or 'arm64-darwin'"
    echo "  identity - name of the iPhone developer identity"
    echo "  profile  - absolute filepath to the provisioning profile"
    echo "  source   - absolute filepath to the source ipa to repack"
    echo " "
    echo "Available identities:"
    security find-identity -v -p codesigning
    exit 1
}

function terminate_trap() {
    trap - SIGHUP SIGINT SIGTERM
    [ ! -z "${ROOT:-}" ] && rm -rf "${ROOT}"
    terminate "An unexpected error occurred."
}


# ----------------------------------------------------------------------------
# Script environment
# ----------------------------------------------------------------------------
[ ! -z "${DYNAMO_HOME:-}" ] || terminate "DYNAMO_HOME is not set"

PLATFORM="${1:-}" && [ ! -z "${PLATFORM}" ] || terminate_usage
IDENTITY="${2:-}" && [ ! -z "${IDENTITY}" ] || terminate_usage
PROFILE="${3:-}" && [ ! -z "${PROFILE}" ] || terminate_usage
SOURCE="${4:-}" && [ ! -z "${SOURCE}" ] || terminate_usage
[[ "armv7-darwin arm64-darwin" =~ "${PLATFORM}" ]] || terminate_usage

SOURCE="$(cd "$(dirname "${SOURCE}")"; pwd)/$(basename "${SOURCE}")"
PROFILE="$(cd "$(dirname "${PROFILE}")"; pwd)/$(basename "${PROFILE}")"

CODESIGN="codesign"
SECURITY="security"
PLISTBUDDY="/usr/libexec/PlistBuddy"
ZIP="zip"
UNZIP="unzip"
ENGINE="${DYNAMO_HOME:-}/bin/${PLATFORM:-}/dmengine"

[ $(which "${CODESIGN}") ] || terminate "'${CODESIGN}' is not installed"
[ $(which "${SECURITY}") ] || terminate "'${SECURITY}' is not installed"
[ $(which "${PLISTBUDDY}") ] || terminate "'${PLISTBUDDY}' is not installed"
[ $(which "${ZIP}") ] || terminate "'${ZIP}' is not installed"
[ $(which "${UNZIP}") ] || terminate "'${UNZIP}' is not installed"

[ -f "${SOURCE}" ] || terminate "Source does not exist: ${SOURCE}"
[ -f "${PROFILE}" ] || terminate "Profile does not exist: ${PROFILE}"
[ -f "${ENGINE}" ] || terminate "Engine does not exist: ${ENGINE}"


# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
ROOT="$(mktemp -d)"
trap 'terminate_trap' SIGHUP SIGINT SIGTERM EXIT

BUILD="${ROOT}/build"
PROVISION="${ROOT}/provision.plist"
ENTITLEMENT="${ROOT}/entitlement.plist"

APPLICATION="$(basename "${SOURCE}" ".ipa")"
TARGET="$(cd "$(dirname "${SOURCE}")"; pwd)/${APPLICATION}.repack"

mkdir -p "${BUILD}"
"${UNZIP}" "${SOURCE}" -d "${BUILD}" > /dev/null 2>&1

(
    cd "${BUILD}"

    "${SECURITY}" cms -D -i "${PROFILE}" > "${PROVISION}"
    "${PLISTBUDDY}" -x -c 'Print :Entitlements' "${PROVISION}" > "${ENTITLEMENT}"

    cp -v "${ENGINE}" "Payload/${APPLICATION}.app/${APPLICATION}"
    chmod +x "Payload/${APPLICATION}.app/${APPLICATION}"

    rm -rf "Payload/${APPLICATION}.app/_CodeSignature"
    cp "${PROVISION}" "Payload/${APPLICATION}.app/embedded.mobileprovision"
    "${CODESIGN}" -f -s "${IDENTITY}" --entitlements "${ENTITLEMENT}" "Payload/${APPLICATION}.app"

    "${ZIP}" -qr "${TARGET}.ipa" "Payload"
    [ -d "${TARGET}.app" ] && rm -rf "${TARGET}.app"
    cp -r "Payload/${APPLICATION}.app" "${TARGET}.app"
)

rm -rf "${ROOT}"


# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${TARGET}.ipa"
echo "Wrote ${TARGET}.app"
exit 0
