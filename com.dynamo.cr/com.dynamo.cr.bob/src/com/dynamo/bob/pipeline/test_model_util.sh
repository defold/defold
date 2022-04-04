#!/usr/bin/env bash

# We create the jar via "./scripts/build.py build_bob_light --skip-tests"

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -e

CLASS_NAME=ModelUtil

#DEFOLD_DIR=$DYNAMO_HOME/../..
#DLIB_BUILD_DIR=$DEFOLD_DIR/engine/dlib/build/default/src/

# JNA=$DEFOLD_DIR/com.dynamo.cr/com.dynamo.cr.bob/lib/jna-5.10.0.jar
# JNA_PLATFORM=$DEFOLD_DIR/com.dynamo.cr/com.dynamo.cr.bob/lib/jna-platform-5.10.0.jar


PACKAGE_CLASS=com.dynamo.bob.pipeline.$CLASS_NAME
JAR=${DYNAMO_HOME}/share/java/bob-light.jar

echo "Running jar:" $JAR

java ${JNA_DEBUG_FLAGS} -cp ${JAR} ${PACKAGE_CLASS}  $*
