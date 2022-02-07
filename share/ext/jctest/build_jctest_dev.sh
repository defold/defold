#!/usr/bin/env bash

BRANCH=message-flush
URL=https://github.com/JCash/jctest/archive/refs/heads/${BRANCH}.zip
PREFIX=./tmp

wget -q $URL

rm -rf ./tmp_unpacked
unzip -q ${BRANCH}.zip -d tmp_unpacked

mkdir -p ${PREFIX}/include/jc_test

cp -v ./tmp_unpacked/jctest-${BRANCH}/src/jc_test.h ${PREFIX}/include/jc_test

cd ${PREFIX} && tar czvf ../jctest-dev-common.tar.gz include

rm -rf ./tmp_unpacked
rm -rf ./tmp