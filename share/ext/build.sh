#!/bin/bash

function usage() {
    echo "build.sh PRODUCT PLATFORM"
    echo "Supported platforms"
    echo " * darwin"
    echo " * x86_64-darwin"
    echo " * linux"
    echo " * x86_64-linux"
    echo " * armv7-darwin"
    echo " * arm64-darwin"
    echo " * x86_64-ios"
    echo " * armv7-android"
    echo " * i586-mingw32msvc"
    echo " * js-web"
    echo " * as3-web"
    echo " * win32 (luajit)"
    echo " * x86_64-win32 (luajit)"
    exit $1
}

[ -z $1 ] && usage 1
[ -z $2 ] && usage 1

mkdir -p download
mkdir -p build

pushd $1 >/dev/null
./build_$1.sh $2
popd >/dev/null
