#!/usr/bin/env bash

# Copyright 2020-2025 The Defold Foundation
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

_SOURCE="${BASH_SOURCE[0]:-$0}"
SCRIPTDIR="$( cd "$( dirname "${_SOURCE}" )" >/dev/null 2>&1 && pwd )"

eval $(python $SCRIPTDIR/../../build_tools/set_sdk_vars.py VERSION_IPHONEOS VERSION_IPHONESIMULATOR VERSION_IPHONEOS_MIN VERSION_MACOSX_MIN VERSION_MACOSX VERSION_XCODE PACKAGES_EMSCRIPTEN_SDK)

# config
IOS_SDK_VERSION=$VERSION_IPHONEOS
IOS_SIMULATOR_SDK_VERSION=$VERSION_IPHONESIMULATOR
IOS_MIN_SDK_VERSION=$VERSION_IPHONEOS_MIN

OSX_MIN_SDK_VERSION=$VERSION_MACOSX_MIN
OSX_SDK_VERSION=$VERSION_MACOSX
XCODE_VERSION=$VERSION_XCODE

OSX_SDK_ROOT=${DYNAMO_HOME}/ext/SDKs/MacOSX${OSX_SDK_VERSION}.sdk
IOS_SDK_ROOT=${DYNAMO_HOME}/ext/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk
IOS_SIMULATOR_SDK_ROOT=${DYNAMO_HOME}/ext/SDKs/iPhoneSimulator${IOS_SDK_VERSION}.sdk
DARWIN_TOOLCHAIN_ROOT=${DYNAMO_HOME}/ext/SDKs/XcodeDefault${XCODE_VERSION}.xctoolchain

EMSCRIPTEN_SDK_ROOT=${DYNAMO_HOME}/ext/SDKs/${PACKAGES_EMSCRIPTEN_SDK}
export EMSCRIPTEN_BIN_DIR=${EMSCRIPTEN_SDK_ROOT}/upstream/emscripten
export EM_CACHE=${EMSCRIPTEN_SDK_ROOT}/.defold_cache
export EM_CONFIG=${EMSCRIPTEN_SDK_ROOT}/.emscripten

readonly HOST_UNAME=$(uname)
readonly HOST_ARCH=$(arch)

HOST_PLATFORM=${HOST_UNAME}
if [ "Darwin" == "${HOST_PLATFORM}" ]; then
    HOST_PLATFORM="x86_64-macos"
    if [ "arm64" == "${HOST_ARCH}" ]; then
        HOST_PLATFORM="arm64-macos"
    fi
elif [ "Linux" == "${HOST_PLATFORM}" ]; then
    HOST_PLATFORM="x86_64-linux"
    if [ "${HOST_ARCH}" == "aarch64" ] || [ "${HOST_ARCH}" == "arm64" ]; then
        HOST_PLATFORM="arm64-linux"
    fi
elif [[ "${HOST_PLATFORM}" == MINGW* ]] || [[ "${HOST_PLATFORM}" == MSYS* ]] || [[ "${HOST_PLATFORM}" == CYGWIN* ]]; then
    HOST_PLATFORM="x86_64-win32"
    if [ "${HOST_ARCH}" == "i686" ] || [ "${HOST_ARCH}" == "i386" ]; then
        HOST_PLATFORM="win32"
    fi
fi

if [ "${HOST_PLATFORM}" == "${HOST_UNAME}" ]; then
    echo "Error setting HOST_PLATFORM: '${HOST_PLATFORM}'"
    exit 1
fi


if [ "Darwin" == "$(uname)" ]; then
    if [ ! -d "${DARWIN_TOOLCHAIN_ROOT}" ]; then
        DARWIN_TOOLCHAIN_ROOT=$(xcode-select -print-path)/Toolchains/XcodeDefault.xctoolchain
    fi
    if [ ! -d "${OSX_SDK_ROOT}" ]; then
        OSX_SDK_ROOT=$(xcrun -f --sdk macosx --show-sdk-path)
    fi
    if [ ! -d "${IOS_SDK_ROOT}" ]; then
        IOS_SDK_ROOT=$(xcrun -f --sdk iphoneos --show-sdk-path)
    fi
    if [ ! -d "${IOS_SIMULATOR_SDK_ROOT}" ]; then
        IOS_SIMULATOR_SDK_ROOT=$(xcrun -f --sdk iphonesimulator --show-sdk-path)
    fi
