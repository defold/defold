set -e
platform=`uname -s`

if [ "Darwin" == $platform ]; then
    export DYLD_INSERT_LIBRARIES=$DYNAMO_HOME/lib/libdlib_memprofile.dylib
fi

if [ "Linux" == $platform ]; then
    export LD_PRELOAD=$DYNAMO_HOME/lib/libdlib_memprofile.so
fi

dmengine $@
