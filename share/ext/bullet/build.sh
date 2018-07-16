#! /usr/bin/env bash

readonly BASE_URL=https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bullet
readonly FILE_URL=bullet-2.77.zip
readonly PRODUCT=bullet
readonly VERSION=2.77

export CONF_TARGET=$1

. ../common.sh


function convert_line_endings() {
    local platform=$1
    local folder=$2
    case $platform in
         *linux)
            DOS2UNIX=fromdos
            ;;
         *)
            DOS2UNIX=dos2unix
            ;;
    esac

    find $folder -type f -name "*.*" -exec $DOS2UNIX {} \;
}

function cmi_unpack() {
    unzip ../../download/$FILE_URL

    pushd ${PRODUCT}-${VERSION}

    rm -rf Demos Extras UnitTests msvc

    # Convert line endings to unix style
    convert_line_endings $CONF_TARGET .

    popd
}

function cmi_configure() {
    pushd ${PRODUCT}-${VERSION}

    case $1 in
         *linux)
            # tested with cmake 3.5.0
            cmake -DCMAKE_BUILD_TYPE=RELEASE -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_DEMOS=OFF -DBUILD_EXTRAS=OFF
            ;;

         *win32)
            # tested with cmake 3.9
            if [ "$CONF_TARGET" == "win32" ]; then
                ARCH=""
            else
                ARCH=" Win64"
            fi
            cmake -G "Visual Studio 14 2015${ARCH}" . -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_DEMOS=OFF -DBUILD_EXTRAS=OFF
            ;;
         *)
            # tested with cmake 3.7.1
            cmake -G "Unix Makefiles" -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_DEMOS=OFF -DBUILD_EXTRAS=OFF
            ;;
    esac

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

case $CONF_TARGET in
    *win32)
        # visual studio projects can be built with cmake: https://stackoverflow.com/a/28370892
        function cmi_make() {
            set -e

            pushd ${PRODUCT}-${VERSION}
            cmake  --build . --config Release

            set +e

            # "install"
            mkdir -p $PREFIX/lib/$CONF_TARGET
            mkdir -p $PREFIX/bin/$CONF_TARGET
            mkdir -p $PREFIX/share/$CONF_TARGET
            mkdir -p $PREFIX/include/

            find ./lib/ -iname "*${LIB_SUFFIX}" -print0 | xargs -0 -I {} cp -v {} $PREFIX/lib/$CONF_TARGET

            popd
        }
        ;;
    *)
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
        ;;
esac

download
cmi $1
