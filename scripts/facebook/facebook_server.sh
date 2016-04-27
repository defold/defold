#!/usr/bin/env bash
set -eu

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"

function terminate() {
    _EXITCODE=${1}
    _MESSAGE=${2}
    echo "${_MESSAGE}"
    trap - EXIT
    exit ${_EXITCODE}
}

[ "$#" == "1" ] || terminate 1 "Usage: ${SCRIPT_NAME} <port>"
_ARG_PORT="${1}"

openssl req -x509 -newkey rsa:2048 -days 30 -subj "/CN=Defold Team" \
    -nodes -keyout "${SCRIPT_PATH}/key.pem" -out "${SCRIPT_PATH}/cert.pem" \
    > /dev/null 2>&1

python "${SCRIPT_PATH}/facebook_iap_server.py" "${_ARG_PORT}"

rm -f "${SCRIPT_PATH}/key.pem"
rm -f "${SCRIPT_PATH}/cert.pem"
