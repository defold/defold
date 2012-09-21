#!/bin/sh

function usage() {
    echo "build.sh PRODUCT PLATFORM"
    echo "Supported platforms"
    echo " * darwin"
    echo " * x86_64-darwin"
    echo " * linux"
    echo " * armv6-darwin"
    echo " * i586-mingw32msvc"
    exit $1
}

[ -z $1 ] && usage 1
[ -z $2 ] && usage 1

mkdir -p download
mkdir -p build

pushd $1 >/dev/null
./build_$1.sh $2
popd >/dev/null
