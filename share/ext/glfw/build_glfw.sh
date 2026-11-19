#!/usr/bin/env bash
# Compilation guide:
# https://www.glfw.org/docs/latest/compile.html

readonly VERSION=3.4
readonly BASE_URL=https://github.com/glfw/glfw/releases/download/${VERSION}/
readonly FILE_BASE=glfw-${VERSION}
readonly FILE_URL=${FILE_BASE}.zip
readonly PRODUCT=glfw

. ../common.sh

PLATFORM=$1
PWD=$(pwd)
SOURCE_DIR=${PWD}/source
BUILD_DIR=${PWD}/build/${PLATFORM}

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi


CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_EXAMPLES=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_TESTS=OFF ${CMAKE_FLAGS}"
CMAKE_FLAGS="-DGLFW_BUILD_DOCS=OFF ${CMAKE_FLAGS}"

case $PLATFORM in
    arm64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    x86_64-macos)
        CMAKE_FLAGS="-DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION} ${CMAKE_FLAGS}"
        ;;
    arm64-linux)
        # Need to setup clang on linux
        cmi_setup_cc $PLATFORM
        ;;
    x86_64-linux)
        ;;
    *)
        echo "Unsupported GLFW build platform: ${PLATFORM}" >&2
        exit 1
        ;;
esac

function cmi_unpack() {
    echo "Unpacking $SCRIPTDIR/download/$FILE_URL"
    unzip -q -d ${SOURCE_DIR} $SCRIPTDIR/download/$FILE_URL

    mv $FILE_BASE/* .
}

function normalize_package_folders() {
    echo "Normalizing folder names..."
    # Rename folders to lowercase if incorrectly capitalized
    UPPER_PRODUCT=$(echo "$PRODUCT" | tr '[:lower:]' '[:upper:]')
    [ -d "include/${UPPER_PRODUCT}" ] && mv "include/${UPPER_PRODUCT}" "include/${PRODUCT}"
}

download

mkdir -p ${SOURCE_DIR}

pushd $SOURCE_DIR

cmi_unpack
cmi_patch

## BUILD
echo "CMAKE_FLAGS: '${CMAKE_FLAGS}"
cmake ${CMAKE_FLAGS} ${SOURCE_DIR}
cmake --build . --config Release

## PACKAGE
TARGET_LIB=lib/$PLATFORM

echo $TARGET_LIB

# clean out anything previously built
rm -rf ./lib/
mkdir -p $TARGET_LIB

cp -v src/libglfw3.a "${TARGET_LIB}/"

PACKAGE=glfw-${VERSION}-${PLATFORM}.tar.gz

normalize_package_folders
tar cfvz $PACKAGE lib include

popd

## FINALIZE
mv $SOURCE_DIR/$PACKAGE .

# rm -rf $SOURCE_DIR
