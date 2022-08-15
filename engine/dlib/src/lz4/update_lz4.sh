#!/usr/bin/env bash

set -e


SOURCE_REPO=$1

if [ -z "${SOURCE_REPO}" ]; then
    echo "Usage: ./update.sh <path to repo>"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ -z "${DYNAMO_HOME}" ]; then
    echo "DYNAMO_HOME is not set!"
    exit 1
fi

echo "Found DYNAMO_HOME=${DYNAMO_HOME}"
DEFOLD_HOME=${DYNAMO_HOME}/../..

cp -v ${SOURCE_REPO}/lib/lz4.* ${SCRIPT_DIR}/
cp -v ${SOURCE_REPO}/lib/lz4hc.* ${SCRIPT_DIR}/

SHA1=$(cd ${SOURCE_REPO} && git rev-parse --short HEAD)

echo ""
echo "Copied sha1: ${SHA1}"

echo "Done"

