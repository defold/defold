#!/usr/bin/env bash

DIR=$1
if [ "$DIR" == "" ]; then
    DIR=`pwd`
fi

docker run --hostname=ubuntu --name=ubuntu -t -i -v ${DIR}:/home/builder ubuntu
