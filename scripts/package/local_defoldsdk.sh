#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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
