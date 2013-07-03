# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p lib/linux
mkdir -p lib/x86_64-darwin
mkdir -p lib/win32

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:builds/linux/libtexc_shared.so.${SHA1} lib/linux/libtexc_shared.so
scp builder@ci-master.defold.com:builds/x86_64-darwin/libtexc_shared.dylib.${SHA1} lib/x86_64-darwin/libtexc_shared.dylib
scp builder@ci-master.defold.com:builds/win32/texc_shared.dll.${SHA1} lib/win32/texc_shared.dll
