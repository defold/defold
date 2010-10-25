#!/bin/bash
set -e

. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.server com.dynamo.cr.server.test com.dynamo.cr.test"

prebuild
gitclone
cp -r features/com.dynamo.cr.server.test.feature $BUILD_DIRECTORY/features
build build_server.xml server.properties
build build_server_test.xml server-test.properties
postbuild

rm -rf tmp/configuration
mkdir -p tmp/configuration
c=tmp/configuration/config.ini
echo -n "osgi.bundles=" > $c

for p in $( ls $BUILD_DIRECTORY/buildRepo/plugins/*.jar ); do
    echo -n "${p}" >> $c
    # Add @start for com.dynamo.cr.test, the bundle that runs the tests
    if [[ ${p} =~ com.dynamo.cr.test_ ]]; then echo -n "@start" >> $c; fi
    echo -n "," >> $c
done

java -DtestBundle=com.dynamo.cr.server.test\
     -DtestClass=com.dynamo.cr.server.git.test.GitTest\
     -jar org.eclipse.osgi_3.6.1.R36x_v20100806.jar\
     -configuration tmp/configuration
