# config

ARM_DARWIN_ROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer
IOS_SDK_VERSION=5.1

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
		armv6-darwin)
            [ ! -e "$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" ] && echo "No SDK found at $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" && exit 1
            export CFLAGS="-isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CPPFLAGS="-arch armv6 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            export CXXFLAGS="-arch armv6 -isysroot $ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            # NOTE: We use the gcc-compiler as preprocessor. The preprocessor seems to only work with x86-arch.
            # Wrong include-directories and defines are selected.
            export CPP="$ARM_DARWIN_ROOT/usr/bin/llvm-gcc-4.2 -E"
            export CC=$ARM_DARWIN_ROOT/usr/bin/llvm-gcc-4.2
            export CXX=$ARM_DARWIN_ROOT/usr/bin/llvm-g++-4.2
            export AR=$ARM_DARWIN_ROOT/usr/bin/ar
            export RANLIN=$ARM_DARWIN_ROOT/usr/bin/ranlib
            cmi_cross $1 arm-darwin
            ;;

        darwin)
            export CPPFLAGS="-m32"
            export CXXFLAGS="-m32"
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
            export RANLIN=i586-mingw32msvc-ranlib
            cmi_cross $1 $1
            ;;

		*)
			echo "Unknown target $1" && exit 1
			;;
	esac
}
