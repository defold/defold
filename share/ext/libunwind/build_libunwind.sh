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



export CONF_TARGET=$1

readonly PRODUCT=libunwind
readonly TAR_INCLUDES=1



. ../common.sh

function cmi_configure() {
    echo "No configure exists"
}

function cmi_unpack() {
    echo "No unpack exists"
}


case $CONF_TARGET in
    *armv7-android)
        # SHA1 of master as of 2018-12-07
        readonly VERSION=8ba86320a71bcdc7b411070c0c0f101cf2131cf2

        function cmi_make() {
            # Note: For Android 32/64, we have to rename libunwind since it is now shipped with the NDK
            #       and is not compatible with our code.
            echo -e "Checking out libunwind into libunwind_android"
            git clone https://android.googlesource.com/platform/external/libunwind libunwind_android
            pushd libunwind_android
            git checkout $VERSION

            echo -e "Moving jni makefiles"
            mkdir -p jni/
            cp ../../jni/* jni/

            echo -e "Applying patch files"
            patch -p1 -s -f < ../../android_changes.patch || exit 1

            echo -e "Building libunwind with ndk-build"
            ${ANDROID_NDK_ROOT}/ndk-build APP_ABI=armeabi-v7a NDK_PROJECTPATH="." APP_STL=c++_static NDK_DEBUG=0 -B -j16 || exit 1
            popd

            echo -e "Copying library to build folder"
            mkdir -p $PREFIX/lib/$CONF_TARGET
            mkdir -p $PREFIX/include/$CONF_TARGET/

            cp -v libunwind_android/obj/local/armeabi-v7a/libunwind_defold.a $PREFIX/lib/$CONF_TARGET/libunwind.a
            cp -vr libunwind_android/include/* $PREFIX/include/$CONF_TARGET/
        }
        ;;
    *arm64-android)
        # SHA1 of master as of 2018-12-07
        readonly VERSION=8ba86320a71bcdc7b411070c0c0f101cf2131cf2

        function cmi_make() {
            echo -e "Checking out libunwind into libunwind_android"
            git clone https://android.googlesource.com/platform/external/libunwind libunwind_android
            pushd libunwind_android
            git checkout $VERSION

            echo -e "Moving jni makefiles"
            mkdir -p jni/
            cp ../../jni/* jni/

            echo -e "Applying patch files"
            patch -p1 -s -f < ../../android_changes.patch || exit 1

            echo -e "Building libunwind with ndk-build"
            ${ANDROID_NDK_ROOT}/ndk-build NDK_PROJECTPATH="." APP_STL=c++_static NDK_DEBUG=0 -B -j16 || exit 1
            popd

            echo -e "Copying library to build folder"
            mkdir -p $PREFIX/lib/$CONF_TARGET
            mkdir -p $PREFIX/include/$CONF_TARGET/

            cp -v libunwind_android/obj/local/arm64-v8a/libunwind_defold.a  $PREFIX/lib/$CONF_TARGET/libunwind.a
            cp -vr libunwind_android/include/* $PREFIX/include/$CONF_TARGET/
        }
        ;;
    *)
        echo "Platform not implemented/supported"
        exit 1
        ;;
esac

cmi $1
