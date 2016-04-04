#!/usr/bin/env bash

# NOTE: On Linux at least libcurl4-openssl-dev and gettext packages are required apart from gcc etc
# Probably some other dev packages as well

# About static linking. Static linking is not currently possible
# on Linux as the configure scripts fails to discover curl as
# it on links with curl and not with the libraries curl is dependent on
# We could perhaps patch configure?

set -e
VERSION=1.7.9.5

[ -z "${1}" ] && echo "usage: package-git PLATFORM" && exit 1
PLATFORM="${1}"

ROOT=`pwd`
if [ "$PLATFORM" == "darwin" ]; then
    SDK='/Developer/SDKs/MacOSX10.6.sdk'
    [ ! -e "$SDK" ] && echo "SDK $SDK not found!" && exit 1
	export CFLAGS="-isysroot $SDK"
	export LDFLAGS="-isysroot $SDK"
fi
GIT_DIR=${ROOT}/tmp/build
rm -rf tmp

(
	mkdir -p tmp
	cd tmp

	curl -O http://git-core.googlecode.com/files/git-${VERSION}.tar.gz
	tar xfz git-${VERSION}.tar.gz
	(
		cd git-${VERSION}
		./configure --prefix=${GIT_DIR}
		make -j4
		make install
	)
)

rm -rvf git/$PLATFORM
mkdir -p git/$PLATFORM/bin/
mkdir -p git/$PLATFORM/libexec/git-core

for x in git git-receive-pack git-upload-pack; do
    cp -v $GIT_DIR/bin/$x git/$PLATFORM/bin/
done

for x in git-pull git-sh-setup git-sh-i18n git-merge git-remote-http git-remote-https; do
    cp -v $GIT_DIR/libexec/git-core/$x git/$PLATFORM/libexec/git-core
done

