# Note: $USER must be set on windows!

download() {
    [ -f $DYNAMO_EXT/$1 ] && gzip -t $DYNAMO_EXT/$1 && return
    scp "$REMOTE_PATH/$1" $DYNAMO_EXT
}

tmp=`uname -s`
[ "Linux" == ${tmp:0:5} ] && HOST="linux"
[ "Darwin" == $tmp ] && HOST="darwin"
[ "MINGW" == ${tmp:0:5} ] && HOST="win32"

[ -z $HOST ] && echo "Unsupported host: $tmp" && exit 1

# Validate DYNAMO_EXT
[ -z "$DYNAMO_EXT" ] && echo "ERROR: DYNAMO_EXT not set" && exit 1
[ ! -d "$DYNAMO_EXT" ] && echo "ERROR: DYNAMO_EXT is not a directory" && exit 1

REMOTE_PATH="$USER@overrated.dyndns.org:packages"
PACKAGES_ALL="protobuf-2.0.3 waf-1.5.3"
PACKAGES_HOST="protobuf-2.0.3"

for p in $PACKAGES_ALL; do
    download "$p-common.tar.gz"
done

for p in $PACKAGES_HOST; do
    download "$p-$HOST.tar.gz"
done

pushd $DYNAMO_EXT > /dev/null
for p in `ls *.tar.gz`; do
    echo "Extracting $p..."
    tar xfz $p
done

rm *.tar.gz

popd > /dev/null

