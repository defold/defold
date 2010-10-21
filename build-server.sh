#!/bin/sh

export BUILD_PROPERTIES=server.properties
. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.server"

prebuild
gitclone
build build_server.xml
postbuild
