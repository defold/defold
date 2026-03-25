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

# NOTE: This script is only used for CI
# The corresponding file for development is build.xml
#
# POSIX script dir (CI may run with `sh`, not bash).

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"

set -e

# Copy when present (optional tools not in every dynamo_home snapshot).
copy_if_exists() {
    if [ -f "$1" ]; then
        cp -v "$1" "$2"
    fi
}
mkdir -p lib/x86_64-linux
mkdir -p lib/arm64-linux
mkdir -p lib/x86_64-macos
mkdir -p lib/arm64-macos
mkdir -p lib/x86-win32
mkdir -p lib/x86_64-win32
mkdir -p libexec

mkdir -p libexec/x86_64-linux
mkdir -p libexec/arm64-linux
mkdir -p libexec/x86_64-macos
mkdir -p libexec/arm64-macos
# mkdir -p libexec/x86-win32
mkdir -p libexec/x86_64-win32
mkdir -p libexec/arm64-ios
mkdir -p libexec/x86_64-ios
# mkdir -p libexec/armv7-android
mkdir -p libexec/arm64-android
mkdir -p libexec/js-web
mkdir -p libexec/wasm-web
mkdir -p libexec/wasm_pthread-web

# Android bundletool (from common package bundletool-all-common.tar.gz → ext/share/java/)
cp -v "$DYNAMO_HOME/ext/share/java/bundletool-all.jar" libexec/bundletool-all.jar

SHA1=`git log --pretty=%H -n1`

# SPIRV toolchain
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/glslang libexec/x86_64-macos/glslang
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/glslang libexec/arm64-macos/glslang
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/glslang libexec/x86_64-linux/glslang
cp -v $DYNAMO_HOME/ext/bin/arm64-linux/glslang libexec/arm64-linux/glslang
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/glslang.exe libexec/x86_64-win32/glslang.exe

# Tint (wgsl toolchain)
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/tint libexec/x86_64-macos/tint
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/tint libexec/arm64-macos/tint
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/tint libexec/x86_64-linux/tint
cp -v $DYNAMO_HOME/ext/bin/arm64-linux/tint libexec/arm64-linux/tint
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/tint.exe libexec/x86_64-win32/tint.exe

# glTF validator
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/gltf_validator libexec/x86_64-macos/gltf_validator
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/gltf_validator libexec/arm64-macos/gltf_validator
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/gltf_validator libexec/x86_64-linux/gltf_validator
cp -v $DYNAMO_HOME/ext/bin/arm64-linux/gltf_validator libexec/arm64-linux/gltf_validator
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/gltf_validator.exe libexec/x86_64-win32/gltf_validator.exe

#
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/builtins.zip lib/builtins.zip

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-android/classes.dex lib/classes.dex
cp -v $DYNAMO_HOME/ext/share/java/android.jar lib/android.jar

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/dlib.jar lib/dlib.jar
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/modelimporter.jar lib/modelimporter.jar
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/shaderc.jar lib/shaderc.jar
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/texturecompiler.jar lib/texturecompiler.jar

# TEXC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libtexc_shared.so lib/x86_64-linux/libtexc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-linux/libtexc_shared.so lib/arm64-linux/libtexc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/texc_shared.dll lib/x86_64-win32/texc_shared.dll
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-macos/libtexc_shared.dylib lib/x86_64-macos/libtexc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-macos/libtexc_shared.dylib lib/arm64-macos/libtexc_shared.dylib

# MODELC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libmodelc_shared.so lib/x86_64-linux/libmodelc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-linux/libmodelc_shared.so lib/arm64-linux/libmodelc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/modelc_shared.dll lib/x86_64-win32/modelc_shared.dll
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-macos/libmodelc_shared.dylib lib/x86_64-macos/libmodelc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-macos/libmodelc_shared.dylib lib/arm64-macos/libmodelc_shared.dylib

