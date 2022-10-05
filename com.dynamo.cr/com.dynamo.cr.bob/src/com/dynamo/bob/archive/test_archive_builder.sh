#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -e

CLASS_NAME=com.dynamo.bob.archive.ArchiveBuilder

BOB=$(find ${DYNAMO_HOME}/share/java -iname "bob-light.jar")
if [ -z "${BOB}" ]; then
    echo "Couldn't find the jar file!"
fi
echo "Found ${BOB}"

#JNI_DEBUG_FLAGS="-Xcheck:jni"

java ${JNI_DEBUG_FLAGS} -Djava.library.path=${BUILD_DIR} -Djni.library.path=${BUILD_DIR} ${JNA_DEBUG_FLAGS} -cp ${BOB} ${CLASS_NAME} $*
