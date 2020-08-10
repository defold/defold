#!/usr/bin/env bash

# list is found by doing
# $ find . -iname "*switch*" > files.txt
# $ find . -iname "*nx*" >> files.txt

PATHS="./scripts/package/package_switch_sdk.sh;\
./switch;\
./README_SWITCH.md;\
./com.dynamo.cr/com.dynamo.cr.bob/lib/switch-tools.zip;\
./com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/bundle/SwitchBundler.java;\
./engine/docs/src/switch.apidoc_;\
./engine/switch;\
./editor/test/resources/test_project/switcher;\
./editor/test/resources/test_project/switcher/switcher.atlas;\
./share/ext/luajit/switchbuild.bat;\
./packages/luajit-2.1.0-beta3-arm64-nx64.tar.gz;\
./packages/protobuf-2.3.0-arm64-nx64.tar.gz;\
./packages/bullet-2.77-arm64-nx64.tar.gz;\
./com.dynamo.cr/com.dynamo.cr.bob/libexec/arm64-nx64;\
./engine/testmain/src/nx64;\
./engine/dlib/src/test/nx64;\
./engine/dlib/src/dlib/nx64;\
./engine/hid/src/nx64;\
./engine/graphics/src/nx64;\
./engine/sound/src/devices/device_nx64.cpp;\
./engine/engine/content/builtins/manifests/nx64;\
./engine/engine/src/nx64;\
./editor/test/resources/test_project/car/env/nx.jpg;\
"

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
