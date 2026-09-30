#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
eval $(python $SCRIPT_DIR/../../../build_tools/set_sdk_vars.py VERSION_XCODE)
pushd $SCRIPT_DIR/..
BUILD_DIR=$(realpath ./build/src)

set -e

CLASS_NAME=com.dynamo.bob.pipeline.TexcLibraryJni
LIBNAME=texc_shared
SUFFIX=.so
if [ "Darwin" == "$(uname)" ]; then
    SUFFIX=.dylib
elif [[ "$OSTYPE" == "cygwin" ]]; then
    # POSIX compatibility layer and Linux environment emulation for Windows
    SUFFIX=.dll
elif [[ "$OSTYPE" == "msys" ]]; then
    # Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
    SUFFIX=.dll
elif [[ "$OSTYPE" == "win32" ]]; then
    # I'm not sure this can happen.
    SUFFIX=.dll
fi

SHARED_LIB=./build/src/lib${LIBNAME}${SUFFIX}
if [ -z "${SHARED_LIB}" ]; then
    echo "Couldn't find the shared library!"
fi
echo "Found ${SHARED_LIB}"

JAR=$(find . -iname "*.jar")
if [ -z "${JAR}" ]; then
    echo "Couldn't find the jar file!"
fi
echo "Found ${JAR}"

if [ "Darwin" == "$(uname)" ]; then

    set +e
    if [ "Darwin" == "$(uname)" ]; then
        USING_ASAN=$(otool -L $SHARED_LIB | grep -e "clang_rt.asan")
        USING_UBSAN=$(otool -L $SHARED_LIB | grep -e "clang_rt.ubsan")
    fi
    set -e

    PACKAGED_XCODE=${DYNAMO_HOME}/ext/SDKs/
    PACKAGED_XCODE_TOOLCHAIN=${PACKAGED_XCODE}/XcodeDefault${VERSION_XCODE}.xctoolchain
    LOCAL_XCODE=$(xcode-select -print-path)
    LOCAL_XCODE_TOOLCHAIN=${LOCAL_XCODE}/Toolchains/XcodeDefault.xctoolchain
fi

if [ "${USING_ASAN}" != "" ]; then
    echo "Finding ASAN!"

    if [ -d "${PACKAGED_XCODE_TOOLCHAIN}" ]; then
        ASAN_LIB=$(find ${PACKAGED_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic${SUFFIX}")
    fi
    if [ ! -e ${ASAN_LIB} ]; then
        ASAN_LIB=$(find ${LOCAL_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic${SUFFIX}")
    fi

    echo "ASAN_LIB=${ASAN_LIB}"
    export DYLD_INSERT_LIBRARIES=${ASAN_LIB}
fi
if [ "${USING_UBSAN}" != "" ]; then
    echo "Finding UBSAN!"

    if [ -d "${PACKAGED_XCODE_TOOLCHAIN}" ]; then
        echo "LOoking in packaged sdks"
        UBSAN_LIB=$(find ${PACKAGED_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic${SUFFIX}")
    fi
    if [ "${UBSAN_LIB}" == "" ]; then
        echo "LOoking in local installs"
        UBSAN_LIB=$(find ${LOCAL_XCODE_TOOLCHAIN}/usr/lib/clang -iname "libclang_rt.asan_osx_dynamic${SUFFIX}")
    fi

    echo "UBSAN_LIB=${UBSAN_LIB}"

    export DYLD_INSERT_LIBRARIES=${UBSAN_LIB}
fi

#JNI_DEBUG_FLAGS="-Xcheck:jni"
#export DYLD_INSERT_LIBRARIES=${JAVA_HOME}/lib/libjsig.dylib

export DM_TEXTURECOMPILER_LOG_LEVEL=DEBUG
export DM_ATLASPACKER_LOG_LEVEL=DEBUG

JAR_FOLDER=${DYNAMO_HOME}/../../com.dynamo.cr/com.dynamo.cr.bob/bin/lib/*

java ${JNI_DEBUG_FLAGS} -Djava.library.path=${BUILD_DIR} -Djni.library.path=${BUILD_DIR} -cp "${JAR}:${JAR_FOLDER}" ${CLASS_NAME} $*
