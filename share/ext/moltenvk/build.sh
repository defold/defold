#!/usr/bin/env bash

export CONF_TARGET=$1

readonly PRODUCT=moltenvk
readonly VERSION=1.0.30

. ../common.sh

function cmi_configure() {
    echo "No configure exists"
}

function cmi_unpack() {
    echo "No unpack exists"
}

case $CONF_TARGET in
    *x86_64-darwin)

        function cmi_make() {

        	echo -e "Checking out MoltenVK"
        	git clone git@github.com:KhronosGroup/MoltenVK.git

        	pushd MoltenVK

        	git checkout tags/v$VERSION

			git apply ../patch_$VERSION.patch

        	./fetchDependencies

        	xcodebuild -project MoltenVKPackaging.xcodeproj -scheme "MoltenVK Package (macOS only)" build

        	popd

        	echo -e "Copying library to build folder"
            mkdir -p $PREFIX/lib/$CONF_TARGET
            mkdir -p $PREFIX/include/$CONF_TARGET/

            cp -v MoltenVK/Package/Release/MoltenVK/macOS/static/libMoltenVK.a $PREFIX/lib/$CONF_TARGET/
            cp -vr MoltenVK/Package/Release/MoltenVK/include/* $PREFIX/include/$CONF_TARGET/
        }
        ;;
    *)
        echo "Platform not implemented/supported"
        exit 1
        ;;
esac

cmi $1
