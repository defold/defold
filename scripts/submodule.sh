#!/usr/bin/env bash
# Copyright 2020-2024 The Defold Foundation
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

# ----------------------------------------------------------------------------
# basics
# ----------------------------------------------------------------------------
SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"
DEFOLD_PATH="$(cd "${SCRIPT_PATH}/.."; pwd)"
trap 'terminate 1 "${SCRIPT_NAME}: aborted."' SIGINT SIGTERM

# ----------------------------------------------------------------------------
# functions
# ----------------------------------------------------------------------------
function terminate() {
    echo "${2:-${SCRIPT_NAME}: terminated}"
    exit ${1:-0}
}


# ----------------------------------------------------------------------------
# arguments
# ----------------------------------------------------------------------------
PLATFORM=""
OPTIONS=""
SUBMODULES=""
if [ "$#" -gt 0 ]; then
    PLATFORM="$1"
    shift
fi
while [ "$#" -gt 0 ]; do
    if [ "$1" = "--" ]; then
        shift
        OPTIONS="$OPTIONS $@"
        break
    elif echo $1 | grep -q '^--'; then
        OPTIONS="$OPTIONS $1"
    else
        SUBMODULES="$SUBMODULES $1"
    fi
    shift
done


if [ -z "$SUBMODULES" ] || [ -z "$PLATFORM" ]; then
    terminate 1 "Usage: ${SCRIPT_NAME} <platform> <module> [, <module> .. ]"
fi
for _SUBMODULE in ${SUBMODULES}; do
    if [ ! -d "${DEFOLD_PATH}/engine/${_SUBMODULE}" ]; then
        terminate 1 "Submodule '${_SUBMODULE}' does not exist."
    fi
done

# ----------------------------------------------------------------------------
# script
# ----------------------------------------------------------------------------
[ "${DYNAMO_HOME:-}" != "" ] || terminate 1 "DYNAMO_HOME is not set."
[ -d "${DYNAMO_HOME}" ] || \
    terminate 1 "DYNAMO_HOME (${DYNAMO_HOME}) is not a directory."

start_time="$(date -u +%s)"
for _SUBMODULE in ${SUBMODULES}; do
    (
        cd "${DEFOLD_PATH}/engine/${_SUBMODULE}"
        waf install --platform="${PLATFORM}" ${OPTIONS} --prefix="${DYNAMO_HOME}" --skip-codesign --skip-tests --skip-build-tests
    )
done

(
    cd "${DEFOLD_PATH}/engine/engine"
    find "build" -type f -name "*dmengine*" | xargs -I% rm -f "%"
    find "build" -type f -name "classes.dex" | xargs -I% rm -f "%"
    find "build" -type d -name "*dmengine*" | xargs -I% rm -rf "%"
    waf install --platform="${PLATFORM}" ${OPTIONS} --prefix="${DYNAMO_HOME}" --skip-codesign --skip-tests --skip-build-tests
)


# ----------------------------------------------------------------------------
# teardown
# ----------------------------------------------------------------------------
end_time="$(date -u +%s)"
elapsed="$(($end_time-$start_time))"
echo "Built $(echo $SUBMODULES | wc -w) submodule(s) for platform '${PLATFORM}' in ${elapsed} seconds"
terminate 0 "${SCRIPT_NAME} done."
