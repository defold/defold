#! /usr/bin/env bash

readonly BASE_URL=https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bullet
readonly FILE_URL=bullet-2.77.zip
readonly PRODUCT=bullet
readonly VERSION=2.77

export CONF_TARGET=$1

. ../common.sh

function cmi_unpack() {
    unzip ../../download/$FILE_URL

    pushd ${PRODUCT}-${VERSION}
    rm -rf Demos Demos Extras UnitTests msvc

    # Convert line endings to unix style
    find . -type f -name "*.*" -exec dos2unix {} \;

    popd
}

function cmi_configure() {
    pushd ${PRODUCT}-${VERSION}

    PREFIX=$PREFIX cmake -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_DEMOS=OFF -DBUILD_EXTRAS=OFF

    popd
}

LIB_SUFFIX=
case $1 in
     *win32)
        LIB_SUFFIX=".lib"
        ;;
     *)
        LIB_SUFFIX=".a"
        ;;
esac

function cmi_make() {
    set -e
    pushd ${PRODUCT}-${VERSION}
    echo cmi_make
    pwd
    make -j8 VERBOSE=1
    #make install

    set +e

    # "install"
    mkdir -p $PREFIX/lib/$CONF_TARGET
    mkdir -p $PREFIX/include/

    pushd src
    find . -name "*.h" -print0 | cpio -pmd0 $PREFIX/include/
    popd
    find . -iname "*${LIB_SUFFIX}" -print0 | xargs -0 -I {} cp -v {} $PREFIX/lib/$CONF_TARGET

    popd
}


download
cmi $1
