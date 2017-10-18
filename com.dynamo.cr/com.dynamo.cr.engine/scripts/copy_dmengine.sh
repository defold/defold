set -e
mkdir -p engine/linux
mkdir -p engine/x86_64-linux
mkdir -p engine/darwin
mkdir -p engine/x86_64-darwin
mkdir -p engine/win32
mkdir -p engine/x86_64-win32
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

copy linux/dmengine engine/linux/dmengine
copy linux/dmengine_release engine/linux/dmengine_release

copy x86_64-linux/dmengine engine/x86_64-linux/dmengine
copy x86_64-linux/dmengine_release engine/x86_64-linux/dmengine_release

copy darwin/dmengine engine/darwin/dmengine
copy darwin/dmengine_release engine/darwin/dmengine_release
copy x86_64-darwin/dmengine engine/x86_64-darwin/dmengine
copy x86_64-darwin/dmengine_release engine/x86_64-darwin/dmengine_release
copy win32/dmengine.exe engine/win32/dmengine.exe
copy win32/dmengine_release.exe engine/win32/dmengine_release.exe
copy x86_64-win32/dmengine.exe engine/x86_64-win32/dmengine.exe
copy x86_64-win32/dmengine_release.exe engine/x86_64-win32/dmengine_release.exe
copy arm64-darwin/dmengine engine/arm64-ios/dmengine
copy arm64-darwin/dmengine_release engine/arm64-ios/dmengine_release
copy armv7-darwin/dmengine engine/ios/dmengine
copy armv7-darwin/dmengine_release engine/ios/dmengine_release
copy armv7-android/libdmengine.so engine/android/libdmengine.so
copy armv7-android/libdmengine_release.so engine/android/libdmengine_release.so
copy js-web/dmengine.js engine/js-web/dmengine.js
copy js-web/dmengine_release.js engine/js-web/dmengine_release.js
copy js-web/defold_sound.swf engine/js-web/defold_sound.swf
