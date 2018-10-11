#!/usr/bin/env bash

DIR=$1
if [ "$DIR" == "" ]; then
    DIR=`pwd`
fi

docker run --rm --name ubuntu --hostname=ubuntu -t -i -v ${DIR}:/home/builder builder/ubuntu
