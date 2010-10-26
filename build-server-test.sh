#!/bin/bash
set -e

rm -rf build
. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.server com.dynamo.cr.server.test com.dynamo.cr.test"

prebuild
gitclone
cp -r features/com.dynamo.cr.server.test.feature $BUILD_DIRECTORY/features
build build_server_test.xml server-test.properties

rm -rf tmp
mkdir -p tmp/configuration
mkdir -p tmp/plugins
cp *.jar tmp
for p in $( ls $BUILD_DIRECTORY/buildRepo/plugins/*.jar ); do
    base=`basename $p`
    mkdir -p tmp/plugins/$base
    pushd tmp/plugins/$base  > /dev/null
    jar xf $p
    popd > /dev/null
done
test_bundle=`ls $BUILD_DIRECTORY/buildRepo/plugins/com.dynamo.cr.test_*.jar`
c=tmp/configuration/config.ini
echo "osgi.bundles=org.eclipse.equinox.common_3.6.0.v20100503.jar@start,org.eclipse.update.configurator_3.3.100.v20100512.jar@start,${test_bundle}@start" > $c
pushd tmp  > /dev/null

java -DtestBundle=com.dynamo.cr.server.test\
     -DtestClass=com.dynamo.cr.server.git.test.GitTest\
     -jar org.eclipse.osgi_3.6.1.R36x_v20100806.jar\
     -configuration configuration
popd > /dev/null
