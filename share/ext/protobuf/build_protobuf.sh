#!/usr/bin/env bash
# Copyright 2020-2022 The Defold Foundation
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



readonly PRODUCT=protobuf
readonly VERSION=3.20.1
readonly BASE_URL=https://github.com/protocolbuffers/protobuf/archive
readonly FILE_URL=protobuf-${VERSION}.tar.gz

readonly CONFIGURE_ARGS="--with-protoc=../cross_tmp/src/protoc CPPFLAGS=-DGOOGLE_PROTOBUF_NO_RTTI --disable-shared"

#test the compiler
if [ "$(uname -o)" == "Msys" ]; then
	WINDEFINES=$(echo | g++ -E -dM - | grep WIN32)
	if [ -z "$WINDEFINES" ]; then
		echo "Compiler doesn't support WIN32 defines!"
		echo G++=$(which g++)
		echo $(g++ --version)
		exit 1
	fi
fi

. ../common.sh

# The package is ancient
# A fairly small package of ~1.5MB, so it's stored locally
function download() {
    mkdir -p ../download
    cp ./${FILE_URL} ../download/
}

download

function cmi_configure() {
	./autogen.sh
    echo CONFIGURE_ARGS=$CONFIGURE_ARGS $2
    ${CONFIGURE_WRAPPER} ./configure $CONFIGURE_ARGS $2 \
        --disable-shared \
        --prefix=${PREFIX} \
        --bindir=${PREFIX}/bin/$1 \
        --libdir=${PREFIX}/lib/$1 \
        --with-http=off \
        --with-html=off \
        --with-ftp=off \
        --with-x=no
}

#
# Build protoc locally first,
# in order to make the cross platform configuration work
#
rm -rf cross_tmp
mkdir cross_tmp
pushd cross_tmp >/dev/null
tar xfz ../../download/$FILE_URL --strip-components=1

# We need to apply patches for this cross_tmp build as well
# [ -f ../patch_$VERSION ] && echo "Applying patch ../patch_$VERSION" && patch -p1 < ../patch_$VERSION

echo **********************
echo CREATING HOST TOOLS
echo **********************

set -e
./autogen.sh
./configure
make -j8
set +e
popd >/dev/null

echo **********************
echo CREATING LIBRARY
echo **********************

cmi $1

rm -rf cross_tmp
rm *.o
