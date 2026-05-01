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



set -e


REMOTERY_REPO=$1

if [ -z "${REMOTERY_REPO}" ]; then
    echo "Usage: ./update.sh <path to Remotery repo>"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ -z "${DYNAMO_HOME}" ]; then
    echo "DYNAMO_HOME is not set!"
    exit 1
fi

echo "Found DYNAMO_HOME=${DYNAMO_HOME}"
DEFOLD_REPO=${DYNAMO_HOME}/../..


cp -v ${REMOTERY_REPO}/lib/*.c ${DEFOLD_REPO}/engine/dlib/src/remotery/lib/
cp -v ${REMOTERY_REPO}/lib/*.h ${DEFOLD_REPO}/engine/dlib/src/dmsdk/external/remotery/Remotery.h
cp -v -r ${REMOTERY_REPO}/vis/ ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis

echo "Applying patch"

(cd ${DEFOLD_REPO} && git apply ./engine/dlib/src/remotery/defold.patch)

# Since we want to be able to start the server stand along, we want to keep index.html intact
# but we need a patched version to be served from the editor
cp -v ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis/index.html ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis/orig.index.html
(cd ${DEFOLD_REPO} && git apply ./engine/dlib/src/remotery/defoldvis.patch)
(cd ${DEFOLD_REPO} && git apply ./engine/dlib/src/remotery/issue-8146.patch)
cp -v ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis/index.html ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis/patched.index.html
mv -v ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis/orig.index.html ${DEFOLD_REPO}/editor/resources/engine-profiler/remotery/vis/index.html

echo "Done"

