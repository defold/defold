set -e
mkdir -p engine/x86-linux
mkdir -p engine/x86-darwin
mkdir -p engine/x86-win32
mkdir -p engine/ios
mkdir -p engine/arm64-ios
mkdir -p engine/android
mkdir -p engine/js-web

SHA1=`git log --pretty=%H -n1`

copy () {
    # echo for indicating progress as scp progress is suppressed when not running in a tty (e.g. from maven or on buildbot)
    echo "Copying $1"
    cp -v $DYNAMO_HOME/archive/${SHA1}/engine/$1 $2
}

copy x86-linux/dmengine engine/x86-linux/dmengine
copy x86-linux/dmengine_release engine/x86-linux/dmengine_release
copy x86-darwin/dmengine engine/x86-darwin/dmengine
copy x86-darwin/dmengine_release engine/x86-darwin/dmengine_release
copy x86-win32/dmengine.exe engine/x86-win32/dmengine.exe
copy x86-win32/dmengine_release.exe engine/x86-win32/dmengine_release.exe
copy arm64-darwin/dmengine engine/arm64-ios/dmengine
copy arm64-darwin/dmengine_release engine/arm64-ios/dmengine_release
copy armv7-darwin/dmengine engine/ios/dmengine
copy armv7-darwin/dmengine_release engine/ios/dmengine_release
copy armv7-android/libdmengine.so engine/android/libdmengine.so
copy armv7-android/libdmengine_release.so engine/android/libdmengine_release.so
copy js-web/dmengine.js engine/js-web/dmengine.js
copy js-web/dmengine_release.js engine/js-web/dmengine_release.js
copy js-web/defold_sound.swf engine/js-web/defold_sound.swf
