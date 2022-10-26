#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $SCRIPT_DIR/..
BUILD_DIR=$(realpath ./build/src)

set -e

CLASS_NAME=com.dynamo.bob.pipeline.ModelImporter
LIBNAME=modelc_shared
SUFFIX=.so
if [ "Darwin" == "$(uname)" ]; then
    SUFFIX=.dylib
fi

MODELIMPORTER_SHARED_LIB=./build/src/lib${LIBNAME}${SUFFIX}
if [ -z "${MODELIMPORTER_SHARED_LIB}" ]; then
    echo "Couldn't find the shared library!"
fi
echo "Found ${MODELIMPORTER_SHARED_LIB}"

JAR=$(find . -iname "*.jar")
if [ -z "${JAR}" ]; then
    echo "Couldn't find the jar file!"
fi
echo "Found ${JAR}"

set +e
USING_ASAN=$(otool -L $MODELIMPORTER_SHARED_LIB | grep -e "clang_rt.asan")
USING_UBSAN=$(otool -L $MODELIMPORTER_SHARED_LIB | grep -e "clang_rt.ubsan")
set -e

PACKGED_XCODE=${DYNAMO_HOME}/ext/SDKs/
PACKGED_XCODE_TOOLCHAIN=${PACKGED_XCODE}/Toolchains/XcodeDefault.xctoolchain
LOCAL_XCODE=$(xcode-select -print-path)
LOCAL_XCODE_TOOLCHAIN=${LOCAL_XCODE}/Toolchains/XcodeDefault.xctoolchain

if [ "${USING_ASAN}" != "" ]; then
    echo "Finding ASAN!"

    if [ -d "${PACKGED_XCODE_TOOLCHAIN}" ]; then
        ASAN_LIB=$(find ${PACKGED_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic.dylib")
    fi
    if [ ! -e ${ASAN_LIB} ]; then
        ASAN_LIB=$(find ${LOCAL_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic.dylib")
    fi

    echo "ASAN_LIB=${ASAN_LIB}"
    export DYLD_INSERT_LIBRARIES=${ASAN_LIB}
fi
if [ "${USING_UBSAN}" != "" ]; then
    echo "Finding UBSAN!"

    if [ -d "${PACKGED_XCODE_TOOLCHAIN}" ]; then
        echo "LOoking in packaged sdks"
        UBSAN_LIB=$(find ${PACKGED_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic.dylib")
    fi
    if [ "${UBSAN_LIB}" == "" ]; then
        echo "LOoking in local installs"
        UBSAN_LIB=$(find ${LOCAL_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic.dylib")
    fi

    echo "UBSAN_LIB=${UBSAN_LIB}"

    export DYLD_INSERT_LIBRARIES=${UBSAN_LIB}
fi

#JNI_DEBUG_FLAGS="-Xcheck:jni"

export DM_MODELIMPORTER_LOG_LEVEL=DEBUG

java ${JNI_DEBUG_FLAGS} -Djava.library.path=${BUILD_DIR} -Djni.library.path=${BUILD_DIR} ${JNA_DEBUG_FLAGS} -cp ${JAR} ${CLASS_NAME} $*
