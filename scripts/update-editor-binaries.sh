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


set -eu

SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"
DEFOLD_PATH="$(cd "${SCRIPT_PATH}/.."; pwd)"
: "${DYNAMO_HOME:?DYNAMO_HOME must be set}"

# Prefer locally rebuilt engine binaries and classes.dex, including after sync_archive.
# Repackage the jar used by the editor; Gradle skips unchanged compilation.
exec "${DEFOLD_PATH}/com.dynamo.cr/com.dynamo.cr.bob/gradlew" \
    -p "${DEFOLD_PATH}/com.dynamo.cr/com.dynamo.cr.bob" -Pprefer-local-engines install "$@"
