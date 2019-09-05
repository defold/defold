# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p lib/x86_64-linux
mkdir -p lib/x86_64-darwin
mkdir -p lib/x86-win32
mkdir -p lib/x86_64-win32
mkdir -p libexec/x86_64-linux
mkdir -p libexec/x86_64-darwin
mkdir -p libexec/x86-win32
mkdir -p libexec/x86_64-win32
mkdir -p libexec/armv7-darwin
mkdir -p libexec/arm64-darwin
mkdir -p libexec/x86_64-ios
mkdir -p libexec/armv7-android
mkdir -p libexec/arm64-android
mkdir -p libexec/js-web
mkdir -p libexec/wasm-web

SHA1=`git log --pretty=%H -n1`

# APKC
cp -v $DYNAMO_HOME/ext/bin/x86_64-darwin/apkc libexec/x86_64-darwin/apkc
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/apkc libexec/x86_64-linux/apkc
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/apkc.exe libexec/x86_64-win32/apkc.exe

# SPIRV toolchain
cp -v $DYNAMO_HOME/ext/bin/x86_64-darwin/glslc libexec/x86_64-darwin/glslc
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/glslc libexec/x86_64-linux/glslc
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/glslc.exe libexec/x86_64-win32/glslc.exe

cp -v $DYNAMO_HOME/ext/bin/x86_64-darwin/spirv-cross libexec/x86_64-darwin/spirv-cross
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/spirv-cross libexec/x86_64-linux/spirv-cross
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/spirv-cross.exe libexec/x86_64-win32/spirv-cross.exe

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/builtins.zip lib/builtins.zip

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/armv7-android/classes.dex lib/classes.dex
cp -v $DYNAMO_HOME/ext/share/java/android.jar lib/android.jar

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/dlib.jar lib/dlib.jar

# TEXC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libtexc_shared.so lib/x86_64-linux/libtexc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-darwin/libtexc_shared.dylib lib/x86_64-darwin/libtexc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/texc_shared.dll lib/x86_64-win32/texc_shared.dll

# PVRTexLib
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/PVRTexLib.dll lib/x86_64-win32/PVRTexLib.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-linux/libPVRTexLib.so lib/x86_64-linux/libPVRTexLib.so
cp -v $DYNAMO_HOME/ext/lib/x86_64-darwin/libPVRTexLib.dylib lib/x86_64-darwin/libPVRTexLib.dylib

# Win32 32
cp -v $DYNAMO_HOME/ext/lib/win32/OpenAL32.dll lib/x86-win32/OpenAL32.dll
cp -v $DYNAMO_HOME/ext/lib/win32/wrap_oal.dll lib/x86-win32/wrap_oal.dll
# Win32 64
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/OpenAL32.dll lib/x86_64-win32/OpenAL32.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/wrap_oal.dll lib/x86_64-win32/wrap_oal.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/msvcr120.dll lib/x86_64-win32/msvcr120.dll


rm -rf tmp
mkdir -p tmp
tar xf ../../packages/luajit-2.1.0-beta3-x86_64-win32.tar.gz -C tmp
tar xf ../../packages/luajit-2.1.0-beta3-x86_64-linux.tar.gz -C tmp
tar xf ../../packages/luajit-2.1.0-beta3-x86_64-darwin.tar.gz -C tmp

cp -v tmp/bin/x86_64-linux/luajit-32 libexec/x86_64-linux/luajit-32
cp -v tmp/bin/x86_64-linux/luajit-64 libexec/x86_64-linux/luajit-64
cp -v tmp/bin/x86_64-darwin/luajit-32 libexec/x86_64-darwin/luajit-32
cp -v tmp/bin/x86_64-darwin/luajit-64 libexec/x86_64-darwin/luajit-64
cp -v tmp/bin/x86_64-win32/luajit-32.exe libexec/x86_64-win32/luajit-32.exe
cp -v tmp/bin/x86_64-win32/luajit-64.exe libexec/x86_64-win32/luajit-64.exe
jar cfM lib/luajit-share.zip -C $DYNAMO_HOME/ext/share/ luajit

copy () {
    cp -v $DYNAMO_HOME/archive/${SHA1}/engine/$1 libexec/$2
}

copy x86_64-linux/stripped/dmengine x86_64-linux/dmengine
copy x86_64-linux/stripped/dmengine_release x86_64-linux/dmengine_release
copy x86_64-linux/stripped/dmengine_headless x86_64-linux/dmengine_headless
copy x86_64-darwin/stripped/dmengine x86_64-darwin/dmengine
copy x86_64-darwin/stripped/dmengine_release x86_64-darwin/dmengine_release
copy x86_64-darwin/stripped/dmengine_headless x86_64-darwin/dmengine_headless
copy win32/dmengine.exe x86-win32/dmengine.exe
copy win32/dmengine_release.exe x86-win32/dmengine_release.exe
copy win32/dmengine_headless.exe x86-win32/dmengine_headless.exe
copy x86_64-win32/dmengine.exe x86_64-win32/dmengine.exe
copy x86_64-win32/dmengine_release.exe x86_64-win32/dmengine_release.exe
copy x86_64-win32/dmengine_headless.exe x86_64-win32/dmengine_headless.exe
copy armv7-darwin/stripped/dmengine armv7-darwin/dmengine
copy armv7-darwin/stripped/dmengine_release armv7-darwin/dmengine_release
copy arm64-darwin/stripped/dmengine arm64-darwin/dmengine
copy arm64-darwin/stripped/dmengine_release arm64-darwin/dmengine_release
copy x86_64-ios/stripped/dmengine x86_64-ios/dmengine
copy x86_64-ios/stripped/dmengine_release x86_64-ios/dmengine_release
copy armv7-android/stripped/libdmengine.so armv7-android/libdmengine.so
copy armv7-android/stripped/libdmengine_release.so armv7-android/libdmengine_release.so
copy arm64-android/stripped/libdmengine.so arm64-android/libdmengine.so # TODO only valid once arm64-android CI target is present --jbnn
copy arm64-android/stripped/libdmengine_release.so arm64-android/libdmengine_release.so # TODO only valid once arm64-android CI target is present --jbnn
copy js-web/dmengine.js js-web/dmengine.js
copy js-web/defold_sound.swf js-web/defold_sound.swf
#copy js-web/dmengine.js.mem js-web/dmengine.js.mem
copy js-web/dmengine_release.js js-web/dmengine_release.js
#copy js-web/dmengine_release.js.mem js-web/dmengine_release.js.mem
copy wasm-web/dmengine.js wasm-web/dmengine.js
copy wasm-web/dmengine.wasm wasm-web/dmengine.wasm
copy wasm-web/dmengine_release.js wasm-web/dmengine_release.js
copy wasm-web/dmengine_release.wasm wasm-web/dmengine_release.wasm
