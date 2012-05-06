#!/bin/sh
set -e

. ./build-common.sh

prebuild
gitclone
echo 'root.linux.gtk.x86=absolute:${buildDirectory}/plugins/com.dynamo.cr.editor/jre_linux/' > build/root.properties
echo 'root.linux.gtk.x86.permissions.755=jre/'  >> build/root.properties
echo 'root.win32.win32.x86.folder.jre_win32=absolute:${buildDirectory}/plugins/com.dynamo.cr.editor/jre_win32/jre' >> build/root.properties
build build_editor.xml editor.properties editor
postbuild

