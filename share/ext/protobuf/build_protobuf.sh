#!/usr/bin/env bash
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



readonly PRODUCT=protobuf
readonly VERSION=2.3.0
readonly BASE_URL=https://github.com/mathiaswking/protobuf-${VERSION}/archive
readonly FILE_URL=protobuf-${VERSION}.tar.gz

readonly CONFIGURE_ARGS="--with-protoc=../cross_tmp/src/protoc --disable-shared"

. ../common.sh

# The package is ancient
# A fairly small package of ~1.5MB, so it's stored locally
function download() {
    cp ./${FILE_URL} ../download/
}

download

#
# Build protoc locally first,
# in order to make the cross platform configuration work
#
rm -rf cross_tmp
mkdir cross_tmp
pushd cross_tmp >/dev/null
tar xfz ../../download/$FILE_URL --strip-components=1

# We need to apply patches for this cross_tmp build as well
[ -f ../patch_$VERSION ] && echo "Applying patch ../patch_$VERSION" && patch -p1 < ../patch_$VERSION

set -e
./configure && make -j8
set +e
popd >/dev/null

cmi $1

rm -rf cross_tmp
