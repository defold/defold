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


VC_PATH="/c/Program Files (x86)/Microsoft Visual Studio/2019/Community"
SDK_PATH="C:\Program Files (x86)\Windows Kits"


# Set it specifically if you need to, otherwise it will find the highest number
#MSVC_VERSION="14.29.30037"
#SDK_10_VERSION="10.0.18362.0"

if [ -e $MSVC_VERSION ]; then
	# get the highest version number
	MSVC_VERSION=$(ls "$VC_PATH/VC/Tools/MSVC/" | tail -n 1)
fi

if [ -e $SDK_10_VERSION ]; then
	# get the highest version number
	SDK_10_VERSION=$(ls "$SDK_PATH/10/Include/" | tail -n 1)
fi

if [ -e $MSVC_VERSION ]; then
	echo "No msvc version found at $VC_PATH/VC/Tools/MSVC"
	exit 1
fi

if [ -e $SDK_10_VERSION ]; then
	echo "No windows sdk version found at $SDK_PATH/10/Include"
	exit 1
fi

echo MSVC_VERSION=${MSVC_VERSION}
echo SDK_10_VERSION=${SDK_10_VERSION}


PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-2019-${MSVC_VERSION}.tar.gz"
PACKAGES_WIN32_SDK_10="WindowsKits-${SDK_10_VERSION}.tar.gz"


TARGET_PATH=$(pwd)/local_sdks
TMP_PATH=${TARGET_PATH}/tmp
if [ ! -d "${TMP_PATH}" ]; then
	mkdir -p ${TMP_PATH}
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_10}" ]; then
	echo "Packing to ${PACKAGES_WIN32_SDK_10}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} -C "${SDK_PATH}" 10/Include/${SDK_10_VERSION} 10/Lib/${SDK_10_VERSION}/um/x86 10/Lib/${SDK_10_VERSION}/um/x64 10/Lib/${SDK_10_VERSION}/ucrt/x86 10/Lib/${SDK_10_VERSION}/ucrt/x64 10/Licenses 10/bin/${SDK_10_VERSION}/x64 10/bin/${SDK_10_VERSION}/x86
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} already existed"
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN}" ]; then
	echo "Packing to ${PACKAGES_WIN32_TOOLCHAIN}"
	TMP=${TMP_PATH}/MicrosoftVisualStudio2019

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

	rm -rf ${TMP}
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} already existed"
fi

echo "Done."