fi

ANDROID_NDK_VERSION=25b
ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-"${DYNAMO_HOME}/ext/SDKs/android-ndk-r${ANDROID_NDK_VERSION}"}

ANDROID_VERSION=19 # Android 4.4
ANDROID_64_VERSION=21 # Android 5.0

MAKEFILE=Makefile
MAKEFILE_ARGS=

# for win32/msys, try "wget --no-check-certificate -O $FILE_URL"
CURL="curl -L -O"

TAR_SKIP_BIN=0
TAR_INCLUDES=0

function download() {
    mkdir -p ../download
    [ ! -f ../download/$FILE_URL ] && $CURL $BASE_URL/$FILE_URL && mv $FILE_URL ../download
    echo "Downloaded to ../download/$FILE_URL"
}

function cmi_make() {
    set -e
    make -f $MAKEFILE $MAKEFILE_ARGS -j8
    make install
    set +e
}

function cmi_unpack() {
    echo "Unpacking $SCRIPTDIR/download/$FILE_URL"
    tar xfz $SCRIPTDIR/download/$FILE_URL --strip-components=1
}

function cmi_configure() {
    echo CONFIGURE_ARGS=$CONFIGURE_ARGS $2
    ${CONFIGURE_WRAPPER} ./configure $CONFIGURE_ARGS $2 \
        --disable-shared \
        --prefix=${PREFIX} \
        --bindir=${PREFIX}/bin/$1 \
        --libdir=${PREFIX}/lib/$1 \
        --with-http=off \
        --with-html=off \
        --with-ftp=off \
        --with-x=no
}

function cmi_patch() {
    set -e
    [ -f ../patch_$VERSION ] && echo "Applying patch ../patch_$VERSION" && patch --binary -p1 < ../patch_$VERSION
    set +e
}

function cmi_do() {
    rm -rf $PREFIX
    rm -rf tmp
    mkdir -p tmp
    mkdir -p $PREFIX
    pushd tmp  >/dev/null
    cmi_unpack
    cmi_patch
    cmi_configure $1 $2
    cmi_make
    popd >/dev/null
}

function cmi_package_common() {
    local TGZ_COMMON="$PRODUCT-$VERSION-common.tar.gz"
    pushd $PREFIX  >/dev/null
    tar cfvz $TGZ_COMMON include share
    popd >/dev/null

    [ ! -d ../build ] && mkdir ../build
    mv -v $PREFIX/$TGZ_COMMON ../build
    echo "../build/$TGZ_COMMON created"
}

function cmi_package_platform() {
    local TGZ="$PRODUCT-$VERSION-$1.tar.gz"
    pushd $PREFIX  >/dev/null
    if [ ${TAR_SKIP_BIN} -eq "1" ]; then
        tar cfvz $TGZ lib
    elif [ ${TAR_INCLUDES} -eq "1" ]; then
        tar cfvz $TGZ lib bin include
    else
        tar cfvz $TGZ lib bin
    fi
    popd >/dev/null

    [ ! -d ../build ] && mkdir ../build
    mv -v $PREFIX/$TGZ ../build
    echo "../build/$TGZ created"
}

function cmi_cleanup() {
    rm -rf tmp
    rm -rf $PREFIX
}

function cmi_cross() {
    if [[ $2 == "js-web" ]] || [[ $2 == "wasm-web" ]] || [[ $2 == "wasm_pthread-web" ]]; then
        # Cross compiling protobuf for js-web with --host doesn't work
        # Unknown host in reported by configure script
        cmi_do $1
    else
        cmi_do $1 "--host=$2"
    fi

    cmi_package_platform $1
    cmi_cleanup
}

function cmi_buildplatform() {
    cmi_do $1 ""
    cmi_package_common
    cmi_package_platform $1
    cmi_cleanup
}

# Trick to override functions
function save_function() {
    local ORIG_FUNC=$(declare -f $1)
    local NEWNAME_FUNC="$2${ORIG_FUNC#$1}"
    eval "$NEWNAME_FUNC"
}

function windows_path_to_posix() {
    echo "/$1" | sed -e 's/\\/\//g' -e 's/C:/c/' -e 's/ /\\ /g' -e 's/(/\\(/g' -e 's/)/\\)/g'
}

function path_to_posix() {
    echo "$1" | sed -e 's/\\/\//g' -e 's/C:/c/' -e 's/ /\\ /g' -e 's/(/\\(/g' -e 's/)/\\)/g'
}

