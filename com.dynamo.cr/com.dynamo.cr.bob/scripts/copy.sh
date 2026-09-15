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

# Public artifacts are read directly by Gradle from DYNAMO_HOME or its archive.
# Keep the existing staging directories for private-platform copy hooks.

DIR="$(cd "$(dirname "$0")" && pwd)"

set -e
mkdir -p lib/x86_64-linux
mkdir -p lib/arm64-linux
mkdir -p lib/x86_64-macos
mkdir -p lib/arm64-macos
mkdir -p lib/x86_64-win32

mkdir -p libexec
mkdir -p libexec/x86_64-linux
mkdir -p libexec/arm64-linux
mkdir -p libexec/x86_64-macos
mkdir -p libexec/arm64-macos
mkdir -p libexec/x86_64-win32
mkdir -p libexec/arm64-ios
# mkdir -p libexec/arm64_sim-ios
# mkdir -p libexec/armv7-android
mkdir -p libexec/arm64-android
# mkdir -p libexec/x86_64-android
mkdir -p libexec/wasm-web
# mkdir -p libexec/wasm_pthread-web

if [ -e "${DIR}/copy_private.sh" ]; then
    sh ${DIR}/copy_private.sh
fi
