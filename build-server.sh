#!/bin/bash
set -e

rm -rf build
. ./build-common.sh

prebuild
gitclone
build build_server.xml server.properties
postbuild
