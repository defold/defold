#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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

set -euo pipefail

MBEDTLS_VERSION="4.1.0"
MBEDTLS_ARCHIVE_URL="https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${MBEDTLS_VERSION}/mbedtls-${MBEDTLS_VERSION}.tar.bz2"
MBEDTLS_ARCHIVE_DIR="mbedtls-${MBEDTLS_VERSION}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mbedtls-update.XXXXXX")"

cleanup()
{
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

require_tool()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required tool: $1" >&2
        exit 1
    fi
}

copy_tree()
{
    local source="$1"
    local destination="$2"
    mkdir -p "$(dirname "${destination}")"
    cp -R "${source}" "${destination}"
}

remove_upstream_build_files()
{
    find "${SCRIPT_DIR}" \
        \( -name '.gitignore' \
        -o -name 'CMakeLists.txt' \
        -o -name 'Makefile' \
        -o -name '*.cmake' \
        -o -name 'crypto-library.make' \) \
        -delete
}

require_tool curl
require_tool tar

echo "Downloading ${MBEDTLS_ARCHIVE_URL}"
curl -L -o "${TMP_DIR}/mbedtls.tar.bz2" "${MBEDTLS_ARCHIVE_URL}"
tar -xjf "${TMP_DIR}/mbedtls.tar.bz2" -C "${TMP_DIR}"

MBEDTLS_ROOT="${TMP_DIR}/${MBEDTLS_ARCHIVE_DIR}"

echo "Replacing vendored Mbed TLS sources in ${SCRIPT_DIR}"
rm -rf \
    "${SCRIPT_DIR}/include" \
    "${SCRIPT_DIR}/library" \
    "${SCRIPT_DIR}/tf-psa-crypto"

copy_tree "${MBEDTLS_ROOT}/include" "${SCRIPT_DIR}/include"
copy_tree "${MBEDTLS_ROOT}/library" "${SCRIPT_DIR}/library"

mkdir -p "${SCRIPT_DIR}/tf-psa-crypto"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/include" "${SCRIPT_DIR}/tf-psa-crypto/include"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/core" "${SCRIPT_DIR}/tf-psa-crypto/core"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/dispatch" "${SCRIPT_DIR}/tf-psa-crypto/dispatch"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/drivers" "${SCRIPT_DIR}/tf-psa-crypto/drivers"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/extras" "${SCRIPT_DIR}/tf-psa-crypto/extras"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/platform" "${SCRIPT_DIR}/tf-psa-crypto/platform"
copy_tree "${MBEDTLS_ROOT}/tf-psa-crypto/utilities" "${SCRIPT_DIR}/tf-psa-crypto/utilities"

remove_upstream_build_files

echo "Updated Mbed TLS to ${MBEDTLS_VERSION}"
