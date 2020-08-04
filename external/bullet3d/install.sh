#!/usr/bin/env bash

readonly BASE_URL=https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bullet
readonly FILE_URL=bullet-2.77.zip
readonly PRODUCT=bullet
readonly VERSION=2.77

. ../common.sh

function convert_line_endings() {
    local platform=$(uname)
    case $platform in
         *linux)
            DOS2UNIX=fromdos
            ;;
         *)
            DOS2UNIX=dos2unix
            ;;
    esac
    find . -type f -name "*.*" -exec $DOS2UNIX {} \;
}

function cmi_unpack() {
	echo cmi_unpack
    #unzip -q ../../download/$FILE_URL
    unzip -q ../$FILE_URL
    pushd ${PRODUCT}-${VERSION}
    rm -rf Demos Extras UnitTests msvc Glut
    # Convert line endings to unix style
    convert_line_endings
    popd
}

cmi_install
