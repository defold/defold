readonly BASE_URL=http://protobuf.googlecode.com/files
readonly FILE_URL=protobuf-2.3.0.tar.gz
readonly PRODUCT=protobuf
readonly VERSION=2.3.0

readonly CONFIGURE_ARGS="--with-protoc=../cross_tmp/src/protoc"

. ../common.sh

download
mkdir cross_tmp
pushd cross_tmp
tar xfz ../$FILE_URL --strip-components=1
set -e
./configure && make
set +e
popd

cmi $1

rm -rf cross_tmp

