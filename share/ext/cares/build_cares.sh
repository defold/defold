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

function cmi_unpack() {
    git clone $BASE_URL ${PRODUCT}-${VERSION}
    pushd ${PRODUCT}-${VERSION}
    git checkout $VERSION
}

# If we need patching, do it here, or fix the paths. We enter the build folder in cmi_unpack,
# and leave it in cmi_make, so the patch path is not valid from common.sh at this point.
function cmi_patch() {
    echo -e "Skipping patch.."
}

function cmi_configure() {
    set -e
    case $CONF_TARGET in
        *armv7-darwin)
            cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DCMAKE_OSX_SYSROOT="$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" -DCMAKE_C_COMPILER=$CC -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_BUILD_TOOLS=OFF -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DCARES_STATIC=YES -DCMAKE_IOS_SDK_ROOT="$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            ;;
        *arm64-darwin)
            cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DCMAKE_OSX_SYSROOT="$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk" -DCMAKE_C_COMPILER=$CC -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_BUILD_TOOLS=OFF -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DCARES_STATIC=YES -DCMAKE_IOS_SDK_ROOT="$ARM_DARWIN_ROOT/SDKs/iPhoneOS${IOS_SDK_VERSION}.sdk"
            ;;
        *darwin)
            cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DCMAKE_OSX_SYSROOT="$ARM_DARWIN_ROOT/SDKs/MacOSX${OSX_SDK_VERSION}.sdk" -DCMAKE_C_COMPILER=$CC -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_BUILD_TOOLS=OFF -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DCARES_STATIC=YES
            ;;
        *x86_64-darwin)
            cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DCMAKE_OSX_SYSROOT="$ARM_DARWIN_ROOT/SDKs/MacOSX${OSX_SDK_VERSION}.sdk" -DCMAKE_C_COMPILER=$CC -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_BUILD_TOOLS=OFF -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DCARES_STATIC=YES
            ;;
        *)
            cmake -G "Unix Makefiles" -DCMAKE_C_COMPILER=$CC -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_BUILD_TOOLS=OFF -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR -DCMAKE_LD=$LD -DCMAKE_RANLIB=$RANLIB -DCMAKE_BUILD_TYPE=RELEASE -DCARES_STATIC=YES
            exit 1
            ;;
    esac
    set +e
}

function cmi_make() {
    set -e
    make -f $MAKEFILE -j8
    make install
    set +e
    popd
}

cmi $1
