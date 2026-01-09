#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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
    x86_64-ios)
		lipo -thin x86_64 $MOLTENVK_FRAMEWORK_PATH/ios-arm64_x86_64-simulator/libMoltenVK.a -o lib/$PLATFORM/libMoltenVK.a
        ;;
esac

popd

cmi_package_platform $PLATFORM

cmi_cleanup
