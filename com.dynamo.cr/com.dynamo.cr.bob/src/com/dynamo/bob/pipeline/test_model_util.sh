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



# We create the jar via "./scripts/build.py build_bob_light --skip-tests"

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -e

MODEL_PATH=$1

CLASS_NAME=ModelUtil
case $MODEL_PATH in *.dae|*.DAE)
    CLASS_NAME=ColladaUtil;;
esac

DEFOLD_HOME=$DYNAMO_HOME/../..
MODELC_BUILD_DIR=${DEFOLD_HOME}/engine/modelc/build/src

PACKAGE_CLASS=com.dynamo.bob.pipeline.$CLASS_NAME
JAR=${DYNAMO_HOME}/share/java/bob-light.jar

#JNI_DEBUG_FLAGS="-Xcheck:jni"
#export DYLD_INSERT_LIBRARIES=${JAVA_HOME}/lib/libjsig.dylib

echo "java.library.path ${MODELC_BUILD_DIR}"
echo "Running jar:" $JAR
echo "Using main class:" ${CLASS_NAME}

MODELINPUT=$1
shift

java ${JNI_DEBUG_FLAGS} -Djava.library.path=${MODELC_BUILD_DIR} -cp ${JAR} ${PACKAGE_CLASS} "${MODELINPUT}" $*
