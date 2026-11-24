#!/usr/bin/env bash
# We create the jar via "./scripts/build.py build_bob_light --skip-tests"

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -e

DEFOLD_HOME=$DYNAMO_HOME/../..

PACKAGE_CLASS=com.dynamo.bob.tile.ConvexHull2D
JAR=${DYNAMO_HOME}/share/java/bob-light.jar

echo "Running jar:" $JAR
echo "Using main class:" ${PACKAGE_CLASS}

java -cp ${JAR} ${PACKAGE_CLASS} $*
