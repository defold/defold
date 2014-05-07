set -e
mkdir -p engine/linux
mkdir -p engine/darwin
mkdir -p engine/win32
mkdir -p engine/ios
mkdir -p engine/android

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

function copy() {
	# echo for indicating progress as scp progress is suppressed when not running in a tty (e.g. from maven or on buildbot)
	echo "Copying $1"
	scp builder@ci-master.defold.com:/archive/${SHA1}/engine/$1 $2
}

copy linux/dmengine engine/linux/dmengine
copy linux/dmengine_release engine/linux/dmengine_release
copy darwin/dmengine engine/darwin/dmengine
copy darwin/dmengine_release engine/darwin/dmengine_release
copy win32/dmengine.exe engine/win32/dmengine.exe
copy win32/dmengine_release.exe engine/win32/dmengine_release.exe
copy armv7-darwin/dmengine engine/ios/dmengine
copy armv7-darwin/dmengine_release engine/ios/dmengine_release
copy armv7-android/libdmengine.so engine/android/libdmengine.so
copy armv7-android/libdmengine_release.so engine/android/libdmengine_release.so
