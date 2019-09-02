# config

IOS_TOOLCHAIN_ROOT=${DYNAMO_HOME}/ext/SDKs/XcodeDefault10.1.xctoolchain
ARM_DARWIN_ROOT=${DYNAMO_HOME}/ext
IOS_SDK_VERSION=12.1
IOS_SIMULATOR_SDK_VERSION=12.1

IOS_MIN_SDK_VERSION=6.0
OSX_MIN_SDK_VERSION=10.7

ANDROID_NDK_VERSION=20
ANDROID_NDK_ROOT=${DYNAMO_HOME}/ext/SDKs/android-ndk-r${ANDROID_NDK_VERSION}

ANDROID_VERSION=16 # Android 4.1
ANDROID_GCC_VERSION='4.9'

ANDROID_64_VERSION=21 # Android 5.0
ANDROID_64_GCC_VERSION='4.9'

FLASCC=~/local/FlasCC1.0/sdk

MAKEFILE=Makefile

# for win32/msys, try "wget --no-check-certificate -O $FILE_URL"
CURL="curl -L -O"

TAR_SKIP_BIN=0
TAR_INCLUDES=0

function download() {
    mkdir -p ../download
    [ ! -f ../download/$FILE_URL ] && $CURL $BASE_URL/$FILE_URL && mv $FILE_URL ../download
}

function cmi_make() {
    set -e
    make -f $MAKEFILE -j8
    make install
    set +e
}

function cmi_unpack() {
    tar xfz ../../download/$FILE_URL --strip-components=1
}

