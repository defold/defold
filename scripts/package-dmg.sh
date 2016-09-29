#!/usr/bin/env bash
set -eu

# ----------------------------------------------------------------------------
# Environment
# ----------------------------------------------------------------------------
SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"

# ----------------------------------------------------------------------------
# Functions
# ----------------------------------------------------------------------------
function terminate() {
    trap - SIGHUP SIGINT SIGTERM
    echo "TERM - ${1:-}" && exit ${2:-1}
}

function terminate_usage() {
    echo "${1:-}"
    echo "Usage: ${SCRIPT_NAME} <source> <target>"
    echo " - source : The directory to package as dmg."
    echo " - target : The target filepath the dmg should be written to."
    exit 0
}

function abspath() {
    __BASENAME="$(basename "${1:-}")"
    __DIRNAME="$(dirname "${1:-}")"
    if [ -d "${__DIRNAME}" ]; then
        __DIRNAME="$(cd "${__DIRNAME}"; pwd)"
    fi
    echo "${__DIRNAME}/${__BASENAME}"
}

# ----------------------------------------------------------------------------
# Initialize
# ----------------------------------------------------------------------------
trap 'terminate "An unexpected error occurred."' SIGHUP SIGINT SIGTERM

ARG_SOURCE="$(abspath "${1:-}")"
ARG_TARGET="$(abspath "${2:-}")"
ARG_PACKAGE="$(basename "${ARG_TARGET}")"
ARG_DESTINATION="$(dirname "${ARG_TARGET}")"
[ "${#}" -eq "2" ] || terminate_usage "Wrong number of arguments."
[ -d "${ARG_SOURCE}" ] || terminate_usage "Source does not exist: ${ARG_SOURCE}"
[ -d "${ARG_DESTINATION}" ] || terminate_usage "Destination does not exist: ${ARG_DESTINATION}"
[ $(which "hdiutil") ] || terminate "hdiutil could not be found."

# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
CONTAINER="$(mktemp -d)"
CONTAINER_BUILD="${CONTAINER}/build"
CONTAINER_INSTALL="${CONTAINER}/install"

mkdir "${CONTAINER_BUILD}"
mkdir "${CONTAINER_INSTALL}"

cp -fR "${ARG_SOURCE}" "${CONTAINER_BUILD}"
chmod -R 755 "${CONTAINER_BUILD}"
(cd "${CONTAINER_BUILD}"; ln -s /Applications Applications)

codesign --deep -s "Developer ID Application: Mikael Saker (UP78WR3VG6)" "${CONTAINER_BUILD}/defold-stable/Defold.app"
hdiutil create -volname "Defold" -srcfolder "${CONTAINER_BUILD}" "${CONTAINER_INSTALL}/${ARG_PACKAGE}"
codesign -s "Developer ID Application: Mikael Saker (UP78WR3VG6)" "${CONTAINER_INSTALL}/${ARG_PACKAGE}"

cp -fv "${CONTAINER_INSTALL}/${ARG_PACKAGE}" "${ARG_TARGET}"
rm -rf "${CONTAINER}"






















