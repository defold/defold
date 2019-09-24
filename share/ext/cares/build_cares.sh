#!/usr/bin/env bash

export CONF_TARGET=$1

# Build cares with debug info
export CONFIGURE_FLAGS="--enable-debug --disable-symbol-hiding"

readonly PRODUCT=cares
readonly TAR_INCLUDES=1
readonly VERSION=602aaec984f862a5d59c9eb022f4317954c53917 #804753224fc27f806992ed8c7fe52e5762fd317a
readonly BASE_URL=https://github.com/c-ares/c-ares.git

. ../common.sh

TAR_SKIP_BIN=1

function cmi_cleanup() {
	echo "Skipping cleanup.."
}

case $CONF_TARGET in
    *arm64-android)
        function cmi_unpack() {
            echo -e "Checking out cares into cares_android_arm64"
            git clone $BASE_URL cares_android_arm64
            pushd cares_android_arm64
            git checkout $VERSION

            # ./buildconf

            cmake -G "Unix Makefiles" -DCMAKE_C_COMPILER=$CC -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=DEBUG -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_DEMOS=OFF -DBUILD_EXTRAS=OFF -DCARES_STATIC=YES -DCMAKE_SYSROOT="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${platform}-x86_64/sysroot"
        }
        ;;
    *)
        echo "Platform not implemented/supported"
        exit 1
        ;;
esac

cmi $1
