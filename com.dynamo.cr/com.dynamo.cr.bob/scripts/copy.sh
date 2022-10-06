# Copyright 2020-2022 The Defold Foundation
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

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

set -e
mkdir -p lib/x86_64-linux
mkdir -p lib/x86_64-macos
#mkdir -p lib/arm64-macos
mkdir -p lib/x86-win32
mkdir -p lib/x86_64-win32
mkdir -p libexec/x86_64-linux
mkdir -p libexec/x86_64-macos
#mkdir -p libexec/arm64-macos
mkdir -p libexec/x86-win32
mkdir -p libexec/x86_64-win32
mkdir -p libexec/arm64-ios
mkdir -p libexec/x86_64-ios
mkdir -p libexec/armv7-android
mkdir -p libexec/arm64-android
mkdir -p libexec/js-web
mkdir -p libexec/wasm-web
mkdir -p libexec/arm64-nx64

SHA1=`git log --pretty=%H -n1`

# SPIRV toolchain
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/glslc libexec/x86_64-macos/glslc
#cp -v $DYNAMO_HOME/ext/bin/arm64-macos/glslc libexec/arm64-macos/glslc
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/glslc libexec/x86_64-linux/glslc
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/glslc.exe libexec/x86_64-win32/glslc.exe

cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/spirv-cross libexec/x86_64-macos/spirv-cross
#cp -v $DYNAMO_HOME/ext/bin/arm64-macos/spirv-cross libexec/arm64-macos/spirv-cross
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/spirv-cross libexec/x86_64-linux/spirv-cross
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/spirv-cross.exe libexec/x86_64-win32/spirv-cross.exe

# luac
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/luac-32 libexec/x86_64-linux/luac-32
cp -v $DYNAMO_HOME/ext/bin/x86_64-macos/luac-32 libexec/x86_64-macos/luac-32
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/luac-32.exe libexec/x86_64-win32/luac-32.exe

#
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/builtins.zip lib/builtins.zip

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/armv7-android/classes.dex lib/classes.dex
cp -v $DYNAMO_HOME/ext/share/java/android.jar lib/android.jar

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/dlib.jar lib/dlib.jar
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/modelimporter.jar lib/modelimporter.jar

# TEXC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libtexc_shared.so lib/x86_64-linux/libtexc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-macos/libtexc_shared.dylib lib/x86_64-macos/libtexc_shared.dylib
#cp -v $DYNAMO_HOME/archive/${SHA1}/engine/arm64-macos/libtexc_shared.dylib lib/arm64-macos/libtexc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/texc_shared.dll lib/x86_64-win32/texc_shared.dll

# MODELC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libmodelc_shared.so lib/x86_64-linux/libmodelc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-macos/libmodelc_shared.dylib lib/x86_64-macos/libmodelc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/modelc_shared.dll lib/x86_64-win32/modelc_shared.dll

# Win32 32
cp -v $DYNAMO_HOME/ext/lib/win32/OpenAL32.dll lib/x86-win32/OpenAL32.dll
cp -v $DYNAMO_HOME/ext/lib/win32/wrap_oal.dll lib/x86-win32/wrap_oal.dll
# Win32 64
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/OpenAL32.dll lib/x86_64-win32/OpenAL32.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/wrap_oal.dll lib/x86_64-win32/wrap_oal.dll


rm -rf tmp
mkdir -p tmp
tar xf ../../packages/luajit-2.1.0-633f265-win32.tar.gz -C tmp
tar xf ../../packages/luajit-2.1.0-633f265-x86_64-win32.tar.gz -C tmp
tar xf ../../packages/luajit-2.1.0-633f265-x86_64-linux.tar.gz -C tmp
tar xf ../../packages/luajit-2.1.0-633f265-x86_64-macos.tar.gz -C tmp

cp -v tmp/bin/x86_64-linux/luajit-32 libexec/x86_64-linux/luajit-32
cp -v tmp/bin/x86_64-linux/luajit-64 libexec/x86_64-linux/luajit-64
cp -v tmp/bin/x86_64-macos/luajit-32 libexec/x86_64-macos/luajit-32
cp -v tmp/bin/x86_64-macos/luajit-64 libexec/x86_64-macos/luajit-64
#cp -v tmp/bin/arm64-macos/luajit-32 libexec/arm64-macos/luajit-32
#cp -v tmp/bin/arm64-macos/luajit-64 libexec/arm64-macos/luajit-64
cp -v tmp/bin/win32/luajit-32.exe libexec/x86_64-win32/luajit-32.exe
cp -v tmp/bin/x86_64-win32/luajit-64.exe libexec/x86_64-win32/luajit-64.exe
jar cfM lib/luajit-share.zip -C $DYNAMO_HOME/ext/share/ luajit

copy () {
    cp -v $DYNAMO_HOME/archive/${SHA1}/engine/$1 libexec/$2
}

copy x86_64-linux/stripped/dmengine x86_64-linux/dmengine
copy x86_64-linux/stripped/dmengine_release x86_64-linux/dmengine_release
copy x86_64-linux/stripped/dmengine_headless x86_64-linux/dmengine_headless
copy x86_64-macos/stripped/dmengine x86_64-macos/dmengine
copy x86_64-macos/stripped/dmengine_release x86_64-macos/dmengine_release
copy x86_64-macos/stripped/dmengine_headless x86_64-macos/dmengine_headless
#copy arm64-macos/stripped/dmengine arm64-macos/dmengine
#copy arm64-macos/stripped/dmengine_release arm64-macos/dmengine_release
#copy arm64-macos/stripped/dmengine_headless arm64-macos/dmengine_headless
copy win32/dmengine.exe x86-win32/dmengine.exe
copy win32/dmengine_release.exe x86-win32/dmengine_release.exe
copy win32/dmengine_headless.exe x86-win32/dmengine_headless.exe
copy x86_64-win32/dmengine.exe x86_64-win32/dmengine.exe
copy x86_64-win32/dmengine_release.exe x86_64-win32/dmengine_release.exe
copy x86_64-win32/dmengine_headless.exe x86_64-win32/dmengine_headless.exe
copy arm64-ios/stripped/dmengine arm64-ios/dmengine
copy arm64-ios/stripped/dmengine_release arm64-ios/dmengine_release
copy x86_64-ios/stripped/dmengine x86_64-ios/dmengine
copy x86_64-ios/stripped/dmengine_release x86_64-ios/dmengine_release
copy armv7-android/stripped/libdmengine.so armv7-android/libdmengine.so
copy armv7-android/stripped/libdmengine_release.so armv7-android/libdmengine_release.so
copy arm64-android/stripped/libdmengine.so arm64-android/libdmengine.so # TODO only valid once arm64-android CI target is present --jbnn
copy arm64-android/stripped/libdmengine_release.so arm64-android/libdmengine_release.so # TODO only valid once arm64-android CI target is present --jbnn
copy js-web/dmengine.js js-web/dmengine.js
#copy js-web/dmengine.js.mem js-web/dmengine.js.mem
copy js-web/dmengine_release.js js-web/dmengine_release.js
#copy js-web/dmengine_release.js.mem js-web/dmengine_release.js.mem
copy wasm-web/dmengine.js wasm-web/dmengine.js
copy wasm-web/dmengine.wasm wasm-web/dmengine.wasm
copy wasm-web/dmengine_release.js wasm-web/dmengine_release.js
copy wasm-web/dmengine_release.wasm wasm-web/dmengine_release.wasm

if [ -e "${DIR}/copy_private.sh" ]; then
    sh ${DIR}/copy_private.sh
fi
