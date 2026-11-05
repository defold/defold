#!/usr/bin/env bash
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
