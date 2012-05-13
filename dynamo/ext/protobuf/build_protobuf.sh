#!/bin/sh

readonly BASE_URL=http://protobuf.googlecode.com/files
readonly FILE_URL=protobuf-2.3.0.tar.gz
readonly PRODUCT=protobuf
readonly VERSION=2.3.0

readonly CONFIGURE_ARGS="--with-protoc=../cross_tmp/src/protoc"

. ../common.sh

download
rm -rf cross_tmp
mkdir cross_tmp
pushd cross_tmp >/dev/null
tar xfz ../../download/$FILE_URL --strip-components=1
set -e
./configure && make
set +e
popd >/dev/null

cmi $1

rm -rf cross_tmp

