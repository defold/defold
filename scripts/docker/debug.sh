#!/bin/bash

CONTAINER=$(docker ps -f name=ubuntu -q)

if [ -z "$CONTAINER" ]; then
    echo Container \"ubuntu\" is not running. Please start it first \(by running run.sh\)!
else
    docker exec -ubuilder -it $CONTAINER /bin/bash
fi
