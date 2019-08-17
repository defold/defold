#! /usr/bin/env bash

readonly BASE_URL=https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/box2d
readonly FILE_URL=Box2D_v2.2.1.zip
readonly PRODUCT=box2d
readonly VERSION=2.2.1

export CONF_TARGET=$1

. ../common.sh

function cmi_unpack() {
    unzip ../../download/$FILE_URL

    pushd Box2D_v${VERSION}/Box2D
    #rm -rf Demos Demos Extras UnitTests msvc

    # Convert line endings to unix style
    find . -type f -name "*.*" -exec dos2unix {} \;

    popd
}

function cmi_configure() {
    pushd Box2D_v${VERSION}/Box2D
    
    cmake -DCMAKE_BUILD_TYPE=RELEASE -DBOX2D_BUILD_STATIC=ON -DBOX2D_VERSION="${VERSION}" -DBOX2D_INSTALL=OFF

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
    pushd Box2D_v${VERSION}/Box2D
    echo cmi_make
    pwd
    make -j8 VERBOSE=1

    # "install"
    mkdir -p $PREFIX/bin
    mkdir -p $PREFIX/lib/$CONF_TARGET
    mkdir -p $PREFIX/include/Box2D
    mkdir -p $PREFIX/share

    find . -iname "*${LIB_SUFFIX}" -print0 | xargs -0 -I {} cp -v {} $PREFIX/lib/$CONF_TARGET
    find . -name "*.h" -print0 | cpio -pmd0 $PREFIX/include/Box2D

    echo "MAWE: PWD"
    pwd
    popd
    set +e
}


download
cmi $1
