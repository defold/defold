#!/usr/bin/env bash

./scripts/build.py build_platform_sdk

# Unpack the sdk to a new folder
SDKDIR=$(pwd)/testsdk
rm -rf ${SDKDIR}

unzip -q ${DYNAMO_HOME}/defoldsdk.zip -d ${SDKDIR}
echo "export DYNAMO_HOME=${SDKDIR}/defoldsdk"

# since the docker build needs some linux tools
echo "Copying linux tools for Docker"

mkdir testsdk/defoldsdk/lib/x86_64-linux
mkdir testsdk/defoldsdk/ext/bin/x86_64-linux

tar xvf ./packages/protobuf-2.3.0-x86_64-linux.tar.gz -C testsdk/defoldsdk/ext bin/x86_64-linux/protoc
#cp -v ${DYNAMO_HOME}/ext/bin/x86_64-linux/protoc testsdk/defoldsdk/ext/bin/x86_64-linux

if [ ! -f ${DYNAMO_HOME}/lib/x86_64-linux/libdlib_shared.so ]; then
    echo "FILE doesn't exist"

    if [ "" == "$SHA1" ]; then
        SHA1=$(git rev-parse HEAD)
    fi

    mkdir -p ${DYNAMO_HOME}/lib/x86_64-linux

    echo "Using SHA1=$SHA1"
    URL="http://d.defold.com/archive/alpha/${SHA1}/engine/x86_64-linux/libdlib_shared.so"
    echo "Downloading" $URL
    curl $URL -o ${DYNAMO_HOME}/lib/x86_64-linux/libdlib_shared.so
fi

cp -v ${DYNAMO_HOME}/lib/x86_64-linux/libdlib_shared.so testsdk/defoldsdk/lib/x86_64-linux
