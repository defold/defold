#!/bin/bash
set -e

rm -rf build
. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.server com.dynamo.cr.server.test com.dynamo.cr.test"

prebuild
gitclone
build build_server.xml server.properties
postbuild
