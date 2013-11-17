#!/bin/bash
set -e
platform=`uname -s`

if [ "Darwin" == $platform ]; then
    # NOTE: We used to link with -flat_namespace but go spurious link errors
    export DYLD_FORCE_FLAT_NAMESPACE=yes
    export DYLD_INSERT_LIBRARIES=./build/default/src/libdlib_memprofile.dylib
fi

if [ "Linux" == $platform ]; then
    export LD_PRELOAD=./build/default/src/libdlib_memprofile.so
fi

export DMMEMPROFILE_TRACE=1
./build/default/src/test/test_memprofile dummy

unset DYLD_INSERT_LIBRARIES
unset LD_PRELOAD

PYTHONPATH=src/dlib python src/test/test_memprofile.py

rm -f memprofile.trace
