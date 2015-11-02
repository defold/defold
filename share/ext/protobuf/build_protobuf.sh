#!/bin/sh

readonly BASE_URL=http://protobuf.googlecode.com/files
readonly FILE_URL=protobuf-2.3.0.tar.gz
readonly PRODUCT=protobuf
readonly VERSION=2.3.0

readonly CONFIGURE_ARGS="--with-protoc=../cross_tmp/src/protoc --disable-shared"

. ../common.sh

download
rm -rf cross_tmp
mkdir cross_tmp
pushd cross_tmp >/dev/null
tar xfz ../../download/$FILE_URL --strip-components=1

# We need to apply patches for this cross_tmp build as well
[ -f ../patch_$VERSION ] && echo "Applying patch ../patch_$VERSION" && patch -p1 < ../patch_$VERSION

set -e
./configure && make
set +e
popd >/dev/null

cmi $1

rm -rf cross_tmp
