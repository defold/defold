#!/bin/sh
set -e

. ./build-common.sh

prebuild
gitclone
build build_editor.xml editor.properties
postbuild

