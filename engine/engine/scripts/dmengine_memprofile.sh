set -e
platform=`uname -s`

if [ "Darwin" == $platform ]; then
    # NOTE: We used to link with -flat_namespace but go spurious link errors
    export DYLD_FORCE_FLAT_NAMESPACE=yes
    export DYLD_INSERT_LIBRARIES=${DYNAMO_HOME}/lib/arm64-macos/libdlib_memprofile.dylib
fi

if [ "Linux" == $platform ]; then
    export LD_PRELOAD=${DYNAMO_HOME}/lib/x86_64-linux/libdlib_memprofile.so
fi

PROGRAM=$(which dmengine)
if [ "$1" != "" ]; then
    PROGRAM=$1
fi

# memprofile.trace is written in the current working dir
# Also see engine/dlib/README.md for instruction on how to generate a report
DMMEMPROFILE_TRACE=1 ${PROGRAM} $@
