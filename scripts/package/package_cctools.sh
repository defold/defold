#! /usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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



set -e

PWD=`pwd`

VERSION_TAPI=1.6
VERSION_TAPI_SHA1=a66284251b46d591ee4a0cb4cf561b92a0c138d8
VERSION_CCTOOLS_SHA1=6c438753d2252274678d3e0839270045698c159b

TARGET_PATH=${PWD}/local_sdks
TMP=${TARGET_PATH}/_tmpdir
TAPITMP=${TMP}/tapi${VERSION_TAPI}

DARWIN_VERSION=darwin19
TARGET_CCTOOLS=$TARGET_PATH/cctools-port-${DARWIN_VERSION}-${VERSION_CCTOOLS_SHA1}-linux.tar.gz

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
    # # ******************************************************
    echo Compiling apple-libtapi

    mkdir -p $TAPITMP

    git clone https://github.com/tpoechtrager/apple-libtapi.git
    pushd apple-libtapi

    git checkout ${VERSION_TAPI_SHA1}
    INSTALLPREFIX=$TAPITMP ./build.sh
    ./install.sh

    popd

    # ******************************************************
    echo Creating ${TARGET_CCTOOLS}

    CCTOOLSTMP=${TMP}/cctools-port

    mkdir -p $CCTOOLSTMP

    git clone https://github.com/tpoechtrager/cctools-port.git

    cp -v -r $TAPITMP ${CCTOOLSTMP}
    TAPITMP=${CCTOOLSTMP}/tapi${VERSION_TAPI}

    pushd cctools-port/cctools

    git checkout ${VERSION_CCTOOLS_SHA1}

    TMPDIR=$TMP ./configure --prefix=${CCTOOLSTMP} --target=arm-apple-${DARWIN_VERSION} --with-libtapi=$TAPITMP
    make -j8
    make install
    make distclean
    TMPDIR=$TMP ./configure --prefix=${CCTOOLSTMP} --target=x86_64-apple-${DARWIN_VERSION} --with-libtapi=$TAPITMP
    make -j8
    make install
    make distclean

    popd

    tar -czvf ${TARGET_CCTOOLS} -C $TMP cctools-port

    echo Wrote ${TARGET_CCTOOLS}
else
    echo Found ${TARGET_CCTOOLS}
fi

#rm -rf ${TMP}


