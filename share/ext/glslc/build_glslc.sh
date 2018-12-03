#!/usr/bin/env bash

readonly PRODUCT=glslc
readonly VERSION=v2018.0
readonly FILE_URL=${VERSION}.tar.gz
readonly BASE_URL=https://github.com/google/shaderc/archive/

. ../common.sh

function cmi_configure() {
    pushd third_party >/dev/null
    git clone --depth 1 https://github.com/google/googletest.git
    git clone --depth 1 https://github.com/google/glslang.git
    git clone --depth 1 https://github.com/KhronosGroup/SPIRV-Tools.git spirv-tools
    git clone --depth 1 https://github.com/KhronosGroup/SPIRV-Headers.git spirv-headers
    git clone --depth 1 https://github.com/google/re2.git
    git clone --depth 1 https://github.com/google/effcee.git
    popd >/dev/null

    case $PLATFORM in
        x86_64-darwin)
            CMAKE_GENERATOR="Unix Makefiles"
            MAKE_OPTIONS="-- -j8"
            OUTPUT_EXECUTABLE_FILE=$PRODUCT/$PRODUCT
            ;;

        x86_64-linux)
            CMAKE_GENERATOR="Unix Makefiles"
            MAKE_OPTIONS="-- -j8"
            OUTPUT_EXECUTABLE_FILE=$PRODUCT/$PRODUCT
            ;;

    	win32)
            CMAKE_GENERATOR="Visual Studio 14 2015"
            MAKE_OPTIONS=""
            OUTPUT_EXECUTABLE_FILE=$PRODUCT/Release/$PRODUCT.exe
            ;;

        x86_64-win32)
            CMAKE_GENERATOR="Visual Studio 14 2015 Win64"
            MAKE_OPTIONS=""
            OUTPUT_EXECUTABLE_FILE=$PRODUCT/Release/$PRODUCT.exe
            ;;
    esac

    set -e
    mkdir -p build >/dev/null
    pushd build >/dev/null
    cmake -G"${CMAKE_GENERATOR}" ..
    popd >/dev/null
}

function cmi_make() {
    set -e

    pushd build >/dev/null
    cmake --build . --config Release --target glslc_exe $MAKE_OPTIONS
    mkdir -p $PREFIX/bin/$PLATFORM
    cp $OUTPUT_EXECUTABLE_FILE $PREFIX/bin/$PLATFORM
    popd >/dev/null
    cmi_strip

    set +e
}

function cmi_buildplatform() {
    cmi_do $PLATFORM ""

    local TGZ="$PRODUCT-$VERSION-$PLATFORM.tar.gz"

    pushd $PREFIX  >/dev/null
    tar cfvz $TGZ bin

    popd >/dev/null
    popd >/dev/null

    mkdir ../build

    mv -v $PREFIX/$TGZ ../build
    echo "../build/$TGZ created"

    rm -rf tmp
    rm -rf $PREFIX
}

function cmi() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    case $PLATFORM in
        x86_64-darwin)
            function cmi_strip() {
                strip -S -x $PREFIX/bin/$PLATFORM/$PRODUCT
            }
            cmi_buildplatform $PLATFORM
            ;;

        x86_64-linux)
            function cmi_strip() {
                strip -s $PREFIX/bin/$PLATFORM/$PRODUCT
            }
            cmi_buildplatform $PLATFORM
            ;;

    	win32|x86_64-win32)
            function cmi_strip() {
                echo "No stripping supported"
            }
            cmi_setup_vs2015_env $PLATFORM
            cmi_buildplatform $PLATFORM
            ;;
        *)
            echo "Unknown target $PLATFORM" && exit 1
            ;;
    esac
}

download
cmi $1
