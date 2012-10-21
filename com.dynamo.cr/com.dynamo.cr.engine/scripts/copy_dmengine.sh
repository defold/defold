set -e
mkdir -p engine/linux
mkdir -p engine/darwin
mkdir -p engine/win32
mkdir -p engine/ios

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:builds/linux/dmengine.${SHA1} engine/linux/dmengine
scp builder@ci-master.defold.com:builds/darwin/dmengine.${SHA1} engine/darwin/dmengine
scp builder@ci-master.defold.com:builds/win32/dmengine.exe.${SHA1} engine/win32/dmengine.exe
scp builder@ci-master.defold.com:builds/ios/dmengine.${SHA1} engine/ios/dmengine
