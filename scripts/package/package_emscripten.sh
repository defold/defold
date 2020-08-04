#! /usr/bin/env bash

# Copyright 2020 The Defold Foundation
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

set -e

VERSION=1.39.16
URL=https://github.com/emscripten-core/emsdk/archive/${VERSION}.tar.gz
PLATFORM=`uname`
PLATFORM="$(tr [A-Z] [a-z] <<< "$PLATFORM")"

PWD=`pwd`

TARGET_PATH=${PWD}/local_sdks
TMP=${TARGET_PATH}/_tmpdir
TARGET=$TARGET_PATH/emsdk-${VERSION}-${PLATFORM}.tar.gz

if [ ! -d $TMP ]; then
	mkdir -p $TMP
fi

pushd $TMP

if [ ! -e "${VERSION}.tar.gz" ]; then
	wget $URL
fi

if [ ! -d "emsdk-${VERSION}" ]; then
	tar xf "${VERSION}.tar.gz"
fi

cd emsdk-${VERSION}

if [ ! -d "upstream" ]; then
	./emsdk install latest
fi

cd ..

if [ ! -e ${TARGET} ]; then
	echo Writing ${TARGET}
	tar czvf ${TARGET} emsdk-${VERSION}
else
	echo Found ${TARGET}
fi

popd

rm -rf ${TMP}

echo Wrote $TARGET
