#!/usr/bin/env bash
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
[ "$#" -ge "2" ] || \
    terminate 1 "Usage: ${SCRIPT_NAME} <platform> <module> [, <module> .. ]"
for i in $(seq 2 $#); do
    _SUBMODULE="${@:$i:1}"
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

_TIMESTAMP="$(date +%s)"
for i in $(seq 2 $#); do
    _SUBMODULE="${@:$i:1}"
    (
        cd "${DEFOLD_PATH}/engine/${_SUBMODULE}"
        waf configure build install --platform="${1}" \
            --prefix="${DYNAMO_HOME}" --skip-codesign --skip-tests --skip-build-tests --opt-level=0
    )
done

(
    cd "${DEFOLD_PATH}/engine/engine"
    find "build" -type f -name "*dmengine*" | xargs -I% rm -f "%"
    find "build" -type d -name "*dmengine*" | xargs -I% rm -rf "%"
    waf clean configure build install --platform="${1}" \
        --prefix="${DYNAMO_HOME}" --skip-codesign --skip-tests --skip-build-tests --opt-level=0
)


# ----------------------------------------------------------------------------
# teardown
# ----------------------------------------------------------------------------
echo "Built $(echo $# - 1 | bc) submodule(s) for platform '${1}' ..."
echo "... elapsed time: $(echo $(date +%s) - ${_TIMESTAMP} | bc) seconds"
terminate 0 "${SCRIPT_NAME} done."