function cmi_setup_vs2019_env() {
    # from https://stackoverflow.com/a/3272301

    # These lines will be installation-dependent.
    export VSINSTALLDIR='C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\'
    export WindowsSdkDir='C:\Program Files (x86)\Windows Kits\10\'
    export WindowsLibPath='C:\Program Files (x86)\Windows'
    export FrameworkDir32='C:\Windows\Microsoft.NET\Framework\'
    export FrameworkDir64='C:\Windows\Microsoft.NET\Framework64\'
    export UCRTVersion=10.0.19041.0
    export FrameworkVersion=v4.0.30319
    export VCToolsVersion=14.29.30037

    # The following should be largely installation-independent.
    export VCINSTALLDIR='${VSINSTALLDIR}VC\'
    export DevEnvDir='${VSINSTALLDIR}Common7\IDE\'

    local platform=$1
    shift

    arch="x64"
    export FrameworkDir=${FrameworkDir64}
    if [ "$platform" == "win32" ]; then
        arch="x86"
        export FrameworkDir=${FrameworkDir32}
    fi

    export INCLUDE="${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\ATLMFC\\include;${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\include;${WindowsSdkDir}\\include\\${UCRTVersion}\\ucrt;${WindowsSdkDir}\\include\\${UCRTVersion}\\shared;${WindowsSdkDir}\\include\\${UCRTVersion}\\um;${WindowsSdkDir}\\include\\${UCRTVersion}\\winrt;${WindowsSdkDir}\\include\\${UCRTVersion}\\cppwinrt"
    export LIB="${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\ATLMFC\\lib\\${arch};${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\lib\\${arch};${WindowsSdkDir}\\lib\\${UCRTVersion}\\ucrt\\${arch};${WindowsSdkDir}\\lib\\${UCRTVersion}\\um\\${arch}"
    export LIBPATH="${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\ATLMFC\\lib\\${arch};${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\lib\\${arch};${VSINSTALLDIR}\\VC\\Tools\\MSVC\\${VCToolsVersion}\\lib\\x86\\store\\references;${WindowsSdkDir}\\UnionMetadata\\${UCRTVersion};${WindowsSdkDir}\\References\\${UCRTVersion};${FrameworkDir64}\\${FrameworkVersion}"

    c_VSINSTALLDIR="$VSINSTALLDIR"
    c_WindowsSdkDir="$WindowsSdkDir"
    c_FrameworkDir64="$FrameworkDir"
    PATHSEP=";"

    local pathtype=$1
    shift

    if [ "$pathtype" == "bash" ]; then
        c_VSINSTALLDIR=$(windows_path_to_posix "$VSINSTALLDIR")
        c_WindowsSdkDir=$(windows_path_to_posix "$WindowsSdkDir")
        c_FrameworkDir64=$(windows_path_to_posix "$FrameworkDir")
        PATHSEP=":"
    fi

    echo BEFORE VSINSTALLDIR == $VSINSTALLDIR
    echo AFTER c_VSINSTALLDIR == $c_VSINSTALLDIR

    TMPPATH="${c_VSINSTALLDIR}Common7/IDE/Extensions/Microsoft/IntelliCode/CLI"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}VC/Tools/MSVC/${VCToolsVersion}/bin/Host${arch}/${arch}"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/IDE/VC/VCPackages"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/IDE/CommonExtensions/Microsoft/TestWindow"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/IDE/CommonExtensions/Microsoft/TeamFoundation/Team Explorer"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}MSBuild/Current/bin/Roslyn"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Team Tools/Performance Tools/${arch}"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Team Tools/Performance Tools"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/Tools/devinit"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}MSBuild/Current/Bin"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/IDE/"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/Tools/"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
    TMPPATH="${TMPPATH}${PATHSEP}${c_VSINSTALLDIR}Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    TMPPATH="${TMPPATH}${PATHSEP}${c_WindowsSdkDir}bin/${UCRTVersion}/${arch}"
    TMPPATH="${TMPPATH}${PATHSEP}${c_WindowsSdkDir}bin/${arch}"
    TMPPATH="${TMPPATH}${PATHSEP}${c_FrameworkDir64}${FrameworkVersion}"

    export PATH="$TMPPATH${PATHSEP}${PATH}"
}

