#!/usr/bin/env bash

set -e

SHA1=$1

if [ -z "${SHA1}" ]; then
    echo "You must specify a SHA1"
    echo "Usage: ./update_skribidi.sh <sha1>"
    exit 1
fi

SHA1_SHORT=${SHA1:0:7}

ZIP=${SHA1}.zip
ZIP_SHORT=Skribidi-${SHA1_SHORT}.zip
URL=https://github.com/memononen/Skribidi/archive/${ZIP}

if [ ! -e ${ZIP_SHORT} ]; then
    wget ${URL}
    mv ${ZIP} ${ZIP_SHORT}
    git add ${ZIP_SHORT}
fi

if [ "" != "$(ls ./package/Skribidi)" ]; then
    rm -rf ./package/Skribidi
fi

(cd package && unzip ../${ZIP_SHORT})

mv package/Skribidi-${SHA1} package/Skribidi
git add package/Skribidi
