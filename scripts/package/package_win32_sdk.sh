#!/usr/bin/env bash
# Copyright 2020-2022 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

# Run from Windows Terminal in a git-bash environment
# ./package_win32_sdk.sh

set -e

VSWHERE=${DYNAMO_HOME}/../../scripts/windows/vswhere2/vswhere2.exe

echo "HELLO"
WIN_SDK_ROOT=`${VSWHERE} --sdk_root`
WIN_SDK_VERSION=`${VSWHERE} --sdk_version`
INCLUDES_PATHS=`${VSWHERE} --includes`
LIB_PATHS=`${VSWHERE} --lib_paths`
BIN_PATHS=`${VSWHERE} --bin_paths`
VS_ROOT=`${VSWHERE} --vs_root`
VS_VERSION=`${VSWHERE} --vs_version`
COMMUNITY_VERSION=2022
echo "WIN_SDK_ROOT" ${WIN_SDK_ROOT}
echo "WIN_SDK_VERSION" ${WIN_SDK_VERSION}
echo "INCLUDES_PATHS" ${INCLUDES_PATHS}
echo "LIB_PATHS" ${LIB_PATHS}
echo "BIN_PATHS" ${BIN_PATHS}
echo "VS_ROOT" ${VS_ROOT}
echo "VS_VERSION" ${VS_VERSION}


PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-${COMMUNITY_VERSION}-${VS_VERSION}.tar.gz"
PACKAGES_WIN32_SDK_10="WindowsKits-${WIN_SDK_VERSION}.tar.gz"


TARGET_PATH=$(pwd)/local_sdks
TMP_PATH=${TARGET_PATH}/tmp
if [ ! -d "${TMP_PATH}" ]; then
	mkdir -p ${TMP_PATH}
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_10}" ]; then
	echo "Packing to ${PACKAGES_WIN32_SDK_10}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} -C "${WIN_SDK_ROOT}/.." 10/Include/${WIN_SDK_VERSION} 10/Lib/${WIN_SDK_VERSION}/um/x86 10/Lib/${WIN_SDK_VERSION}/um/x64 10/Lib/${WIN_SDK_VERSION}/ucrt/x86 10/Lib/${WIN_SDK_VERSION}/ucrt/x64 10/Licenses 10/bin/${WIN_SDK_VERSION}/x64 10/bin/${WIN_SDK_VERSION}/x86
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} already existed"
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN}" ]; then
	echo "Packing to ${PACKAGES_WIN32_TOOLCHAIN}"
	TMP=${TMP_PATH}/MicrosoftVisualStudio${COMMUNITY_VERSION}
	TARGETDIR=${TMP}/VC/Tools/MSVC/${VS_VERSION}

	mkdir -p ${TARGETDIR}/bin/Hostx64
	mkdir -p ${TARGETDIR}/bin/Hostx86
	mkdir -p ${TARGETDIR}/include
	mkdir -p ${TARGETDIR}/lib/x64
	mkdir -p ${TARGETDIR}/lib/x86
	mkdir -p ${TARGETDIR}/atlmfc

	cp -r -v "${VS_ROOT}/bin/Hostx64/x64" "${TARGETDIR}/bin/Hostx64"
	cp -r -v "${VS_ROOT}/bin/Hostx64/x86" "${TARGETDIR}/bin/Hostx86"
	cp -r -v "${VS_ROOT}/include" "${TARGETDIR}"
	cp -r -v "${VS_ROOT}/lib/x64" "${TARGETDIR}/lib"
	cp -r -v "${VS_ROOT}/lib/x86" "${TARGETDIR}/lib"

	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} -C "$TMP" VC
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} already existed"
fi

echo "Done."
