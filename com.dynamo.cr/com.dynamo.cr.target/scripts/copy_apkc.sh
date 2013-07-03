set -e
mkdir -p lib/linux
mkdir -p lib/darwin
mkdir -p lib/win32

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:/archive/go/darwin/apkc.${SHA1} lib/darwin/apkc
scp builder@ci-master.defold.com:/archive/go/linux/apkc.${SHA1} lib/linux/apkc
scp builder@ci-master.defold.com:/archive/go/win32/apkc.${SHA1} lib/win32/apkc
