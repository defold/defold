set -e
mkdir -p engine/linux
mkdir -p engine/darwin
mkdir -p engine/win32
mkdir -p engine/ios
mkdir -p engine/android

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:builds/linux/dmengine.${SHA1} engine/linux/dmengine
scp builder@ci-master.defold.com:builds/linux/dmengine_release.${SHA1} engine/linux/dmengine_release
scp builder@ci-master.defold.com:builds/darwin/dmengine.${SHA1} engine/darwin/dmengine
scp builder@ci-master.defold.com:builds/darwin/dmengine_release.${SHA1} engine/darwin/dmengine_release
scp builder@ci-master.defold.com:builds/win32/dmengine.exe.${SHA1} engine/win32/dmengine.exe
scp builder@ci-master.defold.com:builds/win32/dmengine_release.exe.${SHA1} engine/win32/dmengine_release.exe
scp builder@ci-master.defold.com:builds/armv7-darwin/dmengine.${SHA1} engine/ios/dmengine
scp builder@ci-master.defold.com:builds/armv7-darwin/dmengine_release.${SHA1} engine/ios/dmengine_release
scp builder@ci-master.defold.com:builds/armv7-android/libdmengine.so.${SHA1} engine/android/libdmengine.so
scp builder@ci-master.defold.com:builds/armv7-android/libdmengine_release.so.${SHA1} engine/android/libdmengine_release.so
