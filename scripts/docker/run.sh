#!/usr/bin/env bash
# Copyright 2020-2025 The Defold Foundation
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



DIR=$1
if [ "$DIR" == "" ]; then
    DIR=`pwd`
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ ! -z "${DYNAMO_HOME}" ]; then
    USE_PATH_MAPPINGS="-v ${DYNAMO_HOME}:/dynamo_home"
fi

if [ ! -z "${DM_PACKAGES_URL}" ]; then
    USE_ENV="--env DM_PACKAGES_URL=${DM_PACKAGES_URL}"
fi

if [ ! -z "${DM_DOCKER_BUILD_PLATFORM}" ]; then
    DOCKER_PLATFORM="--platform ${DM_DOCKER_BUILD_PLATFORM}"
fi

docker run --rm --name ubuntu --hostname=ubuntu -t -i -v ${DIR}:/home/builder ${DOCKER_PLATFORM} ${USE_PATH_MAPPINGS} ${USE_ENV} -v ${SCRIPT_DIR}/bashrc:/home/builder/.bashrc builder/ubuntu
