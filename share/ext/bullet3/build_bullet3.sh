#!/usr/bin/env bash

readonly PRODUCT=bullet3
readonly VERSION=2.88
readonly BASE_URL=https://github.com/bulletphysics/bullet3/archive/
readonly FILE_URL=${VERSION}.tar.gz

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

function cmi_configure() {

    echo cmi_configure
    echo "pwd=" `pwd`

    cmi_set_compilers $1

    echo cmi_set_compilers
    echo "pwd=" `pwd`

    OPTIONS="-DCMAKE_BUILD_TYPE=RELEASE -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_PYBULLET=OFF -DBUILD_BULLET2_DEMOS=OFF -DBUILD_UNIT_TESTS=OFF -DBUILD_EXTRAS=OFF"
    COMPILERS="-DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR  -DCMAKE_RANLIB=$RANLIB"
    case $1 in
         *linux)
            # tested with cmake 3.5.0
            cmake $OPTIONS
            ;;

         *win32)
            # tested with cmake 3.9
            if [ "$CONF_TARGET" == "win32" ]; then
                ARCH=""
            else
                ARCH=" Win64"
            fi
            CXXFLAGS="${CXXFLAGS} /NODEFAULTLIB:library"
            CFLAGS="${CXXFLAGS} /NODEFAULTLIB:library"
            cmake -G "Visual Studio 14 2015${ARCH}" . $OPTIONS -DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF
            ;;
         *android)
            # tested with cmake 3.13.2
            cmake -G "Unix Makefiles" $OPTIONS $COMPILERS -DCMAKE_OSX_SYSROOT=
            ;;
         x86_64-ios)
            cmake -G "Unix Makefiles" $OPTIONS $COMPILERS -DCMAKE_OSX_SYSROOT=$SYSROOT -DCMAKE_FRAMEWORK_PATH=$FRAMEWORKS
            ;;
         *-darwin)
            cmake -G "Unix Makefiles" $OPTIONS $COMPILERS -DCMAKE_OSX_SYSROOT=$SYSROOT -DCMAKE_FRAMEWORK_PATH=$FRAMEWORKS
            ;;
         *)
            # tested with cmake 3.7.1
            cmake -G "Unix Makefiles" $OPTIONS $COMPILERS
            ;;
    esac

    echo cmi_configure end
    echo "pwd=" `pwd`

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

            cmake  --build . --config Release

            set +e

            # "install"
            mkdir -p $PREFIX/lib/$CONF_TARGET
            mkdir -p $PREFIX/bin/$CONF_TARGET
            mkdir -p $PREFIX/share/$CONF_TARGET
            mkdir -p $PREFIX/include/

            find ./lib -iname "*${LIB_SUFFIX}" -print0 | xargs -0 -I {} cp -v {} $PREFIX/lib/$CONF_TARGET

            pushd $PREFIX/lib/$CONF_TARGET
            find . -iname "*${LIB_SUFFIX}" -exec sh -c 'x="{}"; mv {} lib$(basename $x)' \;
            popd
        }
        ;;
    *)
        function cmi_make() {

            echo cmi_make
            echo "pwd=" `pwd`

            set -e

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
        }
        ;;
esac

download
cmi $1