# SHADERC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libshaderc_shared.so lib/x86_64-linux/libshaderc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-linux/libshaderc_shared.so lib/arm64-linux/libshaderc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/shaderc_shared.dll lib/x86_64-win32/shaderc_shared.dll
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-macos/libshaderc_shared.dylib lib/x86_64-macos/libshaderc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-macos/libshaderc_shared.dylib lib/arm64-macos/libshaderc_shared.dylib


cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/luajit-64 libexec/x86_64-linux/luajit-64
cp -v $DYNAMO_HOME/ext/bin/arm64-linux/luajit-64 libexec/arm64-linux/luajit-64
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/luajit-64 libexec/x86_64-macos/luajit-64
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/luajit-64 libexec/arm64-macos/luajit-64
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/luajit-64.exe libexec/x86_64-win32/luajit-64.exe
jar cfM lib/luajit-share.zip -C $DYNAMO_HOME/ext/share/ luajit

#LIPO
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/lipo libexec/x86_64-macos/lipo
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/lipo libexec/arm64-macos/lipo
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/lipo libexec/x86_64-linux/lipo
cp -v $DYNAMO_HOME/ext/bin/arm64-linux/lipo libexec/arm64-linux/lipo
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/lipo.exe libexec/x86_64-win32/lipo.exe

# Keep in sync with build_tools/sdk.py (ANDROID_BUILD_TOOLS_VERSION / ANDROID_NDK_VERSION).
ANDROID_BT="${ANDROID_BUILD_TOOLS_VERSION:-36.1.0}"
ANDROID_SDK_BT="$DYNAMO_HOME/ext/SDKs/android-sdk/build-tools/$ANDROID_BT"
NDK_ROOT="$DYNAMO_HOME/ext/SDKs/android-ndk-r25b"

# Optional ext/bin (luajit-32, spirv-cross CLI, Ogg tools — when installed for that platform).
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-linux/luajit-32" libexec/x86_64-linux/luajit-32
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-linux/luajit-32" libexec/arm64-linux/luajit-32
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-macos/luajit-32" libexec/x86_64-macos/luajit-32
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-macos/luajit-32" libexec/arm64-macos/luajit-32
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-win32/luajit-32.exe" libexec/x86_64-win32/luajit-32.exe

copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-linux/spirv-cross" libexec/x86_64-linux/spirv-cross
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-linux/spirv-cross" libexec/arm64-linux/spirv-cross
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-macos/spirv-cross" libexec/x86_64-macos/spirv-cross
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-macos/spirv-cross" libexec/arm64-macos/spirv-cross
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-win32/spirv-cross.exe" libexec/x86_64-win32/spirv-cross.exe

copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-linux/libogg.so" libexec/x86_64-linux/libogg.so
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-linux/libogg.so" libexec/arm64-linux/libogg.so
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-macos/libogg.dylib" libexec/x86_64-macos/libogg.dylib
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-macos/libogg.dylib" libexec/arm64-macos/libogg.dylib
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-win32/libogg.dll" libexec/x86_64-win32/libogg.dll

copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-linux/liboggz.so" libexec/x86_64-linux/liboggz.so
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-linux/liboggz.so" libexec/arm64-linux/liboggz.so
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-macos/liboggz.dylib" libexec/x86_64-macos/liboggz.dylib
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-macos/liboggz.dylib" libexec/arm64-macos/liboggz.dylib
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-win32/liboggz.dll" libexec/x86_64-win32/liboggz.dll

copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-linux/oggz-validate" libexec/x86_64-linux/oggz-validate
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-linux/oggz-validate" libexec/arm64-linux/oggz-validate
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-macos/oggz-validate" libexec/x86_64-macos/oggz-validate
copy_if_exists "$DYNAMO_HOME/ext/bin/arm64-macos/oggz-validate" libexec/arm64-macos/oggz-validate
copy_if_exists "$DYNAMO_HOME/ext/bin/x86_64-win32/oggz-validate.exe" libexec/x86_64-win32/oggz-validate.exe

