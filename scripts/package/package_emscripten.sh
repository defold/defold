#! /usr/bin/env bash

set -e

VERSION=4.0.6
URL=https://github.com/emscripten-core/emsdk/archive/${VERSION}.tar.gz
PLATFORM=`uname`
PLATFORM="$(tr [A-Z] [a-z] <<< "$PLATFORM")"

PWD=`pwd`

ARCH=$(arch)
if [ "${ARCH}" == 'i386' ]; then
	ARCH='x86_64'
fi

if [ "${PLATFORM}" == 'darwin' ]; then
	PLATFORM='macos'
fi

TARGET_PATH=${PWD}/local_sdks
TMP=${TARGET_PATH}/_tmpdir
TARGET=$TARGET_PATH/emsdk-${VERSION}-${ARCH}-${PLATFORM}.tar.gz

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
