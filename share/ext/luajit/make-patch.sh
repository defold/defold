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



# The platform is just to be able to load it
. ./version.sh

# VERSION from build_luajit.h
readonly PATCH_FILE=patch_${VERSION}

ORIGINAL_REPO=git@github.com:LuaJIT/LuaJIT.git
CHANGED_REPO=git@github.com:Defold/LuaJIT.git

BRANCH_A=${SHA1}
BRANCH_B=v2.1-defold

DIR_A=repo_a
DIR_B=repo_b

if [ ! -d "${DIR_A}" ]; then
    echo "Cloning ${ORIGINAL_REPO} into ${DIR_A}"
    git clone ${ORIGINAL_REPO} ${DIR_A}
    (cd ${DIR_A} && git checkout ${BRANCH_A})
fi

if [ ! -d "${DIR_B}" ]; then
    echo "Cloning ${CHANGED_REPO} into ${DIR_B}"
    git clone ${CHANGED_REPO} ${DIR_B}
    (cd ${DIR_B} && git checkout ${BRANCH_B})
fi

echo "Creating patch..."

diff -ruw --exclude=".git" ${DIR_A} ${DIR_B} > ${PATCH_FILE}

echo "Wrote ${PATCH_FILE}"

rm -rf ${DIR_A}
rm -rf ${DIR_B}
