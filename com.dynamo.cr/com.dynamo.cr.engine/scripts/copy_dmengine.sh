set -e
mkdir -p engine/linux
mkdir -p engine/darwin
mkdir -p engine/win32
mkdir -p engine/ios

scp ci-master.defold.com:builds/linux/dmengine engine/linux
scp ci-master.defold.com:builds/darwin/dmengine engine/darwin
scp ci-master.defold.com:builds/win32/dmengine.exe engine/win32
scp ci-master.defold.com:builds/ios/dmengine engine/ios