# Android SDK build-tools (host matches CI machine — see README_ANDROID.md).
if [ -f "$ANDROID_SDK_BT/aapt2" ] || [ -f "$ANDROID_SDK_BT/aapt2.exe" ]; then
    case "$(uname -s)_$(uname -m)" in
        Linux_x86_64)
            mkdir -p libexec/x86_64-linux/lib
            copy_if_exists "$ANDROID_SDK_BT/aapt2" libexec/x86_64-linux/aapt2
            copy_if_exists "$ANDROID_SDK_BT/zipalign" libexec/x86_64-linux/zipalign
            copy_if_exists "$ANDROID_SDK_BT/lib64/libc++.so" libexec/x86_64-linux/lib/libc++.so
            ;;
        Linux_aarch64)
            mkdir -p libexec/arm64-linux/lib
            copy_if_exists "$ANDROID_SDK_BT/aapt2" libexec/arm64-linux/aapt2
            copy_if_exists "$ANDROID_SDK_BT/zipalign" libexec/arm64-linux/zipalign
            copy_if_exists "$ANDROID_SDK_BT/lib64/libc++.so" libexec/arm64-linux/lib/libc++.so
            ;;
        Darwin_arm64)
            copy_if_exists "$ANDROID_SDK_BT/aapt2" libexec/arm64-macos/aapt2
            copy_if_exists "$ANDROID_SDK_BT/zipalign" libexec/arm64-macos/zipalign
            ;;
        Darwin_x86_64)
            copy_if_exists "$ANDROID_SDK_BT/aapt2" libexec/x86_64-macos/aapt2
            copy_if_exists "$ANDROID_SDK_BT/zipalign" libexec/x86_64-macos/zipalign
            ;;
        MINGW*|MSYS*)
            copy_if_exists "$ANDROID_SDK_BT/aapt2.exe" libexec/x86_64-win32/aapt2.exe
            copy_if_exists "$ANDROID_SDK_BT/zipalign.exe" libexec/x86_64-win32/zipalign.exe
            ;;
    esac
fi

# NDK llvm-strip as strip_android (names expected by AndroidBundler).
ndk_tag=""
case "$(uname -s)_$(uname -m)" in
    Linux_x86_64) ndk_tag=linux-x86_64 ;;
    Linux_aarch64) ndk_tag=linux-aarch64 ;;
    Darwin_*) ndk_tag=darwin-x86_64 ;;
    MINGW*|MSYS*) ndk_tag=windows-x86_64 ;;
esac
if [ -n "$ndk_tag" ]; then
    ndk_bin="$NDK_ROOT/toolchains/llvm/prebuilt/$ndk_tag/bin"
    if [ -f "$ndk_bin/llvm-strip" ]; then
        case "$(uname -s)_$(uname -m)" in
            Linux_x86_64)
                cp -v "$ndk_bin/llvm-strip" libexec/x86_64-linux/strip_android
                cp -v "$ndk_bin/llvm-strip" libexec/x86_64-linux/strip_android_aarch64
                ;;
            Linux_aarch64)
                cp -v "$ndk_bin/llvm-strip" libexec/arm64-linux/strip_android
                cp -v "$ndk_bin/llvm-strip" libexec/arm64-linux/strip_android_aarch64
                ;;
            Darwin_arm64)
                cp -v "$ndk_bin/llvm-strip" libexec/arm64-macos/strip_android
                cp -v "$ndk_bin/llvm-strip" libexec/arm64-macos/strip_android_aarch64
                ;;
            Darwin_x86_64)
                cp -v "$ndk_bin/llvm-strip" libexec/x86_64-macos/strip_android
                cp -v "$ndk_bin/llvm-strip" libexec/x86_64-macos/strip_android_aarch64
                ;;
        esac
    elif [ -f "$ndk_bin/llvm-strip.exe" ]; then
        cp -v "$ndk_bin/llvm-strip.exe" libexec/x86_64-win32/strip_android.exe
        cp -v "$ndk_bin/llvm-strip.exe" libexec/x86_64-win32/strip_android_aarch64.exe
    fi
fi

