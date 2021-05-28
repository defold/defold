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

cp -v ${DYNAMO_HOME}/lib/x86_64-linux/libdlib_shared.so testsdk/defoldsdk/lib/x86_64-linux
cp -v ${DYNAMO_HOME}/ext/bin/x86_64-linux/protoc testsdk/defoldsdk/ext/bin/x86_64-linux
