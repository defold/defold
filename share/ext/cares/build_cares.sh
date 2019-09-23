#!/usr/bin/env bash

export CONF_TARGET=$1

# Build cares with debug info
export CONFIGURE_FLAGS="--enable-debug"

readonly PRODUCT=cares
readonly TAR_INCLUDES=1
readonly VERSION=602aaec984f862a5d59c9eb022f4317954c53917 #804753224fc27f806992ed8c7fe52e5762fd317a
readonly BASE_URL=https://github.com/c-ares/c-ares.git

. ../common.sh

TAR_SKIP_BIN=1

function cmi_cleanup() {
	echo "Skipping cleanup.."
}

case $CONF_TARGET in
    *arm64-android)
        function cmi_unpack() {
            echo -e "Checking out cares into cares_android_arm64"
            git clone $BASE_URL cares_android_arm64
            pushd cares_android_arm64
            git checkout $VERSION

            ./buildconf
        }
        ;;
    *)
        echo "Platform not implemented/supported"
        exit 1
        ;;
esac

cmi $1
