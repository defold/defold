# config

IOS_TOOLCHAIN_ROOT=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain
ARM_DARWIN_ROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer
IOS_SDK_VERSION=6.0

ANDROID_ROOT=~/android
ANDROID_NDK_VERSION=8b
ANDROID_VERSION=14
ANDROID_GCC_VERSION='4.6'

function download() {
    mkdir -p ../download
	[ ! -f ../download/$FILE_URL ] && curl -O $BASE_URL/$FILE_URL && mv $FILE_URL ../download
}

function cmi_do() {
    rm -rf $PREFIX
    rm -rf tmp
    mkdir tmp
    mkdir -p $PREFIX
    pushd tmp  >/dev/null
    tar xfz ../../download/$FILE_URL --strip-components=1
    [ -f ../patch_$VERSION ] && echo "Applying patch ../patch_$VERSION" && patch -p1 < ../patch_$VERSION

    ./configure $CONFIGURE_ARGS $2 \
        --disable-shared \
        --prefix=${PREFIX} \
        --bindir=${PREFIX}/bin/$1 \
        --libdir=${PREFIX}/lib/$1 \
        --with-http=off \
        --with-html=off \
        --with-ftp=off \
        --with-x=no

    set -e
    make
    make install
    set +e
}

function cmi_cross() {
    cmi_do $1 "--host=$2"

    local TGZ="$PRODUCT-$VERSION-$1.tar.gz"
    pushd $PREFIX  >/dev/null
    tar cfz $TGZ lib
    popd >/dev/null
    popd >/dev/null

    echo "../build/$TGZ created"
    mv $PREFIX/$TGZ ../build

    rm -rf tmp
    rm -rf $PREFIX
}

function cmi_buildplatform() {
    cmi_do $1 ""

    local TGZ="$PRODUCT-$VERSION-$1.tar.gz"
    local TGZ_COMMON="$PRODUCT-$VERSION-common.tar.gz"
    pushd $PREFIX  >/dev/null
    tar cfz $TGZ lib
    tar cfz $TGZ_COMMON include share
    popd >/dev/null
    popd >/dev/null

    mv $PREFIX/$TGZ ../build
    echo "../build/$TGZ created"
    mv $PREFIX/$TGZ_COMMON ../build
    echo "../build/$TGZ_COMMON created"

    rm -rf tmp
    rm -rf $PREFIX
}

function cmi() {
    export PREFIX=`pwd`/build

	case $1 in
		armv7-darwin)
            [ ! -e "$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" ] && echo "No SDK found at $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" && exit 1
            # NOTE: We set this PATH in order to use libtool from iOS SDK
            # Otherwise we get the following error "malformed object (unknown load command 1)"
            export PATH=$IOS_TOOLCHAIN_ROOT/usr/bin:$PATH
            export CFLAGS="${CFLAGS} -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CPPFLAGS="-arch armv7 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CXXFLAGS="${CXXFLAGS} -arch armv7 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$IOS_TOOLCHAIN_ROOT/usr/bin/clang -E"
            export CC=$IOS_TOOLCHAIN_ROOT/usr/bin/clang
            export CXX=$IOS_TOOLCHAIN_ROOT/usr/bin/clang++
            export AR=$ARM_DARWIN_ROOT/usr/bin/ar
            export RANLIB=$ARM_DARWIN_ROOT/usr/bin/ranlib
            cmi_cross $1 arm-darwin
            ;;

         armv7-android)
            local platform=`uname | awk '{print tolower($0)}'`
            local bin="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/toolchains/arm-linux-androideabi-${ANDROID_GCC_VERSION}/prebuilt/${platform}-x86/bin"
            local sysroot="--sysroot=${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/platforms/android-${ANDROID_VERSION}/arch-arm"
            #  -fstack-protector
#            local stl="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/sources/cxx-stl/stlport/stlport"
            local stl="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/sources/cxx-stl/gnu-libstdc++/${ANDROID_GCC_VERSION}/include"
            local stl_lib="${ANDROID_ROOT}/android-ndk-r${ANDROID_NDK_VERSION}/sources/cxx-stl/gnu-libstdc++/${ANDROID_GCC_VERSION}/libs/armeabi-v7a"
            local stl_arch="${stl_lib}/include"

            export CFLAGS="${CFLAGS} ${sysroot} -fpic -ffunction-sections -funwind-tables -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__  -Wno-psabi -march=armv7-a -mfloat-abi=softfp -mfpu=vfp -mthumb -Os -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 -DANDROID -Wa,--noexecstack"
            export CPPFLAGS=${CFLAGS}
            export CXXFLAGS="${CXXFLAGS} -I${stl} -I${stl_arch} ${CFLAGS}"
            export LDFLAGS="${sysroot} -Wl,--fix-cortex-a8  -Wl,--no-undefined -Wl,-z,noexecstack -L${stl_lib} -lgnustl_static -lsupc++"
            export CPP=${bin}/arm-linux-androideabi-cpp
            export CC=${bin}/arm-linux-androideabi-gcc
            export CXX=${bin}/arm-linux-androideabi-g++
            export AR=${bin}/arm-linux-androideabi-ar
            export RANLIB=${bin}/arm-linux-androideabi-ranlib
            cmi_cross $1 arm-linux
            ;;

        darwin)
            export CPPFLAGS="-m32"
            export CXXFLAGS="${CXXFLAGS} -m32"
            cmi_buildplatform $1
            ;;

        x86_64-darwin)
            cmi_buildplatform $1
            ;;

        linux)
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

		*)
			echo "Unknown target $1" && exit 1
			;;
	esac
}
