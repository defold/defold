#!/bin/sh
set -e

. ./build-common.sh

prebuild
gitclone
echo 'root.linux.gtk.x86=absolute:${buildDirectory}/plugins/com.dynamo.cr.editor/jre_linux/' > build/root.properties
echo 'root.linux.gtk.x86.permissions.755=jre/'  >> build/root.properties
# NOTE:
# Due to bug https://bugs.eclipse.org/bugs/show_bug.cgi?id=300812
# we cannot add jre to p2 on win32
# The jre is explicitly bundled instead. See below
echo 'root.win32.win32.x86=absolute:${buildDirectory}/plugins/com.dynamo.cr.editor/jre_win32/' >> build/root.properties
build build_editor.xml editor.properties editor

# Explicit bundling of jre on win32. See comment above about bug
#export CR_EDITOR_PATH_WIN32=`pwd`/build/I.Defold/Defold-win32.win32.x86.zip
#python zip_jre.py $CR_EDITOR_PATH_WIN32 build/plugins/com.dynamo.cr.editor/jre_win32

postbuild
