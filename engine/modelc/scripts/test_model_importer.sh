#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
BUILD_DIR=./build/default/src

set -e

CLASS_NAME=com.dynamo.bob.pipeline.ModelImporter

MODELIMPORTER_SHARED_LIB=$(find . -iname "*.dylib")
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
if [ "${USING_ASAN}" != "" ]; then
    echo "Loading ASAN!"
    export DYLD_INSERT_LIBRARIES=${DYNAMO_HOME}/ext/SDKs/XcodeDefault13.2.1.xctoolchain/usr/lib/clang/13.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib
fi
if [ "${USING_UBSAN}" != "" ]; then
    echo "Loading UBSAN!"
    export DYLD_INSERT_LIBRARIES=${DYNAMO_HOME}/ext/SDKs/XcodeDefault13.2.1.xctoolchain/usr/lib/clang/13.0.0/lib/darwin/libclang_rt.ubsan_osx_dynamic.dylib
fi

JNI_DEBUG_FLAGS="-Xcheck:jni"

export DM_MODELIMPORTER_LOG_LEVEL=DEBUG

java ${JNI_DEBUG_FLAGS} -Djava.library.path=${BUILD_DIR} -Djni.library.path=${BUILD_DIR} ${JNA_DEBUG_FLAGS} -cp ${JAR} ${CLASS_NAME} $*
