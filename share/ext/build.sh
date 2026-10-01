#!/bin/bash
function usage() {
    echo "build.sh PRODUCT PLATFORM"
    echo "Supported platforms"
    echo " * darwin"
    echo " * x86_64-macos"
    echo " * arm64-macos"
    echo " * x86_64-linux"
    echo " * arm64-linux"
    echo " * arm64-ios"
    echo " * arm64_sim-ios"
    echo " * armv7-android"
    echo " * arm64-android"
    echo " * x86_64-android"
    echo " * wasm-web"
    echo " * wasm_pthread-web"
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
