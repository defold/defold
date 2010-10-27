#!/bin/bash
set -e

rm -rf build
. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.server com.dynamo.cr.server.test com.dynamo.cr.test"

prebuild
gitclone
cp -r features/com.dynamo.cr.server.test.feature $BUILD_DIRECTORY/features
build build_server_test.xml server-test.properties

CLASSPATH=.:`pwd`/junit-4.8.2.jar
CLASSPATH=$CLASSPATH:`pwd`/build/plugins/com.dynamo.cr.test/lib/org.hamcrest.core_1.1.0.v20090501071000.jar
CLASSPATH=$CLASSPATH:$DYNAMO_HOME/ext/share/java/protobuf-java-2.3.0.jar

for j in $( ls `pwd`/build/plugins/com.dynamo.cr.server/ext/*.jar ); do
    CLASSPATH=$CLASSPATH:$j
done
for j in $( ls `pwd`/build/buildRepo/plugins/*.jar ); do
    CLASSPATH=$CLASSPATH:$j
done

export CLASSPATH

pushd build/plugins/com.dynamo.cr.server > /dev/null
java org.junit.runner.JUnitCore com.dynamo.cr.server.resources.test.ProjectResourceTest
java org.junit.runner.JUnitCore com.dynamo.cr.server.git.test.GitTest
popd >/dev/null
