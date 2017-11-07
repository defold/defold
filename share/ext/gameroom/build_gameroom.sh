#!/bin/bash

# Not possible to download SDK directly while it's in beta.
# readonly BASE_URL=https://origincache.facebook.com/developers/resources/?id=
readonly FILE_URL=GameroomPlatformSDK_05.zip
readonly PRODUCT=facebook-gameroom
readonly VERSION=2017-08-14

readonly SDK_ZIP_PATH=../download/$FILE_URL

if [ ! -f $SDK_ZIP_PATH ]; then
    echo "Cannot automatically download the Facebook Gameroom SDK while in beta,"
    echo "please download it manually place SDK zip under ./downloads/"
    echo "https://developers.facebook.com/docs/games/gameroom/sdk"
    exit 1
fi

. ../common.sh

function cmi_unpack() {
    unzip ../../download/$FILE_URL
}

function cmi_make() {
    set +e

    mkdir -p $PREFIX/lib/$PLATFORM
    mkdir -p $PREFIX/include/

    # Copy library depending on build platform
    case $PLATFORM in
        win32)
            cp -v LibFBGPlatform32.lib $PREFIX/lib/$PLATFORM/libFBGPlatform.lib
            ;;
        x86_64-win32)
            cp -v LibFBGPlatform64.lib $PREFIX/lib/$PLATFORM/libFBGPlatform.lib
            ;;
    esac

    # Copy headers and .cpp file
    cp -v include/*.* $PREFIX/include
    cp -v FBG_PlatformLoader.cpp $PREFIX/include

    set +e
}

function cmi_configure() {
    echo "Nothing to configure"
}

# Can't download SDK currently, needs to be downloaded manually for now, see comment at top.
# download
cmi $1
