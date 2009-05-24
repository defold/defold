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

REMOTE_PATH="$USER_OVERRATED@overrated.dyndns.org:/repo/packages"
PACKAGES_ALL="protobuf-2.0.3 waf-1.5.3 gtest-1.2.1 collada-blender-0.3.159.1 vectormathlibrary-r1649 nvidia-texture-tools-2.0.6 PIL-1.1.6 
SDL-1.2.13"
PACKAGES_HOST="protobuf-2.0.3 gtest-1.2.1 glut-3.7.6 cg-2.1 nvidia-texture-tools-2.0.6 PIL-1.1.6 SDL-1.2.13"
PACKAGES_EGGS="protobuf-2.0.3-py2.5.egg pyglet-1.1.3-py2.5.egg"

for p in $PACKAGES_ALL; do
    download "$p-common.tar.gz"
done

for p in $PACKAGES_HOST; do
    download "$p-$HOST.tar.gz"
done

for p in $PACKAGES_EGGS; do
    download "$p"
done

pushd $DYNAMO_EXT > /dev/null
for p in `ls cache/*.tar.gz`; do
    echo "Extracting $p..."
    tar xfz $p
done

mkdir -p $DYNAMO_EXT/lib/python
export PYTHONPATH=$DYNAMO_EXT/lib/python

for p in `ls cache/*.egg`; do
    echo "Installing $p..."
    easy_install -q -d $DYNAMO_EXT/lib/python -N $p
done

#rm *.tar.gz *.egg

popd > /dev/null

echo "Copying src/waf_dynamo.py -> DYNAMO_HOME/ext/lib/python" 
cp src/waf_dynamo.py $DYNAMO_HOME/ext/lib/python

echo "Copying share/valgrind-python.supp -> DYNAMO_HOME/share"
mkdir -p $DYNAMO_HOME/share 
cp share/valgrind-python.supp $DYNAMO_HOME/share