function cmi_configure() {
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
    mkdir tmp
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
    if [[ $2 == "js-web" ]] || [[ $2 == "wasm-web" ]]; then
        # Cross compiling protobuf for js-web with --host doesn't work
        # Unknown host in reported by configure script
        # TODO: Use another target, e.g. i386-freebsd as for as3-web?
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

function cmi_setup_vs2015_env() {
    # from https://stackoverflow.com/a/3272301

    # These lines will be installation-dependent.
    export VSINSTALLDIR='C:\Program Files (x86)\Microsoft Visual Studio 14.0\'
    export WindowsSdkDir='C:\Program Files (x86)\Windows Kits\8.0'
    export FrameworkDir='C:\WINDOWS\Microsoft.NET\Framework\'
    export FrameworkVersion=v4.0.30319
    export Framework35Version=v3.5

    # The following should be largely installation-independent.
    export VCINSTALLDIR="$VSINSTALLDIR"'VC\'
    export DevEnvDir="$VSINSTALLDIR"'Common7\IDE\'

    export FrameworkDIR32="$FrameworkDir"
    export FrameworkVersion32="$FrameworkVersion"

    export INCLUDE="${VCINSTALLDIR}INCLUDE;${WindowsSdkDir}include;"
    export LIB="${VCINSTALLDIR}LIB;${WindowsSdkDir}lib;"
    export LIBPATH="${FrameworkDir}${FrameworkVersion};"
    export LIBPATH="${LIBPATH}${FrameworkDir}${Framework35Version};"
    export LIBPATH="${LIBPATH}${VCINSTALLDIR}LIB;"

    c_VSINSTALLDIR=$(windows_path_to_posix "$VSINSTALLDIR")
    c_WindowsSdkDir=$(windows_path_to_posix "$WindowsSdkDir")
    c_FrameworkDir=$(windows_path_to_posix "$FrameworkDir")

    echo BEFORE VSINSTALLDIR == $VSINSTALLDIR
    echo BEFORE c_VSINSTALLDIR == $c_VSINSTALLDIR

    export PATH="${c_WindowsSdkDir}bin:$PATH"
    export PATH="${c_WindowsSdkDir}bin/NETFX 4.0 Tools:$PATH"
    export PATH="${c_VSINSTALLDIR}VC/VCPackages:$PATH"
    export PATH="${c_FrameworkDir}${Framework35Version}:$PATH"
    export PATH="${c_FrameworkDir}${FrameworkVersion}:$PATH"
    export PATH="${c_VSINSTALLDIR}Common7/Tools:$PATH"
    export PATH="${c_VSINSTALLDIR}VC/BIN:$PATH"
    export PATH="${c_VSINSTALLDIR}Common7/IDE/:$PATH"
}

function cmi() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    case $1 in
        armv7-darwin)
            [ ! -e "$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" ] && echo "No SDK found at $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" && exit 1
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"
            export PATH=$IOS_TOOLCHAIN_ROOT/usr/bin:$PATH
            export CPPFLAGS="-arch armv7 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CXXFLAGS="${CXXFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION} -stdlib=libc++ -arch armv7 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CFLAGS="${CPPFLAGS}"
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$IOS_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC=$IOS_TOOLCHAIN_ROOT/usr/bin/clang
            export CXX=$IOS_TOOLCHAIN_ROOT/usr/bin/clang++
            export AR=$IOS_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$IOS_TOOLCHAIN_ROOT/usr/bin/ranlib
            cmi_cross $1 arm-darwin
            ;;

        arm64-darwin)
            # Essentially the same environment vars as armv7-darwin but with "-arch arm64".

            [ ! -e "$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" ] && echo "No SDK found at $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" && exit 1
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"
            export PATH=$IOS_TOOLCHAIN_ROOT/usr/bin:$PATH
            export CPPFLAGS="-arch arm64 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export CXXFLAGS="${CXXFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION} -stdlib=libc++ -arch arm64 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CFLAGS="${CPPFLAGS}"
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$IOS_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC=$IOS_TOOLCHAIN_ROOT/usr/bin/clang
            export CXX=$IOS_TOOLCHAIN_ROOT/usr/bin/clang++
            export AR=$IOS_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$IOS_TOOLCHAIN_ROOT/usr/bin/ranlib
            cmi_cross $1 arm-darwin
            ;;

        x86_64-ios)
            [ ! -e "$ARM_DARWIN_ROOT/SDKs/iPhoneSimulator${IOS_SIMULATOR_SDK_VERSION}.sdk" ] && echo "No SDK found at $ARM_DARWIN_ROOT/SDKs/iPhoneSimulator${IOS_SIMULATOR_SDK_VERSION}.sdk" && exit 1
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"
            export PATH=$IOS_TOOLCHAIN_ROOT/usr/bin:$PATH
            export CPPFLAGS="-arch x86_64 -target x86_64-apple-darwin12 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneSimulator${IOS_SIMULATOR_SDK_VERSION}.sdk"
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ -arch x86_64 -target x86_64-apple-darwin12 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneSimulator${IOS_SIMULATOR_SDK_VERSION}.sdk"
            export CFLAGS="${CPPFLAGS}"
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$IOS_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC=$IOS_TOOLCHAIN_ROOT/usr/bin/clang
            export CXX=$IOS_TOOLCHAIN_ROOT/usr/bin/clang++
            export AR=$IOS_TOOLCHAIN_ROOT/usr/bin/ar
            export RANLIB=$IOS_TOOLCHAIN_ROOT/usr/bin/ranlib
            # cmi_buildplatform $1
            cmi_cross $1 x86_64-darwin
            ;;

         armv7-android)
            local platform=`uname | awk '{print tolower($0)}'`
            local bin="${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-${ANDROID_GCC_VERSION}/prebuilt/${platform}-x86_64/bin"
            local llvm="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/bin"
            local sysroot="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/sysroot"
            #  -fstack-protector

            # Note: no-c++11-narrowing is added for the upgrade from NDK 10e to 20. Clang is much more vigilant than gcc,
            #       so to save time by not having to patch bullet (and others presumably) we skip narrowing.
            export CFLAGS="${CFLAGS} -isysroot ${sysroot} -fpic -ffunction-sections -funwind-tables -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__  -march=armv7-a -mfloat-abi=softfp -mfpu=vfp -mthumb -Os -fomit-frame-pointer -fno-strict-aliasing -DANDROID -Wno-c++11-narrowing"
            export CPPFLAGS=${CFLAGS}
            export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ ${CFLAGS}"
            export LDFLAGS="-isysroot ${sysroot} -Wl,--fix-cortex-a8  -Wl,--no-undefined -Wl,-z,noexecstack"

            export CPP="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang -E"
            export CC="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang"
            export CXX="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang++"
            export AR=${bin}/arm-linux-androideabi-ar
            export AS=${bin}/arm-linux-androideabi-as
            export LD=${bin}/arm-linux-androideabi-ld
            export RANLIB=${bin}/arm-linux-androideabi-ranlib
            cmi_cross $1 arm-linux
            ;;

        arm64-android)
            local platform=`uname | awk '{print tolower($0)}'`
            local bin="${ANDROID_NDK_ROOT}/toolchains/aarch64-linux-android-${ANDROID_64_GCC_VERSION}/prebuilt/${platform}-x86_64/bin"
            local llvm="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/bin"
            local sysroot="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/sysroot"

            export CFLAGS="${CFLAGS} -isysroot ${sysroot} -fpic -ffunction-sections -funwind-tables -D__aarch64__  -march=armv8-a -Os -fomit-frame-pointer -fno-strict-aliasing -DANDROID -Wno-c++11-narrowing"
            export CPPFLAGS=${CFLAGS}
            export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ ${CFLAGS}"
            export CPP="${llvm}/aarch64-linux-android${ANDROID_64_VERSION}-clang -E"
            export CC="${llvm}/aarch64-linux-android${ANDROID_64_VERSION}-clang"
            export CXX="${llvm}/aarch64-linux-android${ANDROID_64_VERSION}-clang++"
            export AR=${bin}/aarch64-linux-android-ar
            export AS=${bin}/aarch64-linux-android-as
            export LD=${bin}/aarch64-linux-android-ld
            export RANLIB=${bin}/aarch64-linux-android-ranlib
            cmi_cross $1 arm-linux
            ;;

        darwin)
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export CPPFLAGS="-m32"
            export CXXFLAGS="${CXXFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} -m32 -stdlib=libc++ "
            export CFLAGS="${CFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} -m32"
            export LDFLAGS="-m32"
            cmi_buildplatform $1
            ;;

        x86_64-darwin)
            # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
            # Force libstdc++ for now
            export CXXFLAGS="${CXXFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} -stdlib=libc++ "
            cmi_buildplatform $1
            ;;

        linux)
            export CPPFLAGS="-m32 -fPIC"
            export CXXFLAGS="${CXXFLAGS} -m32 -fPIC"
            export CFLAGS="${CFLAGS} -m32 -fPIC"
            export LDFLAGS="-m32"
            cmi_buildplatform $1
            ;;

        x86_64-linux)
            export CFLAGS="${CFLAGS} -fPIC"
            export CXXFLAGS="${CXXFLAGS} -fPIC"
            export CPPFLAGS="${CPPFLAGS} -fPIC"
            cmi_buildplatform $1
            ;;

        win32)
            cmi_buildplatform $1
            ;;

        x86_64-win32)
            cmi_buildplatform $1
            ;;

        i586-mingw32msvc)
            export CPP=i586-mingw32msvc-cpp
            export CC=i586-mingw32msvc-gcc
            export CXX=i586-mingw32msvc-g++
            export AR=i586-mingw32msvc-ar
            export RANLIB=i586-mingw32msvc-ranlib
            cmi_cross $1 $1
            ;;

        js-web)
            export CONFIGURE_WRAPPER=${EMSCRIPTEN}/emconfigure
            export CC=${EMSCRIPTEN}/emcc
            export CXX=${EMSCRIPTEN}/em++
            export AR=${EMSCRIPTEN}/emar
            export LD=${EMSCRIPTEN}/em++
            export RANLIB=${EMSCRIPTEN}/emranlib
            cmi_cross $1 $1
            ;;

        wasm-web)
            export CONFIGURE_WRAPPER=${EMSCRIPTEN}/emconfigure
            export CC=${EMSCRIPTEN}/emcc
            export CXX=${EMSCRIPTEN}/em++
            export AR=${EMSCRIPTEN}/emar
            export LD=${EMSCRIPTEN}/em++
            export RANLIB=${EMSCRIPTEN}/emranlib
            cmi_cross $1 $1
            ;;

        as3-web)
            export CPP="$FLASCC/usr/bin/cpp"
            export CC=$FLASCC/usr/bin/gcc
            export CXX=$FLASCC/usr/bin/g++
            export AR=$FLASCC/usr/bin/ar
            export RANLIB=$FLASCC/usr/bin/ranlib
            # NOTE: We use a fake platform in order to make configure-scripts happy
            cmi_cross $1 i386-freebsd
            ;;

        *)
            echo "Unknown target $1" && exit 1
            ;;
    esac
}
