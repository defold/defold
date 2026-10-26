#!/usr/bin/env bash
set -eu

SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"
DEFOLD_PATH="$(cd "${SCRIPT_PATH}/.."; pwd)"
: "${DYNAMO_HOME:?DYNAMO_HOME must be set}"

# Prefer locally rebuilt engine binaries and classes.dex, including after sync_archive.
# Repackage the jar used by the editor; Gradle skips unchanged compilation.
exec "${DEFOLD_PATH}/com.dynamo.cr/com.dynamo.cr.bob/gradlew" \
    -p "${DEFOLD_PATH}/com.dynamo.cr/com.dynamo.cr.bob" -Pprefer-local-engines install "$@"
