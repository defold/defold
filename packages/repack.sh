#!/usr/bin/env bash

set -e

PKG=$1

mkdir -p packages/tmp

cd packages/tmp
pwd

tar xvf ../${PKG}

function rename {
    local src=$1
    local tgt=$2
    if [ -d "$src" ]; then
        echo "Renaming $src to $tgt"
        mv $src $tgt
    fi
}

rename bin/x86_64-darwin bin/x86_64-macos
rename lib/x86_64-darwin lib/x86_64-macos
rename bin/arm64-darwin bin/arm64-ios
rename lib/arm64-darwin lib/arm64-ios

#tree .

dirs=""
if [ -d "bin" ]; then
    dirs="$dirs bin"
fi
if [ -d "lib" ]; then
    dirs="$dirs lib"
fi
if [ -d "include" ]; then
    dirs="$dirs include"
fi
if [ -d "share" ]; then
    dirs="$dirs share"
fi

tar czvf ../${PKG} ${dirs}

cd ../..
pwd

echo "Removing ./packages/tmp"
rm -rf ./packages/tmp