# macOS toolchain (iOS bundling); populate both mac libexec pairs when building on macOS.
if [ "$(uname -s)" = "Darwin" ]; then
    _ca=$(xcrun -f codesign_allocate 2>/dev/null || true)
    _st=$(xcrun -f strip 2>/dev/null || true)
    if [ -n "$_ca" ]; then
        copy_if_exists "$_ca" libexec/x86_64-macos/codesign_allocate
        copy_if_exists "$_ca" libexec/arm64-macos/codesign_allocate
    fi
    if [ -n "$_st" ]; then
        copy_if_exists "$_st" libexec/x86_64-macos/strip
        copy_if_exists "$_st" libexec/arm64-macos/strip
    fi
fi

copy () {
    cp -v $DYNAMO_HOME/archive/${SHA1}/engine/$1 libexec/$2
}

copy x86_64-linux/stripped/dmengine x86_64-linux/dmengine
copy x86_64-linux/stripped/dmengine_release x86_64-linux/dmengine_release
# copy x86_64-linux/stripped/dmengine_headless x86_64-linux/dmengine_headless
copy arm64-linux/stripped/dmengine arm64-linux/dmengine
copy arm64-linux/stripped/dmengine_release arm64-linux/dmengine_release
# copy arm64-linux/stripped/dmengine_headless arm64-linux/dmengine_headless
copy x86_64-macos/stripped/dmengine x86_64-macos/dmengine
copy x86_64-macos/stripped/dmengine_release x86_64-macos/dmengine_release
# copy x86_64-macos/stripped/dmengine_headless x86_64-macos/dmengine_headless
copy arm64-macos/stripped/dmengine arm64-macos/dmengine
copy arm64-macos/stripped/dmengine_release arm64-macos/dmengine_release
# copy arm64-macos/stripped/dmengine_headless arm64-macos/dmengine_headless
# copy win32/dmengine.exe x86-win32/dmengine.exe
# copy win32/dmengine_release.exe x86-win32/dmengine_release.exe
# copy win32/dmengine_headless.exe x86-win32/dmengine_headless.exe
copy x86_64-win32/dmengine.exe x86_64-win32/dmengine.exe
copy x86_64-win32/dmengine_release.exe x86_64-win32/dmengine_release.exe
# copy x86_64-win32/dmengine_headless.exe x86_64-win32/dmengine_headless.exe
copy arm64-ios/stripped/dmengine arm64-ios/dmengine
copy arm64-ios/stripped/dmengine_release arm64-ios/dmengine_release
copy x86_64-ios/stripped/dmengine x86_64-ios/dmengine
copy x86_64-ios/stripped/dmengine_release x86_64-ios/dmengine_release
# copy armv7-android/stripped/libdmengine.so armv7-android/libdmengine.so
# copy armv7-android/stripped/libdmengine_release.so armv7-android/libdmengine_release.so
copy arm64-android/stripped/libdmengine.so arm64-android/libdmengine.so # TODO only valid once arm64-android CI target is present --jbnn
copy arm64-android/stripped/libdmengine_release.so arm64-android/libdmengine_release.so # TODO only valid once arm64-android CI target is present --jbnn
copy js-web/dmengine.js js-web/dmengine.js
copy js-web/dmengine.js.mem js-web/dmengine.js.mem
copy js-web/dmengine_release.js js-web/dmengine_release.js
copy js-web/dmengine_release.js.mem js-web/dmengine_release.js.mem
copy wasm-web/dmengine.js wasm-web/dmengine.js
copy wasm-web/dmengine.wasm wasm-web/dmengine.wasm
copy wasm-web/dmengine_release.js wasm-web/dmengine_release.js
copy wasm-web/dmengine_release.wasm wasm-web/dmengine_release.wasm

copy wasm_pthread-web/dmengine.js wasm_pthread-web/dmengine.js
copy wasm_pthread-web/dmengine.wasm wasm_pthread-web/dmengine.wasm
copy wasm_pthread-web/dmengine_release.js wasm_pthread-web/dmengine_release.js
copy wasm_pthread-web/dmengine_release.wasm wasm_pthread-web/dmengine_release.wasm

if [ -e "${SCRIPT_DIR}/copy_private.sh" ]; then
    sh "${SCRIPT_DIR}/copy_private.sh"
fi
