#!/usr/bin/env bash
. ../common.sh

BUILD_DIR=build
PREFIX=`pwd`/$BUILD_DIR
PLATFORM=$1

PRODUCT=moltenvk
VERSION="${VULKAN_SDK##*/}"
TAR_SKIP_BIN=1

if [ -z "$PLATFORM" ]; then
    echo "No platform specified!"
    exit 1
fi

if [ -z "$VULKAN_SDK" ]; then
    echo "VULKAN_SDK must be set"
    exit 1
fi

if [ ! -d "${VULKAN_SDK}" ]; then
    echo "VULKAN_SDK is set, but doesn't exist"
fi

mkdir -p ${BUILD_DIR}
mkdir -p ${BUILD_DIR}/lib/$PLATFORM

pushd $BUILD_DIR

VULKAN_LIB_PATH=$VULKAN_SDK/macOS/lib
MOLTENVK_FRAMEWORK_PATH=$VULKAN_SDK/MoltenVK/MoltenVK.xcframework

case $PLATFORM in
    arm64-macos)
		lipo -thin arm64 $MOLTENVK_FRAMEWORK_PATH/macos-arm64_x86_64/libMoltenVK.a -o lib/$PLATFORM/libMoltenVK.a

		for f in $VULKAN_LIB_PATH/libvulkan*.dylib; do
			lipo -thin arm64 $f -o lib/$PLATFORM/${f##*/}
		done

        ;;
    x86_64-macos)
		lipo -thin x86_64 $MOLTENVK_FRAMEWORK_PATH/macos-arm64_x86_64/libMoltenVK.a -o lib/$PLATFORM/libMoltenVK.a

		for f in $VULKAN_LIB_PATH/libvulkan*.dylib; do
			lipo -thin x86_64 $f -o lib/$PLATFORM/${f##*/}
		done
        ;;
    arm64-ios)
		cp $MOLTENVK_FRAMEWORK_PATH/ios-arm64/libMoltenVK.a lib/$PLATFORM/
        ;;
esac

popd

cmi_package_platform $PLATFORM

cmi_cleanup
