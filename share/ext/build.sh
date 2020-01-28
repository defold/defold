#!/bin/bash

function invoke_function() {
    # used to invoke something only if the function exists
    FUNCTION=$1
    shift
    [ "$(LC_ALL=C type -t $FUNCTION)" = function ] && $FUNCTION $@
}

[ -f common_private.sh ] && source ./common_private.sh

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
    echo " * arm64-android"
    echo " * i586-mingw32msvc"
    echo " * js-web"
    echo " * wasm-web"
    echo " * win32 (luajit)"
    echo " * x86_64-win32 (luajit)"
    invoke_function usage_private hello world 1 2 3
    exit $1
}

[ -z $1 ] && usage 1
[ -z $2 ] && usage 1

mkdir -p download
mkdir -p build

pushd $1 >/dev/null
./build_$1.sh $2
popd >/dev/null
