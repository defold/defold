set -e
mkdir -p lib/linux
mkdir -p lib/darwin
mkdir -p lib/win32

SHA1=`git log --pretty=%H -n1`

cp -v $DYNAMO_HOME/archive/${SHA1}/go/darwin/apkc lib/darwin/apkc
cp -v $DYNAMO_HOME/archive/${SHA1}/go/linux/apkc lib/linux/apkc
cp -v $DYNAMO_HOME/archive/${SHA1}/go/win32/apkc.exe lib/win32/apkc
