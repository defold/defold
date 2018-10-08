#!/usr/bin/env bash

DIR=$1
if [ "$DIR" == "" ]; then
    DIR=`pwd`
fi


#docker run --rm -it builder-base -v .:/home/builder /bin/bash
docker run -t -i -v ${DIR}:/home/builder ubuntu
#docker exec -ubuilder -it builder-base /bin/bash