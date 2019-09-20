#!/usr/bin/env bash

DIR=$1
if [ "$DIR" == "" ]; then
    DIR=`pwd`
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

docker run --rm --name ubuntu --hostname=ubuntu -t -i -v ${DIR}:/home/builder -v ${SCRIPT_DIR}/bashrc:/home/builder/.bashrc builder/ubuntu
