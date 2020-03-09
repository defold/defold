#!/usr/bin/env bash

# Download and install SDK and make sure NINTENDO_SDK_ROOT is set

# Run from msys
# ./package_switch_sdk.sh

set -e

VERSION="10.1"
SDK_NAME=nx64-sdk-${VERSION}

TARGET_PATH=$(pwd)
TMP_PATH=${TARGET_PATH}/packages
if [ ! -d "${TMP_PATH}" ]; then
	mkdir ${TMP_PATH}
	echo "Created ${TMP_PATH}"
fi

OUT_ARCHIVE=${TMP_PATH}/${SDK_NAME}.tar.gz
if [ ! -e ${OUT_ARCHIVE} ]; then
	echo "Packing ${SDK_NAME}..."

	## first create the archive
	GZIP=-9 tar cvf ${OUT_ARCHIVE} -C "${NINTENDO_SDK_ROOT}" Compilers/NX/nx/aarch64/ Include Libraries/NX-NXFP2-a64 Common/Configs/Targets Resources/SpecFiles Tools/CommandLineTools

	echo "Wrote ${OUT_ARCHIVE}"
else
	echo "Found ${OUT_ARCHIVE}"
fi
