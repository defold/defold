#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.


SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
eval $(python $SCRIPT_DIR/../../../build_tools/set_sdk_vars.py VERSION_XCODE)
pushd $SCRIPT_DIR/..
BUILD_DIR=$(realpath ${DYNAMO_HOME}/../../engine/modelc/build/src)

set -e

CLASS_NAME=com.dynamo.bob.pipeline.ModelImporterJni
LIBNAME=modelc_shared
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

if [ "Darwin" == "$(uname)" ]; then

    set +e
    if [ "Darwin" == "$(uname)" ]; then
        USING_ASAN=$(otool -L $MODELIMPORTER_SHARED_LIB | grep -e "clang_rt.asan")
        USING_UBSAN=$(otool -L $MODELIMPORTER_SHARED_LIB | grep -e "clang_rt.ubsan")
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

export DM_MODELIMPORTER_LOG_LEVEL=DEBUG

MODELINPUT=$1
shift
java ${JNI_DEBUG_FLAGS} -Djava.library.path=${BUILD_DIR} -Djni.library.path=${BUILD_DIR} ${JNA_DEBUG_FLAGS} -cp ${JAR} ${CLASS_NAME} "${MODELINPUT}" $*
