#!/usr/bin/env bash

readonly PRODUCT=PVRTexLib
readonly VERSION=4.18.0

if [ "Linux" == `uname` ]; then
	readonly INSTALL_DIR=/opt/Imagination/PowerVR_Graphics/PowerVR_Tools/PVRTexTool/Library
else
	readonly INSTALL_DIR=/Applications/Imagination/PowerVR_Graphics/PowerVR_Tools/PVRTexTool/Library
fi

if [ "Linux" == `uname` ]; then
	if [ ! -d $INSTALL_DIR ]; then
		echo "Download and install the Native SDK and make sure to install the prebuilt binaries!"
		echo "https://community.imgtec.com/developers/powervr/offline-installers/"
		echo "E.g. https://community.imgtec.com/downloads/linux-64-offline-installer-powervr-tools-sdk-2017-r1/"
		echo "chmod a+x ~/Downloads/PowerVRSDKSetup-2017_R1.run-x64"
		echo "sudo ~/Downloads/PowerVRSDKSetup-2017_R1.run-x64"

		echo "Also note, that you'll need to change the executable path for the libPVRTexLib.dylib:"
		echo "install_name_tool -id @executable_path/libPVRTexLib.dylib $INSTALL_DIR/OSX_x86/libPVRTexLib.dylib"
		exit 1
	fi
fi

export CONF_TARGET=$1

. ../common.sh

SOURCE_DIR=

case $CONF_TARGET in
	linux)
		SOURCE_DIR=$INSTALL_DIR/Linux_x86_32
		;;
	x86_64-linux)
		SOURCE_DIR=$INSTALL_DIR/Linux_x86_64
		;;
	darwin|x86_64-darwin)
		SOURCE_DIR=$INSTALL_DIR/OSX_x86
		;;
	win32)
		SOURCE_DIR=$INSTALL_DIR/Windows_x86_32
		;;
	x86_64-win32)
		SOURCE_DIR=$INSTALL_DIR/Windows_x86_64
		;;
esac


function cmi_unpack() {
	echo "Nothing to unpack"
}

function cmi_patch() {
	echo "Nothing to patch"
}

function cmi_configure() {
	echo "Nothing to configure"
}

function cmi_make() {
	set +e

	echo $PWD
	echo $PREFIX/lib/$CONF_TARGET
	# "install"
	mkdir -p $PREFIX/lib/$CONF_TARGET
	mkdir -p $PREFIX/include/

	cp -v $SOURCE_DIR/*.* $PREFIX/lib/$CONF_TARGET
	cp -v $INSTALL_DIR/Include/*.* $PREFIX/include


	case $CONF_TARGET in
		darwin|x86_64-darwin)
			install_name_tool -id @loader_path/libPVRTexLib.dylib $PREFIX/lib/$CONF_TARGET/libPVRTexLib.dylib
			;;
	esac

	set +e
}

cmi $1