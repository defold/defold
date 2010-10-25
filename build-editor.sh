#!/bin/sh
set -e

. ./build-common.sh

PROJECTS="com.dynamo.cr.common com.dynamo.cr.editor com.dynamo.cr.contenteditor com.dynamo.cr.luaeditor"

prebuild
gitclone
build build_editor.xml editor.properties
postbuild

