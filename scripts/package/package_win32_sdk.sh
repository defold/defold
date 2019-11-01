#!/usr/bin/env bash

# Download and install Visual Studio 2019
# 	https://visualstudio.microsoft.com/downloads/
# Download Windows SDK 8 and 10
#  	https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

# Run from msys
# ./package_win32_sdk.sh

set -e

SDK_10_VERSION="10.0.18362.0"
MSVC_VERSION="14.23.28105"

VS_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"

SDK_PATH="C:\Program Files (x86)\Windows Kits"

PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-2019-${MSVC_VERSION}.tar.gz"
PACKAGES_WIN32_SDK_8="WindowsKits-8.1.tar.gz"
PACKAGES_WIN32_SDK_10="WindowsKits-${SDK_10_VERSION}.tar.gz"


TARGET_PATH=$(pwd)
TMP_PATH=${TARGET_PATH}/packages
if [ ! -d "${TMP_PATH}" ]; then
	mkdir ${TMP_PATH}
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_8}" ]; then
	echo "Packing to ${PACKAGES_WIN32_SDK_8}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_8} -C "${SDK_PATH}" 8.1/Include 8.1/Lib 8.1/sdk_license.rtf 8.1/sdk_third_party_notices.rtf
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_8} already existed"
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_10}" ]; then
	echo "Packing to ${PACKAGES_WIN32_SDK_10}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} -C "${SDK_PATH}" 10/Include/${SDK_10_VERSION} 10/Lib/${SDK_10_VERSION}/um/x86 10/Lib/${SDK_10_VERSION}/um/x64 10/Lib/${SDK_10_VERSION}/ucrt/x86 10/Lib/${SDK_10_VERSION}/ucrt/x64 10/Licenses
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} already existed"
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN}" ]; then
	echo "Packing to ${PACKAGES_WIN32_TOOLCHAIN}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} -C "${VS_PATH}" DIA\ SDK VC/Tools
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} already existed"
fi

echo "Done."
