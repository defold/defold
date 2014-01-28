# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p lib/linux
mkdir -p lib/x86_64-darwin
mkdir -p lib/win32
mkdir -p lib/ios

SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:/archive/${SHA1}/engine/linux/libparticle_shared.so lib/linux/libparticle_shared.so
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/x86_64-darwin/libparticle_shared.dylib lib/x86_64-darwin/libparticle_shared.dylib
scp builder@ci-master.defold.com:/archive/${SHA1}/engine/win32/particle_shared.dll lib/win32/particle_shared.dll
