#!/bin/bash
set -e

rm -rf build
. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.server"

prebuild
gitclone
build build_server.xml server.properties
postbuild
