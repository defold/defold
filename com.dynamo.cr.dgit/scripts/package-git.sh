#!/bin/sh
set -e
VERSION=1.7.9.5

[ -z $1 ] && echo "usage: package-git PLATFORM" && exit 1
PLATFORM=$1

ROOT=`pwd`
GIT_DIR=${ROOT}/tmp/build
rm -rf tmp

mkdir -p tmp
pushd tmp
curl -O http://git-core.googlecode.com/files/git-${VERSION}.tar.gz
tar xfz git-${VERSION}.tar.gz
pushd git-${VERSION}
./configure --prefix=${GIT_DIR}
make -j4 && make install
popd
popd

rm -rvf git/$PLATFORM
mkdir -p git/$PLATFORM/bin/
mkdir -p git/$PLATFORM/libexec/git-core

for x in git git-receive-pack git-upload-pack; do
    cp -v $GIT_DIR/bin/$x git/$PLATFORM/bin/
done

for x in git-pull git-sh-setup git-sh-i18n git-merge git-remote-http git-remote-https; do
    cp -v $GIT_DIR/libexec/git-core/$x git/$PLATFORM/libexec/git-core
done

