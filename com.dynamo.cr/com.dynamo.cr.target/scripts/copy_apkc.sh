set -e
mkdir -p lib/linux
mkdir -p lib/darwin
mkdir -p lib/win32

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:/archive/${SHA1}/go/darwin/apkc lib/darwin/apkc
scp builder@ci-master.defold.com:/archive/${SHA1}/go/linux/apkc lib/linux/apkc
scp builder@ci-master.defold.com:/archive/${SHA1}/go/win32/apkc.exe lib/win32/apkc
