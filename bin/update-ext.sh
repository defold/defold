#! /bin/bash

download() {
    [ -f $DYNAMO_EXT/cache/$1 ] && return
    scp "$REMOTE_PATH/$1" /tmp
    [ ! 0 -eq $? ] && echo "Failed to download $1" && exit 1
    mv /tmp/$1 "$DYNAMO_EXT/cache"
}

# Set $USER_OVERRATED to $USER or $USERNAME if *not* set
USER_OVERRATED=${USER_OVERRATED:-${USER}}
USER_OVERRATED=${USER_OVERRATED:-${USERNAME}}

#[ -z $USER_OVERRATED ] && echo 'Environment variable $USER_OVERRATED not set.' && exit 1

tmp=`uname -s`
[ "Linux" == ${tmp:0:5} ] && HOST="linux"
[ "Darwin" == $tmp ] && HOST="darwin"
[ "MINGW" == ${tmp:0:5} ] && HOST="win32"

[ -z $HOST ] && echo "Unsupported host: $tmp" && exit 1

# Validate DYNAMO_HOME
[ -z "$DYNAMO_HOME" ] && echo "ERROR: DYNAMO_HOME not set" && exit 1
[ ! -d "$DYNAMO_HOME" ] && echo "ERROR: DYNAMO_HOME is not a directory" && exit 1
DYNAMO_EXT=$DYNAMO_HOME/ext

mkdir -p $DYNAMO_EXT/cache

REMOTE_PATH="$USER_OVERRATED@dev.defold.se:/repo/packages"
PACKAGES_ALL="protobuf-2.3.0 waf-1.5.9 gtest-1.2.1 vectormathlibrary-r1649 nvidia-texture-tools-2.0.6 PIL-1.1.6 SDL-1.2.13 junit-4.6 protobuf-java-2.3.0 openal-1.1 maven-3.0.1 vecmath jython-2.5.2 vpx-v0.9.7-p1"
PACKAGES_HOST="protobuf-2.3.0 gtest-1.2.1 glut-3.7.6 cg-2.1 nvidia-texture-tools-2.0.6 PIL-1.1.6 SDL-1.2.13 openal-1.1 PVRTexToolCL-2.08.28.0634 vpx-v0.9.7-p1"
PACKAGES_EGGS="protobuf-2.3.0-py2.5.egg pyglet-1.1.3-py2.5.egg gdata-2.0.6-py2.6.egg"
PACKAGES_IOS="protobuf-2.3.0 gtest-1.2.1"

for p in $PACKAGES_ALL; do
    download "$p-common.tar.gz"
done

for p in $PACKAGES_HOST; do
    download "$p-$HOST.tar.gz"
done

for p in $PACKAGES_IOS; do
    download "$p-armv6-darwin.tar.gz"
done

for p in $PACKAGES_EGGS; do
    download "$p"
done

pushd $DYNAMO_EXT > /dev/null

for p in $PACKAGES_ALL; do
    echo "Extracting $p..."
    tar xfz "cache/$p-common.tar.gz"
done

for p in $PACKAGES_HOST; do
    echo "Extracting $p..."
    tar xfz "cache/$p-$HOST.tar.gz"
done

for p in $PACKAGES_IOS; do
    echo "Extracting $p..."
    tar xfz "cache/$p-armv6-darwin.tar.gz"
done

#for p in `ls cache/*.tar.gz`; do
#    echo "Extracting $p..."
#    tar xfz $p
#done

mkdir -p $DYNAMO_EXT/lib/python
export PYTHONPATH=$DYNAMO_EXT/lib/python

for p in `ls cache/*.egg`; do
    echo "Installing $p..."
    easy_install -q -d $DYNAMO_EXT/lib/python -N $p
done

#rm *.tar.gz *.egg

popd > /dev/null

mkdir -p $DYNAMO_HOME/lib/python
export PYTHONPATH=$PYTHONPATH:$DYNAMO_HOME/lib/python
echo "Copying src/waf_dynamo.py -> DYNAMO_HOME/lib/python"
cp src/waf_dynamo.py $DYNAMO_HOME/lib/python

echo "Copying src/waf_content.py -> DYNAMO_HOME/lib/python"
cp src/waf_content.py $DYNAMO_HOME/lib/python

echo "Copying share/valgrind-python.supp -> DYNAMO_HOME/share"
mkdir -p $DYNAMO_HOME/share
cp share/valgrind-python.supp $DYNAMO_HOME/share
echo "Copying share/valgrind-libasound.supp -> DYNAMO_HOME/share"
cp share/valgrind-libasound.supp $DYNAMO_HOME/share
echo "Copying share/valgrind-libdlib.supp -> DYNAMO_HOME/share"
cp share/valgrind-libdlib.supp $DYNAMO_HOME/share
echo "Copying share/engine_profile.mobileprovision -> DYNAMO_HOME/share"
cp share/engine_profile.mobileprovision $DYNAMO_HOME/share

mkdir -p $DYNAMO_HOME/bin
cp -v bin/git* $DYNAMO_HOME/bin

