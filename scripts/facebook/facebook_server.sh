#!/usr/bin/env bash
set -eu

# ----------------------------------------------------------------------------
# Script environment
# ----------------------------------------------------------------------------
SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"

if [ "$(whoami)" != "root" ]; then
    echo "Must be root to serve HTTPS on port 443 ..."
    exit 1
fi


# ----------------------------------------------------------------------------
# Script functions
# ----------------------------------------------------------------------------
function terminate() {
    trap - SIGHUP SIGINT SIGTERM EXIT
    rm -f "${SCRIPT_PATH}/key.pem"
    rm -f "${SCRIPT_PATH}/cert.pem"
}

# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------
trap 'terminate' SIGHUP SIGINT SIGTERM EXIT
openssl req -x509 -newkey rsa:2048 -days 30 -subj "/CN=Defold Team"         \
    -nodes -keyout "${SCRIPT_PATH}/key.pem" -out "${SCRIPT_PATH}/cert.pem"  \
    > /dev/null 2>&1

python "${SCRIPT_PATH}/https_fileserver.py" \
    --certificate "${SCRIPT_PATH}/cert.pem" \
    --keyfile "${SCRIPT_PATH}/key.pem"