#!/usr/bin/env bash

COUNT=$1
shift
ARGS=$*

function usage {
    echo "USAGE: iterate.sh <count> <cmd> [<args>]"
    exit 1
}

if [ "" == "${COUNT}" ]; then
    usage
fi

if [ "" == "${ARGS}" ]; then
    usage
fi

set -e

for (( i=0 ; i < ${COUNT} ; i++ )) ; do
    echo "**************************************************************************************************************"
    echo "Loop: ${i}: ${ARGS} "
    ${ARGS}
done
