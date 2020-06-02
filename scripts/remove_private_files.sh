#!/usr/bin/env bash

PATHS="./build_tools/private/waf_dynamo_private.py;\
./build_tools/private/waf_dynamo_private.pyc;\
./scripts/build_private.py;\
./scripts/build_private.pyc;\
./packages/protobuf-2.3.0-arm64-nx64.tar.gz;\
./engine/dlib/src/dlib/nx64;\
./engine/app/src/nx64;\
./engine/graphics/src/nx64;\
./engine/hid/src/nx64;\
./engine/sound/src/devices/device_nx64.cpp;\
./engine/engine/content/builtins/manifests/nx64;\
./switch"

echo "This will remove these paths: "

export IFS=";"
for path in $PATHS; do
	if [ $path != "" ]; then
		echo $path
	fi
done

read -p "Are you sure? " -n 1 -r
echo    # (optional) move to a new line
if [[ $REPLY =~ ^[Yy]$ ]]
then
    # do dangerous stuff
	echo "Removing files"

	for path in $PATHS; do
		if [ $path != "" ]; then
			rm -rf $path
		fi
	done

    exit 0
fi

echo "exited without changes"
exit 1
