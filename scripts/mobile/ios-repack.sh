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
    echo "  platform - 'arm64-ios'"
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
[[ "arm64-ios" =~ "${PLATFORM}" ]] || terminate_usage

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

ASAN_PATH=${DYNAMO_HOME}/ext/SDKs/XcodeDefault.xctoolchain/usr/lib/clang/9.0.0/lib/darwin
# e.g. libclang_rt.asan_ios_dynamic.dylib

ASAN_DEPENDENCY=$(otool -L ${ENGINE} | grep libclang_rt.asan | awk '{print $1;}')
ASAN=""
if [ "$ASAN_DEPENDENCY" != "" ]; then
    ASAN=$(basename $ASAN_DEPENDENCY)
fi

# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
ROOT="$(mktemp -d)"
trap 'terminate_trap' SIGHUP SIGINT SIGTERM EXIT

BUILD="${ROOT}/build"
PROVISION="${ROOT}/provision.plist"
ENTITLEMENT="${ROOT}/entitlement.plist"

mkdir -p "${BUILD}"
"${UNZIP}" "${SOURCE}" -d "${BUILD}" > /dev/null 2>&1

APPLICATION="$(cd ${BUILD}/Payload && find . -iname "*.app")"
APPLICATION="$(basename "${APPLICATION}" ".app")"
TARGET="$(cd "$(dirname "${SOURCE}")"; pwd)/${APPLICATION}.repack"

(
    cd "${BUILD}"

    "${SECURITY}" cms -D -i "${PROFILE}" > "${PROVISION}"
    "${PLISTBUDDY}" -x -c 'Print :Entitlements' "${PROVISION}" > "${ENTITLEMENT}"

    EXENAME=$(defaults read ${BUILD}/Payload/${APPLICATION}.app/Info.plist CFBundleExecutable)

    cp -v "${ENGINE}" "Payload/${APPLICATION}.app/${EXENAME}"
    chmod +x "Payload/${APPLICATION}.app/${EXENAME}"

    rm -rf "Payload/${APPLICATION}.app/_CodeSignature"
    cp "${PROFILE}" "Payload/${APPLICATION}.app/embedded.mobileprovision"

    if [ -d  Payload/${APPLICATION}.app/Frameworks ]; then
        for name in Payload/${APPLICATION}.app/Frameworks/*
        do
            name=$(basename $name)
            "${CODESIGN}" -f -s "${IDENTITY}" --verbose "Payload/${APPLICATION}.app/Frameworks/$name"
        done
    fi

    EMBEDDED_PROFILE_NAME="embedded.mobileprovision" "${CODESIGN}" -f -s "${IDENTITY}" --entitlements "${ENTITLEMENT}" "Payload/${APPLICATION}.app"

    if [ "$ASAN" != "" ]; then
        cp -v "${ASAN_PATH}/${ASAN}" "Payload/${APPLICATION}.app/${ASAN}"
        "${CODESIGN}" -f -s "${IDENTITY}" --entitlements "${ENTITLEMENT}" --timestamp=none "Payload/${APPLICATION}.app/${ASAN}"
    fi

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
