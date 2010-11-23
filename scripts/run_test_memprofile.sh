set -e
platform=`uname -s`

if [ "Darwin" == $platform ]; then
    export DYLD_INSERT_LIBRARIES=./build/default/src/libdlib_memprofile.dylib
fi

if [ "Linux" == $platform ]; then
    export XLD_PRELOAD=./build/default/src/libdlib_memprofile.so
fi

./build/default/src/test/test_memprofile dummy
