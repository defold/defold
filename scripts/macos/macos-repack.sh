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
    echo "Usage: ${SCRIPT_NAME} <app>"
    echo "  app   - filepath to the app"
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

APP="${1:-}" && [ ! -z "${APP}" ] || terminate_usage

[ -d "${APP}" ] || terminate "App does not exist: ${APP}"


# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
trap 'terminate_trap' SIGHUP SIGINT SIGTERM EXIT

APP_DIR="$(dirname "$APP")"
EXEPATH=${APP}/Contents/MacOS/
PROJECTNAME=$(cd ${EXEPATH} && ls)
REPACK_APP=${APP_DIR}/${PROJECTNAME}.repack.app
REPACK_EXEPATH=${REPACK_APP}/Contents/MacOS/

cp -vr ${APP} ${REPACK_APP}
cp -v ${DYNAMO_HOME}/bin/x86_64-macos/dmengine ${REPACK_EXEPATH}/${PROJECTNAME}


# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${REPACK_APP}"
exit 0
