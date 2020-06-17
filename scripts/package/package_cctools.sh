#! /usr/bin/env bash

set -e

PWD=`pwd`

TARGET_PATH=${PWD}/local_sdks
TMP=${TARGET_PATH}/_tmpdir
TAPITMP=${TMP}/tapi1.4

VERSION_TAPI=3efb201881e7a76a21e0554906cf306432539cef
VERSION_CCTOOLS=3f979bbcd7ee29d79fb93f829edf3d1d16441147

TARGET_CCTOOLS=$TARGET_PATH/cctools-port-darwin14-${VERSION_CCTOOLS}-linux.tar.gz

# check for cmake

if [ "$(which cmake)" == "" ]; then
    echo "You must have cmake installed"
    exit 1
fi


if [ ! -d $TMP ]; then
    mkdir -p $TMP
fi

pushd $TMP


if [ ! -e ${TARGET_CCTOOLS} ]; then
    # ******************************************************
    echo Compiling apple-libtapi

    mkdir -p $TAPITMP

    git clone https://github.com/tpoechtrager/apple-libtapi.git
    pushd apple-libtapi

    git checkout ${VERSION_TAPI}
    INSTALLPREFIX=$TAPITMP ./build.sh
    ./install.sh

    popd

    # ******************************************************
    echo Creating ${TARGET_CCTOOLS}

    CCTOOLSTMP=${TMP}/cctools-port

    mkdir -p $CCTOOLSTMP

    git clone https://github.com/tpoechtrager/cctools-port.git

    pushd cctools-port/cctools

    git checkout ${VERSION_CCTOOLS}

    ./configure --prefix=${CCTOOLSTMP} --target=arm-apple-darwin14 --with-libtapi=$TAPITMP
    make -j8
    make install
    make distclean
    ./configure --prefix=${CCTOOLSTMP} --target=x86_64-apple-darwin14 --with-libtapi=$TAPITMP
    make -j8
    make install
    make distclean

    popd

    tar -czvf ${TARGET_CCTOOLS} -C $TMP cctools-port

    echo Wrote ${TARGET_CCTOOLS}
else
    echo Found ${TARGET_CCTOOLS}
fi

rm -rf ${TMP}


