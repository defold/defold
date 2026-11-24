#!/usr/bin/env bash
readonly REPO_URL=https://github.com/defold/defold-tremolo.git

readonly BUILD_DIR=./build
readonly PRODUCT=tremolo

PLATFORM=$1

. ../common.sh

function cmi_download() {
    echo "cmi_download -- no package to download"
}

function cmi_unpack() {
    echo "cmi_unpack -- Cloning repo"
    git clone --depth 1 ${REPO_URL} .
}

function cmi_configure() {
    echo "cmi_configure -- no project to configure"
}

case ${PLATFORM} in
    armv7-android)
        export CFLAGS="-D_ARM_ASSEM_ ${CFLAGS}"
        ;;
    *)
        export CFLAGS="-DONLY_C ${CFLAGS}"
        ;;
esac

export CFLAGS="-D_GNU_SOURCE -funsigned-char -Wall -Werror -Wno-unused-variable ${CFLAGS}"


function run() {
    echo "CC: $*"
    $*
}

function cmi_make() {
    # The version is from our own fork but that's ok I think
    export VERSION=$(git rev-parse --short HEAD)

    export CFLAGS="${CFLAGS} -O2 $EXTRA_FLAGS"

    if [ "${PLATFORM}" == "armv7-android" ]; then
        export CFLAGS=${CFLAGS//-mthumb/}
    fi

    ./build.sh ${PLATFORM} ${PREFIX}
}

cmi $1
