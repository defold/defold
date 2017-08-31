#!/usr/bin/env bash
set -eu

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"
DEFOLD_PATH="$(cd "${SCRIPT_PATH}/.."; pwd)"

# ----------------------------------------------------------------------------
# Functions
# ----------------------------------------------------------------------------
function copy_binaries() {
    [ "$#" -eq "2" ] || return 1

    _ARG_SOURCE="${1}"
    _ARG_TARGET="${2}"

    [ -d "${_ARG_SOURCE}" ] || return 1
    [ -d "${_ARG_TARGET}" ] || mkdir -p "${_ARG_TARGET}"

    for _SOURCE_FILEPATH in "${_ARG_SOURCE}"/*; do
        _FILENAME="$(basename "${_SOURCE_FILEPATH}")"
        _TARGET_FILEPATH="${_ARG_TARGET}/${_FILENAME}"
        if [ "${_SOURCE_FILEPATH}" -nt "${_TARGET_FILEPATH}" ]; then
            cp -v "${_SOURCE_FILEPATH}" "${_TARGET_FILEPATH}"
        fi
    done

    return 0
}

function copy_file() {
    [ "$#" -eq "2" ] || return 1

    _ARG_SOURCE="${1}"
    _ARG_TARGET="${2}"

    [ -f "${_ARG_SOURCE}" ] || return 1

    if [ "${_ARG_SOURCE}" -nt "${_ARG_TARGET}" ]; then
        cp -v "${_ARG_SOURCE}" "${_ARG_TARGET}"
    fi
}

# ----------------------------------------------------------------------------
# Resources
# ----------------------------------------------------------------------------
SOURCES=(
    "darwin"        "linux"         "x86_64-linux"  "x86_64-darwin"
    "win32"         "armv7-darwin"  "arm64-darwin"  "armv7-android"
    "js-web"        "x86_64-win32"
)

TARGETS=(
    "x86-darwin"    "x86-linux"     "x86_64-linux"  "x86_64-darwin"
    "x86-win32"     "armv7-darwin"  "arm64-darwin"  "armv7-android"
    "js-web"        "x86_64-win32"
)

# ----------------------------------------------------------------------------
# Update all available binaries
# ----------------------------------------------------------------------------
for i in "${!SOURCES[@]}"; do
    _SOURCE="${DYNAMO_HOME}/bin/${SOURCES[$i]}"
    _TARGET="${DEFOLD_PATH}/com.dynamo.cr/com.dynamo.cr.bob/libexec/${TARGETS[$i]}"
    if [ -d "${_SOURCE}" ]; then
        echo "Updating editor with ${TARGETS[$i]} binaries ..."
        copy_binaries "${_SOURCE}" "${_TARGET}"
    fi
done


# ----------------------------------------------------------------------------
# Update extra resources
# ----------------------------------------------------------------------------
echo "Updating editor with classes.dex ..."
copy_file "${DYNAMO_HOME}/share/java/classes.dex" \
          "${DEFOLD_PATH}/com.dynamo.cr/com.dynamo.cr.bob/lib/classes.dex"


# ----------------------------------------------------------------------------
# Teardown
# ----------------------------------------------------------------------------
exit 0
