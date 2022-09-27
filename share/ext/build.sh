#!/bin/bash
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



function usage() {
    echo "build.sh PRODUCT PLATFORM"
    echo "Supported platforms"
    echo " * darwin"
    echo " * x86_64-macos"
    echo " * arm64-macos"
    echo " * x86_64-linux"
    echo " * arm64-ios"
    echo " * x86_64-ios"
    echo " * armv7-android"
    echo " * arm64-android"
    echo " * i586-mingw32msvc"
    echo " * js-web"
    echo " * wasm-web"
    echo " * win32 (luajit)"
    echo " * x86_64-win32 (luajit)"
    echo " * arm64-nx64"
    exit $1
}

[ -z $1 ] && usage 1
[ -z $2 ] && usage 1

mkdir -p download
mkdir -p build

pushd $1 >/dev/null
./build_$1.sh $2
popd >/dev/null
