set -e
mkdir -p lib/linux
mkdir -p lib/x86_64-darwin
mkdir -p lib/win32
mkdir -p lib/ios

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:builds/linux/libparticle_shared.so.${SHA1} lib/linux/libparticle_shared.so
scp builder@ci-master.defold.com:builds/darwin/libparticle_shared.dylib.${SHA1} lib/x86_64-darwin/libparticle_shared.dylib
scp builder@ci-master.defold.com:builds/win32/libparticle_shared.dll.${SHA1} lib/win32/libparticle_shared.dll
