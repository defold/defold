# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p libexec/linux
mkdir -p libexec/darwin
mkdir -p libexec/win32
mkdir -p share

SHA1=`git log --pretty=%H -n1`

cp -v $DYNAMO_HOME/archive/${SHA1}/ext/bin/linux/luajit libexec/linux/luajit
cp -v $DYNAMO_HOME/archive/${SHA1}/ext/bin/darwin/luajit libexec/darwin/luajit
cp -v $DYNAMO_HOME/archive/${SHA1}/ext/bin/win32/luajit.exe libexec/win32/luajit.exe
cp -v -r $DYNAMO_HOME/ext/share/luajit share
