#!/usr/bin/env bash
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
    echo "  source   - filepath to the web bundle folder"
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

SOURCE="${1:-}" && [ ! -z "${SOURCE}" ] || terminate_usage
SOURCE="$(cd "$(dirname "${SOURCE}")"; pwd)/$(basename "${SOURCE}")"

if [ "$#" -gt 1 ]; then
    VARIANT="${2}"
else
    VARIANT="debug"
fi

[ -d "${SOURCE}" ] || terminate "Source does not exist: ${SOURCE}"

# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
trap 'terminate_trap' SIGHUP SIGINT SIGTERM EXIT

PROJECTNAME=$(cd "${SOURCE}" && ls *_*.js | head -1 | sed 's,_.*.js$,,')
echo $PROJECTNAME

TARGET="${SOURCE}.repack"
echo "$TARGET [${VARIANT}]"

if [ -d "$TARGET" ]; then
    rm -rf "$TARGET"
fi

SUFFIX=""
[ "$VARIANT" != "debug" ] && SUFFIX="_${VARIANT}"

cp -r "${SOURCE}/" "${TARGET}/"

if [ -d "${DYNAMO_HOME}/bin/wasm-web" ]; then
    cp -v "${DYNAMO_HOME}/bin/wasm-web/dmengine${SUFFIX}.js" "${TARGET}/${PROJECTNAME}_wasm.js"
    cp -v "${DYNAMO_HOME}/bin/wasm-web/dmengine${SUFFIX}.wasm" "${TARGET}/${PROJECTNAME}.wasm"
    if [ -e "${DYNAMO_HOME}/bin/wasm-web/dmengine${SUFFIX}.wasm.debug.wasm" ]; then
        cp -v "${DYNAMO_HOME}/bin/wasm-web/dmengine${SUFFIX}.wasm.debug.wasm" "${TARGET}/${PROJECTNAME}.wasm.debug.wasm"
    fi
    if [ -e "${DYNAMO_HOME}/bin/wasm-web/dmengine${SUFFIX}.wasm.map" ]; then
        cp -v "${DYNAMO_HOME}/bin/wasm-web/dmengine${SUFFIX}.wasm.map" "${TARGET}/${PROJECTNAME}.wasm.map"
    fi
fi

if [ -d "${DYNAMO_HOME}/bin/wasm_pthread-web" ]; then
    cp -v "${DYNAMO_HOME}/bin/wasm_pthread-web/dmengine${SUFFIX}.js" "${TARGET}/${PROJECTNAME}_wasm.js"
    cp -v "${DYNAMO_HOME}/bin/wasm_pthread-web/dmengine${SUFFIX}.wasm" "${TARGET}/${PROJECTNAME}.wasm"
    if [ -e "${DYNAMO_HOME}/bin/wasm_pthread-web/dmengine${SUFFIX}.wasm.debug.wasm" ]; then
        cp -v "${DYNAMO_HOME}/bin/wasm_pthread-web/dmengine${SUFFIX}.wasm.debug.wasm" "${TARGET}/${PROJECTNAME}.wasm.debug.wasm"
    fi
    if [ -e "${DYNAMO_HOME}/bin/wasm_pthread-web/dmengine${SUFFIX}.wasm.map" ]; then
        cp -v "${DYNAMO_HOME}/bin/wasm_pthread-web/dmengine${SUFFIX}.wasm.map" "${TARGET}/${PROJECTNAME}.wasm.map"
    fi
fi

# ----------------------------------------------------------------------------
# Script teardown
# ----------------------------------------------------------------------------
trap - SIGHUP SIGINT SIGTERM EXIT
echo "Wrote ${TARGET}"
exit 0
