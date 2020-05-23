#!/usr/bin/env bash
# Copyright 2020 The Defold Foundation
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
# 
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
# 
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.



# Download and install Visual Studio 2019
# 	https://visualstudio.microsoft.com/downloads/
# Download Windows SDK 8 and 10
#  	https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

# Run from msys
# ./package_win32_sdk.sh

set -e

SDK_10_VERSION="10.0.18362.0"
MSVC_VERSION="14.25.28610"

VC_PATH="/c/Program Files (x86)/Microsoft Visual Studio/2019/Community"

SDK_PATH="C:\Program Files (x86)\Windows Kits"

PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-2019-${MSVC_VERSION}.tar.gz"
PACKAGES_WIN32_SDK_8="WindowsKits-8.1.tar.gz"
PACKAGES_WIN32_SDK_10="WindowsKits-${SDK_10_VERSION}.tar.gz"


TARGET_PATH=$(pwd)
TMP_PATH=${TARGET_PATH}/packages
if [ ! -d "${TMP_PATH}" ]; then
	mkdir ${TMP_PATH}
fi

# if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_8}" ]; then
# 	echo "Packing to ${PACKAGES_WIN32_SDK_8}"
# 	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_8} -C "${SDK_PATH}" 8.1/Include 8.1/Lib 8.1/sdk_license.rtf 8.1/sdk_third_party_notices.rtf
# else
# 	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_8} already existed"
# fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_10}" ]; then
	echo "Packing to ${PACKAGES_WIN32_SDK_10}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} -C "${SDK_PATH}" 10/Include/${SDK_10_VERSION} 10/Lib/${SDK_10_VERSION}/um/x86 10/Lib/${SDK_10_VERSION}/um/x64 10/Lib/${SDK_10_VERSION}/ucrt/x86 10/Lib/${SDK_10_VERSION}/ucrt/x64 10/Licenses
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} already existed"
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN}" ]; then
	echo "Packing to ${PACKAGES_WIN32_TOOLCHAIN}"
	TMP=MicrosoftVisualStudio2019

	mkdir -p $TMP/VC/Tools/MSVC/$MSVC_VERSION/bin/Hostx64
	mkdir -p $TMP/VC/Tools/MSVC/$MSVC_VERSION/bin/Hostx86
	mkdir -p $TMP/VC/Tools/MSVC/$MSVC_VERSION/include
	mkdir -p $TMP/VC/Tools/MSVC/$MSVC_VERSION/lib/x64
	mkdir -p $TMP/VC/Tools/MSVC/$MSVC_VERSION/lib/x86
	mkdir -p $TMP/VC/Tools/MSVC/$MSVC_VERSION/atlmfc

	cp -r -v "$VC_PATH/VC/Tools/MSVC/$MSVC_VERSION/bin/Hostx64/x64" "$TMP/VC/Tools/MSVC/$MSVC_VERSION/bin/Hostx64"
	cp -r -v "$VC_PATH/VC/Tools/MSVC/$MSVC_VERSION/bin/Hostx64/x86" "$TMP/VC/Tools/MSVC/$MSVC_VERSION/bin/Hostx86"
	cp -r -v "$VC_PATH/VC/Tools/MSVC/$MSVC_VERSION/include" "$TMP/VC/Tools/MSVC/$MSVC_VERSION"
	cp -r -v "$VC_PATH/VC/Tools/MSVC/$MSVC_VERSION/lib/x64" "$TMP/VC/Tools/MSVC/$MSVC_VERSION/lib"
	cp -r -v "$VC_PATH/VC/Tools/MSVC/$MSVC_VERSION/lib/x86" "$TMP/VC/Tools/MSVC/$MSVC_VERSION/lib"
	cp -r -v "$VC_PATH/VC/Tools/MSVC/$MSVC_VERSION/atlmfc"  "$TMP/VC/Tools/MSVC/$MSVC_VERSION"

	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} -C "$TMP" VC
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} already existed"
fi

echo "Done."
