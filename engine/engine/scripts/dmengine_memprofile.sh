set -e
platform=`uname -s`

if [ "Darwin" == $platform ]; then
    # NOTE: We used to link with -flat_namespace but go spurious link errors
    export DYLD_FORCE_FLAT_NAMESPACE=yes
    export DYLD_INSERT_LIBRARIES=$DYNAMO_HOME/lib/libdlib_memprofile.dylib
fi

if [ "Linux" == $platform ]; then
    export LD_PRELOAD=$DYNAMO_HOME/lib/libdlib_memprofile.so
fi

dmengine $@
