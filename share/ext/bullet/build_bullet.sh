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
    #find . -type f -name "*.*" -exec sh -c "tr -d '\015' <{} >UNIX-file; mv UNIX-file {}; echo {}" \;
    #find . -type f -exec sh -c "tr -d '\015' <{} >UNIX-file; mv UNIX-file {}; echo {}" \;
    find . -type f -name "*.*" -exec dos2unix {} \;

    popd
}

function cmi_configure() {
    pushd ${PRODUCT}-${VERSION}
    
    cmake -DCMAKE_BUILD_TYPE=RELEASE -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_DEMOS=OFF -DBUILD_EXTRAS=OFF

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
    pushd ${PRODUCT}-${VERSION}
    echo cmi_make
    pwd
    make -j8 VERBOSE=1

    echo $PWD
    echo $PREFIX/lib/$CONF_TARGET

    # "install"
    mkdir -p $PREFIX/lib/$CONF_TARGET
    mkdir -p $PREFIX/include/

    find . -iname "*${LIB_SUFFIX}" -print0 | xargs -0 -I {} cp -v {} $PREFIX/lib/$CONF_TARGET
    pushd src
    find . -name "*.h" -print0 | cpio -pmd0 $PREFIX/include/
    popd

    popd
}


download
cmi $1
