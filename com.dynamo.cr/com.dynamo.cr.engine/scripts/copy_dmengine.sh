set -e
mkdir -p engine/linux
mkdir -p engine/darwin
mkdir -p engine/win32
mkdir -p engine/ios
mkdir -p engine/android

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:/archive/${SHA1}/engine/linux/dmengine engine/linux/dmengine
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/linux/dmengine_release engine/linux/dmengine_release
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/darwin/dmengine engine/darwin/dmengine
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/darwin/dmengine_release engine/darwin/dmengine_release
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/win32/dmengine.exe engine/win32/dmengine.exe
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/win32/dmengine_release.exe engine/win32/dmengine_release.exe
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/armv7-darwin/dmengine engine/ios/dmengine
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/armv7-darwin/dmengine_release engine/ios/dmengine_release
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/armv7-android/libdmengine.so engine/android/libdmengine.so
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/armv7-android/libdmengine_release.so engine/android/libdmengine_release.so
