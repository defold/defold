# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p lib/x86-linux
mkdir -p lib/x86_64-linux
mkdir -p lib/x86-darwin
mkdir -p lib/x86_64-darwin
mkdir -p lib/x86-win32
mkdir -p lib/x86_64-win32
mkdir -p libexec/x86-linux
mkdir -p libexec/x86_64-linux
mkdir -p libexec/x86-darwin
mkdir -p libexec/x86_64-darwin
mkdir -p libexec/x86-win32
mkdir -p libexec/x86_64-win32
mkdir -p libexec/armv7-darwin
mkdir -p libexec/arm64-darwin
mkdir -p libexec/armv7-android
mkdir -p libexec/js-web

SHA1=`git log --pretty=%H -n1`

# APKC
cp -v $DYNAMO_HOME/ext/bin/x86_64-darwin/apkc libexec/x86-darwin/apkc
cp -v $DYNAMO_HOME/ext/bin/x86_64-darwin/apkc libexec/x86_64-darwin/apkc
cp -v $DYNAMO_HOME/ext/bin/linux/apkc libexec/x86-linux/apkc
cp -v $DYNAMO_HOME/ext/bin/x86_64-linux/apkc libexec/x86_64-linux/apkc
cp -v $DYNAMO_HOME/ext/bin/win32/apkc.exe libexec/x86-win32/apkc.exe
cp -v $DYNAMO_HOME/ext/bin/x86_64-win32/apkc.exe libexec/x86_64-win32/apkc.exe


cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/builtins.zip lib/builtins.zip

jar cfM lib/android-res.zip -C $DYNAMO_HOME/ext/share/java/ res
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/armv7-android/classes.dex lib/classes.dex
cp -v $DYNAMO_HOME/ext/share/java/android.jar lib/android.jar

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/java/dlib.jar lib/dlib.jar

# TEXC Shared
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/linux/libtexc_shared.so lib/x86-linux/libtexc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-linux/libtexc_shared.so lib/x86_64-linux/libtexc_shared.so
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-darwin/libtexc_shared.dylib lib/x86_64-darwin/libtexc_shared.dylib
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/win32/texc_shared.dll lib/x86-win32/texc_shared.dll
cp -v $DYNAMO_HOME/archive/${SHA1}/engine/x86_64-win32/texc_shared.dll lib/x86_64-win32/texc_shared.dll

# PVRTexLib
cp -v $DYNAMO_HOME/ext/lib/win32/PVRTexLib.dll lib/x86-win32/PVRTexLib.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/PVRTexLib.dll lib/x86_64-win32/PVRTexLib.dll
cp -v $DYNAMO_HOME/ext/lib/linux/libPVRTexLib.so lib/x86-linux/libPVRTexLib.so
cp -v $DYNAMO_HOME/ext/lib/x86_64-linux/libPVRTexLib.so lib/x86_64-linux/libPVRTexLib.so
cp -v $DYNAMO_HOME/ext/lib/darwin/libPVRTexLib.dylib lib/x86-linux/libPVRTexLib.dylib
cp -v $DYNAMO_HOME/ext/lib/x86_64-darwin/libPVRTexLib.dylib lib/x86_64-linux/libPVRTexLib.dylib

# Win32 32
cp -v $DYNAMO_HOME/ext/lib/win32/OpenAL32.dll lib/x86-win32/OpenAL32.dll
cp -v $DYNAMO_HOME/ext/lib/win32/wrap_oal.dll lib/x86-win32/wrap_oal.dll
cp -v $DYNAMO_HOME/ext/lib/win32/msvcr120.dll lib/x86-win32/msvcr120.dll
# Win32 64
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/OpenAL32.dll lib/x86_64-win32/OpenAL32.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/wrap_oal.dll lib/x86_64-win32/wrap_oal.dll
cp -v $DYNAMO_HOME/ext/lib/x86_64-win32/msvcr120.dll lib/x86_64-win32/msvcr120.dll


rm -rf tmp
mkdir -p tmp
tar xf ../../packages/luajit-2.0.5-win32.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.5-x86_64-win32.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.5-linux.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.5-x86_64-linux.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.5-darwin.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.5-x86_64-darwin.tar.gz -C tmp

cp -v tmp/bin/linux/luajit libexec/x86-linux/luajit
cp -v tmp/bin/x86_64-linux/luajit libexec/x86_64-linux/luajit
cp -v tmp/bin/darwin/luajit libexec/x86-darwin/luajit
cp -v tmp/bin/x86_64-darwin/luajit libexec/x86_64-darwin/luajit
cp -v tmp/bin/win32/luajit.exe libexec/x86-win32/luajit.exe
cp -v tmp/bin/x86_64-win32/luajit.exe libexec/x86_64-win32/luajit.exe
jar cfM lib/luajit-share.zip -C $DYNAMO_HOME/ext/share/ luajit

copy () {
    cp -v $DYNAMO_HOME/archive/${SHA1}/engine/$1 libexec/$2
}

copy linux/dmengine x86-linux/dmengine
copy linux/dmengine_release x86-linux/dmengine_release
copy x86_64-linux/dmengine x86_64-linux/dmengine
copy x86_64-linux/dmengine_release x86_64-linux/dmengine_release
copy darwin/dmengine x86-darwin/dmengine
copy darwin/dmengine_release x86-darwin/dmengine_release
copy x86_64-darwin/dmengine x86_64-darwin/dmengine
copy x86_64-darwin/dmengine_release x86_64-darwin/dmengine_release
copy win32/dmengine.exe x86-win32/dmengine.exe
copy win32/dmengine_release.exe x86-win32/dmengine_release.exe
copy x86_64-win32/dmengine.exe x86_64-win32/dmengine.exe
copy x86_64-win32/dmengine_release.exe x86_64-win32/dmengine_release.exe
copy armv7-darwin/dmengine armv7-darwin/dmengine
copy armv7-darwin/dmengine_release armv7-darwin/dmengine_release
copy arm64-darwin/dmengine arm64-darwin/dmengine
copy arm64-darwin/dmengine_release arm64-darwin/dmengine_release
copy armv7-android/libdmengine.so armv7-android/libdmengine.so
copy armv7-android/libdmengine_release.so armv7-android/libdmengine_release.so
copy js-web/dmengine.js js-web/dmengine.js
copy js-web/defold_sound.swf js-web/defold_sound.swf
#copy js-web/dmengine.js.mem js-web/dmengine.js.mem
copy js-web/dmengine_release.js js-web/dmengine_release.js
#copy js-web/dmengine_release.js.mem js-web/dmengine_release.js.mem
