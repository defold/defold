# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p libexec/linux
mkdir -p libexec/darwin
mkdir -p libexec/win32
mkdir -p share

SHA1=`git log --pretty=%H -n1`

mkdir -p tmp
tar xf ../../packages/luajit-2.0.3-win32.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.3-linux.tar.gz -C tmp
tar xf ../../packages/luajit-2.0.3-darwin.tar.gz -C tmp

cp -v tmp/bin/linux/luajit libexec/linux/luajit
cp -v tmp/bin/darwin/luajit libexec/darwin/luajit
cp -v tmp/bin/win32/luajit.exe libexec/win32/luajit.exe
cp -v -r $DYNAMO_HOME/ext/share/luajit share
