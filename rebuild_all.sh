#!/bin/bash
set -e

for lib in "dlib" "ddf" "lua" "graphics" "sound" "physics" "hid" "resource" "script" "gameobject" "gui" "input" "render" "particle" "gamesys" "engine" ; do
	cd $lib
	waf distclean configure --prefix=$DYNAMO_HOME build install
	cd ..
	if [ $? -ne 0 ] ; then echo $? ; fi
done