function cmi_setup_cc() {
    case $1 in
        arm64-ios)
            [ ! -e "${IOS_SDK_ROOT}" ] && echo "No SDK found at ${IOS_SDK_ROOT}" && exit 1
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"
            export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
            export CPPFLAGS="-arch arm64 -isysroot ${IOS_SDK_ROOT}"
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export CXXFLAGS="${CXXFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION} -stdlib=libc++ -arch arm64 -isysroot ${IOS_SDK_ROOT}"
            export CFLAGS="${CPPFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION} "
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang
            export CXX=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++
            export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib
            ;;

        x86_64-ios)
            [ ! -e "${IOS_SIMULATOR_SDK_ROOT}" ] && echo "No SDK found at ${IOS_SIMULATOR_SDK_ROOT}" && exit 1
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"
            export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
            export CPPFLAGS="-arch x86_64 -target x86_64-apple-darwin19 -isysroot ${IOS_SIMULATOR_SDK_ROOT}"
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ -arch x86_64 -target x86_64-apple-darwin19 -isysroot ${IOS_SIMULATOR_SDK_ROOT}"
            export CFLAGS="${CPPFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION} "
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang
            export CXX=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++
            export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib
            ;;

         armv7-android)
            local platform=`uname | awk '{print tolower($0)}'`
            local llvm="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/bin"
            local sysroot="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/sysroot"
            #  -fstack-protector

            # Note: no-c++11-narrowing is added for the upgrade from NDK 10e to 20. Clang is much more vigilant than gcc,
            #       so to save time by not having to patch bullet (and others presumably) we skip narrowing.
            export CFLAGS="${CFLAGS} -isysroot ${sysroot} -fpic -ffunction-sections -funwind-tables -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__  -march=armv7-a -mfloat-abi=softfp -mfpu=vfp -mthumb -Os -fomit-frame-pointer -fno-strict-aliasing -DANDROID "
            export CPPFLAGS=${CFLAGS}
            export CXXFLAGS="${CXXFLAGS} -Wno-c++11-narrowing -stdlib=libc++ ${CFLAGS}"
            export LDFLAGS="-isysroot ${sysroot} -Wl,--fix-cortex-a8  -Wl,--no-undefined -Wl,-z,noexecstack"

            export CPP="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang -E"
            export CC="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang"
            export CXX="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang++"

            export AR="${llvm}/llvm-ar"
            export AS="${llvm}/llvm-as"
            export LD="${llvm}/lld"
            export RANLIB="${llvm}/llvm-ranlib"
            ;;

        arm64-android)
            local platform=`uname | awk '{print tolower($0)}'`
            local llvm="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/bin"
            local sysroot="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/sysroot"

            export CFLAGS="${CFLAGS} -isysroot ${sysroot} -fpic -ffunction-sections -funwind-tables -D__aarch64__  -march=armv8-a -Os -fomit-frame-pointer -fno-strict-aliasing -DANDROID "
            export CPPFLAGS=${CFLAGS}
            export CXXFLAGS="${CXXFLAGS} -Wno-c++11-narrowing -stdlib=libc++ ${CFLAGS}"
            export CPP="${llvm}/aarch64-linux-android${ANDROID_64_VERSION}-clang -E"
            export CC="${llvm}/aarch64-linux-android${ANDROID_64_VERSION}-clang"
            export CXX="${llvm}/aarch64-linux-android${ANDROID_64_VERSION}-clang++"

            export AR="${llvm}/llvm-ar"
            export AS="${llvm}/llvm-as"
            export LD="${llvm}/lld"
            export RANLIB="${llvm}/llvm-ranlib"
            ;;

        x86_64-macos)
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
            export SDKROOT="${OSX_SDK_ROOT}"
            export MACOSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION}
            export CFLAGS="${CFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} "
            export CXXFLAGS="${CXXFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} -stdlib=libc++ "
            export LDFLAGS="${LDFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION}"

            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="arch -x86_64 $DARWIN_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC="arch -x86_64 $DARWIN_TOOLCHAIN_ROOT/usr/bin/clang"
            export CXX="arch -x86_64 $DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++"
            export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib
            ;;

        arm64-macos)
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
            export SDKROOT="${OSX_SDK_ROOT}"
            export MACOSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION}
            export CFLAGS="${CFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} -arch arm64 -target arm64-apple-darwin19 -m64 "
            export CXXFLAGS="${CXXFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} -arch arm64 -target arm64-apple-darwin19 -m64 -stdlib=libc++ "
            export LDFLAGS="${LDFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION}"

            export CPP="$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC="$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang"
            export CXX="$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++"
            export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib
            ;;


        x86_64-linux)
            export CFLAGS="${CFLAGS} --target=x86_64-unknown-linux-gnu -fPIC"
            export CXXFLAGS="${CXXFLAGS} --target=x86_64-unknown-linux-gnu -fPIC"
            export CPPFLAGS="${CPPFLAGS} --target=x86_64-unknown-linux-gnu -fPIC"

            export CC=$(which clang)
            export CXX=$(which clang++)
            export AR=$(which ar)
            export RANLIB=$(which ranlib)
            export CPP="${CC} -E"
            ;;

        arm64-linux)
            export CFLAGS="${CFLAGS} --target=aarch64-unknown-linux-gnu -fPIC"
            export CXXFLAGS="${CXXFLAGS} --target=aarch64-unknown-linux-gnu -fPIC"
            export CPPFLAGS="${CPPFLAGS} --target=aarch64-unknown-linux-gnu -fPIC"

            export CC=$(which clang)
            export CXX=$(which clang++)
            export AR=$(which ar)
            export RANLIB=$(which ranlib)
            export CPP="${CC} -E"
            ;;

        win32)
            ;;

        x86_64-win32)
            ;;

        i586-mingw32msvc)
            export CPP=i586-mingw32msvc-cpp
            export CC=i586-mingw32msvc-gcc
            export CXX=i586-mingw32msvc-g++
            export AR=i586-mingw32msvc-ar
            export RANLIB=i586-mingw32msvc-ranlib
            ;;

        js-web|wasm-web)
            export CONFIGURE_WRAPPER=${EMSCRIPTEN_BIN_DIR}/emconfigure
            export CC=${EMSCRIPTEN_BIN_DIR}/emcc
            export CXX=${EMSCRIPTEN_BIN_DIR}/em++
            export AR=${EMSCRIPTEN_BIN_DIR}/emar
            export LD=${EMSCRIPTEN_BIN_DIR}/em++
            export RANLIB=${EMSCRIPTEN_BIN_DIR}/emranlib
            export CFLAGS="${CFLAGS} -fPIC -fno-exceptions"
            export CXXFLAGS="${CXXFLAGS} -fPIC -fno-exceptions"
            export CMAKE_TOOLCHAIN_FILE="${EMSCRIPTEN_BIN_DIR}/cmake/Modules/Platform/Emscripten.cmake"
            ;;

        wasm_pthread-web)
            export CONFIGURE_WRAPPER=${EMSCRIPTEN_BIN_DIR}/emconfigure
            export CC=${EMSCRIPTEN_BIN_DIR}/emcc
            export CXX=${EMSCRIPTEN_BIN_DIR}/em++
            export AR=${EMSCRIPTEN_BIN_DIR}/emar
            export LD=${EMSCRIPTEN_BIN_DIR}/em++
            export RANLIB=${EMSCRIPTEN_BIN_DIR}/emranlib
            export CFLAGS="${CFLAGS} -fPIC -fno-exceptions -pthread"
            export CXXFLAGS="${CXXFLAGS} -fPIC -fno-exceptions -pthread"
            export CMAKE_TOOLCHAIN_FILE="${EMSCRIPTEN_BIN_DIR}/cmake/Modules/Platform/Emscripten.cmake"
            ;;

        *)
            if [ -f "$SCRIPTDIR/common_private.sh" ]; then
                source $SCRIPTDIR/common_private.sh
                cmi_setup_cc_private $@
            else
                echo "Unknown target $1" && exit 1
            fi
            ;;
    esac
}

function cmi() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    cmi_setup_cc $1

    case $1 in
        armv7-android|arm64-android)
            cmi_cross $1 arm-linux
            ;;

        arm64-ios|x86_64-ios|armv7-android|arm64-android|js-web|wasm-web|wasm_pthread-web)
            cmi_cross $1 arm-ios
            ;;

        # desktop
        x86_64-macos|arm64-macos|x86_64-linux|arm64-linux|win32|x86_64-win32)
            cmi_buildplatform $1
            ;;

        *)
            if [ -f "$SCRIPTDIR/common_private.sh" ]; then
                source $SCRIPTDIR/common_private.sh
                cmi_private $@
            else
                echo "Unknown target $1" && exit 1
            fi
            ;;
    esac
}
