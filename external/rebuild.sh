#!/usr/bin/env bash

LIB=$1
shift

ARG=$1
shift

PLATFORMS_MACOS="arm64-macos x86_64-macos"
PLATFORMS_LINUX="arm64-linux x86_64-linux"
PLATFORMS_WINDOWS="x86_64-win32 x86-win32"
PLATFORMS_PLAYSTATION="x86_64-ps4 x86_64-ps5"
PLATFORMS_NINTENDO="arm64-nx64"
PLATFORMS_IOS="arm64-ios x86_64-ios"
PLATFORMS_ANDROID="arm64-android armv7-android"
PLATFORMS_WEB="js-web wasm-web wasm_pthread-web"

PLATFORMS="arm64-macos x86_64-macos arm64-linux x86_64-linux"

if [ $(uname) == "Darwin" ]; then
    PLATFORMS="${PLATFORMS_MACOS} ${PLATFORMS_IOS} ${PLATFORMS_ANDROID} ${PLATFORMS_WEB}"
fi

if [ $(uname) == "Linux" ]; then
    PLATFORMS="${PLATFORMS_LINUX}"
fi

if [ $(uname) == "Windows" ]; then
    PLATFORMS="${PLATFORMS_WINDOWS}"

    if [ "${ARG}" == "playstation" ]; then
        PLATFORMS="${PLATFORMS_PLAYSTATION}"
    fi

    if [ "${ARG}" == "nintendo" ]; then
        PLATFORMS="${PLATFORMS_NINTENDO}"
    fi
fi

echo "************************************"
echo "Building library ${LIB}"
echo "************************************"

for platform in $PLATFORMS ; do
    echo "************************************"
    echo "PLATFORM = ${platform}"

    (cd ${LIB} && PREFIX=${DYNAMO_HOME} waf configure --platform=${platform})
    (cd ${LIB} && PREFIX=${DYNAMO_HOME} waf install --platform=${platform})
done


echo "************************************"
echo "Done!"
echo "************************************"
