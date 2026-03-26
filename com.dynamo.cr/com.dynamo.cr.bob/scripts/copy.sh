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

# Used from build.py when packaging Bob. Archive-backed copies run only if
# $DYNAMO_HOME/archive/$SHA1 exists (CI / engine synced); ext copies always run.
# The corresponding file for development is build.xml

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

set -e
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
# mkdir -p libexec/js-web
mkdir -p libexec/wasm-web
mkdir -p libexec/wasm_pthread-web

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

# aapt2
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/aapt2 libexec/x86_64-macos/aapt2
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/aapt2 libexec/arm64-macos/aapt2
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/aapt2 libexec/x86_64-linux/aapt2
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/aapt2.exe libexec/x86_64-win32/aapt2.exe

# codesign_allocate
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/codesign_allocate libexec/x86_64-macos/codesign_allocate
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/codesign_allocate libexec/arm64-macos/codesign_allocate

# apkc
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/apkc libexec/x86_64-linux/apkc

# libogg
cp -v $DYNAMO_HOME/ext/lib/x86_64-macos/libogg.dylib libexec/x86_64-macos/libogg.dylib
cp -v $DYNAMO_HOME/ext/lib/arm64-macos/libogg.dylib libexec/arm64-macos/libogg.dylib
cp -v $DYNAMO_HOME/ext/lib/x86_64-linux/libogg.so libexec/x86_64-linux/libogg.so
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/libogg.dll libexec/x86_64-win32/libogg.dll

# liboggz
cp -v $DYNAMO_HOME/ext/lib/x86_64-macos/liboggz.dylib libexec/x86_64-macos/liboggz.dylib
cp -v $DYNAMO_HOME/ext/lib/arm64-macos/liboggz.dylib libexec/arm64-macos/liboggz.dylib
cp -v $DYNAMO_HOME/ext/lib/x86_64-linux/liboggz.so libexec/x86_64-linux/liboggz.so
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/liboggz.dll libexec/x86_64-win32/liboggz.dll

# oggz-validate
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/oggz-validate libexec/x86_64-macos/oggz-validate
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/oggz-validate libexec/arm64-macos/oggz-validate
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/oggz-validate libexec/x86_64-linux/oggz-validate
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/oggz-validate.exe libexec/x86_64-win32/oggz-validate.exe

# strip
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/strip libexec/x86_64-macos/strip
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/strip libexec/arm64-macos/strip

# strip_android
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/strip_android libexec/x86_64-macos/strip_android
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/strip_android libexec/arm64-macos/strip_android
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/strip_android libexec/x86_64-linux/strip_android
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/strip_android.exe libexec/x86_64-win32/strip_android.exe

# strip_android_aarch64
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/strip_android_aarch64 libexec/x86_64-linux/strip_android_aarch64
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/strip_android_aarch64.exe libexec/x86_64-win32/strip_android_aarch64.exe

# zipalign
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/zipalign libexec/x86_64-macos/zipalign
cp -v $DYNAMO_HOME/ext/bin/arm64-macos/zipalign libexec/arm64-macos/zipalign
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/zipalign libexec/x86_64-linux/zipalign
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/zipalign.exe libexec/x86_64-win32/zipalign.exe

#
if [ -d "$DYNAMO_HOME/archive/$SHA1" ]; then
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/builtins.zip lib/builtins.zip

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-android/classes.dex lib/classes.dex

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

fi

cp -v $DYNAMO_HOME/ext/share/java/android.jar lib/android.jar

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

copy () {
    cp -v $DYNAMO_HOME/archive/${SHA1}/engine/$1 libexec/$2
}

if [ -d "$DYNAMO_HOME/archive/$SHA1" ]; then
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
# copy js-web/dmengine.js js-web/dmengine.js
# copy js-web/dmengine.js.mem js-web/dmengine.js.mem
# copy js-web/dmengine_release.js js-web/dmengine_release.js
# copy js-web/dmengine_release.js.mem js-web/dmengine_release.js.mem
copy wasm-web/dmengine.js wasm-web/dmengine.js
copy wasm-web/dmengine.wasm wasm-web/dmengine.wasm
copy wasm-web/dmengine_release.js wasm-web/dmengine_release.js
copy wasm-web/dmengine_release.wasm wasm-web/dmengine_release.wasm

copy wasm_pthread-web/dmengine.js wasm_pthread-web/dmengine.js
copy wasm_pthread-web/dmengine.wasm wasm_pthread-web/dmengine.wasm
copy wasm_pthread-web/dmengine_release.js wasm_pthread-web/dmengine_release.js
copy wasm_pthread-web/dmengine_release.wasm wasm_pthread-web/dmengine_release.wasm

fi

if [ -e "${DIR}/copy_private.sh" ]; then
    sh ${DIR}/copy_private.sh
fi
