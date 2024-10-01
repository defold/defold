#!/usr/bin/env bash

URL=https://github.com/madler/zlib/archive/refs/heads/develop.zip

TMP=./tmp
ZIP=./zlib.zip
curl -L $URL --output ${ZIP}
unzip -q -o ${ZIP} -d ${TMP}

set +e
rm *.h *.c
set -e

cp ${TMP}/zlib-develop/*.c .
cp ${TMP}/zlib-develop/*.h .

rm ${ZIP}
rm -rf ${TMP}
