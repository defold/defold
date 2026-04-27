#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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



# Download and install Visual Studio
# 	https://visualstudio.microsoft.com/downloads/
# Download Windows SDK 10
#  	https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

# Run from msys
# ./package_win32_sdk.sh

set -e

while IFS='=' read -r key value; do
	case "$key" in
		MSVC_VERSION) MSVC_VERSION="$value" ;;
		SDK_VERSION) SDK_VERSION="$value" ;;
		SDK_ROOT) SDK_ROOT="$value" ;;
		SDK_PATH) SDK_PATH="$value" ;;
		VS_ROOT) VS_ROOT="$value" ;;
		YEAR) YEAR="$value" ;;
	esac
done < <(python - <<'PY'
import os
import sys

sys.path.insert(0, os.path.join(os.getcwd(), 'build_tools'))
import sdk

info = sdk.win_locale_vswhere('x86_64-win32')
if info is None:
    raise SystemExit("Failed to locate Visual Studio using the system vswhere.exe")

sdk_root = info['sdk_root'].rstrip("\\/")
sdk_path = os.path.dirname(sdk_root)
year = os.path.normpath(info['vs_root']).split(os.sep)[-6]

values = {
    'MSVC_VERSION': info['vs_version'],
    'SDK_VERSION': info['sdk_version'],
    'SDK_ROOT': info['sdk_root'],
    'SDK_PATH': sdk_path,
    'VS_ROOT': info['vs_root'],
    'YEAR': year,
}

for key, value in values.items():
    print(f"{key}={value}")
PY
)

echo "Found MSVC_VERSION=${MSVC_VERSION}"
echo "Found VS_ROOT=${VS_ROOT}"
echo "Found YEAR=${YEAR}"
echo "Found SDK_VERSION=${SDK_VERSION}"
echo "Found SDK_ROOT=${SDK_ROOT}"


PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-${YEAR}-${MSVC_VERSION}.tar.gz"
PACKAGES_WIN32_SDK_10="WindowsKits-${SDK_VERSION}.tar.gz"


TARGET_PATH=$(pwd)/local_sdks
TMP_PATH=${TARGET_PATH}/tmp
if [ ! -d "${TMP_PATH}" ]; then
	mkdir -p ${TMP_PATH}
fi


if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_SDK_10}" ]; then
	echo "Packing to ${PACKAGES_WIN32_SDK_10}"
	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} -C "${SDK_PATH}" 10/Include/${SDK_VERSION} 10/Lib/${SDK_VERSION}/um/x86 10/Lib/${SDK_VERSION}/um/x64 10/Lib/${SDK_VERSION}/ucrt/x86 10/Lib/${SDK_VERSION}/ucrt/x64 10/Licenses 10/bin/${SDK_VERSION}/x64 10/bin/${SDK_VERSION}/x86
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_SDK_10} already existed"
fi

if [ ! -e "${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN}" ]; then
	echo "Packing to ${PACKAGES_WIN32_TOOLCHAIN}"
	TMP=${TMP_PATH}/MicrosoftVisualStudio${YEAR}
	TMP_VS_ROOT=${TMP}/VC/Tools/MSVC/${MSVC_VERSION}

	mkdir -p ${TMP_VS_ROOT}/bin/Hostx64
	mkdir -p ${TMP_VS_ROOT}/bin/Hostx86
	mkdir -p ${TMP_VS_ROOT}/include
	mkdir -p ${TMP_VS_ROOT}/lib/x64
	mkdir -p ${TMP_VS_ROOT}/lib/x86
	mkdir -p ${TMP_VS_ROOT}/atlmfc

	cp -r -v "${VS_ROOT}/bin/Hostx64/x64" "${TMP_VS_ROOT}/bin/Hostx64"
	cp -r -v "${VS_ROOT}/bin/Hostx64/x86" "${TMP_VS_ROOT}/bin/Hostx86"
	cp -r -v "${VS_ROOT}/include" "${TMP_VS_ROOT}"
	cp -r -v "${VS_ROOT}/lib/x64" "${TMP_VS_ROOT}/lib"
	cp -r -v "${VS_ROOT}/lib/x86" "${TMP_VS_ROOT}/lib"
	cp -r -v "${VS_ROOT}/atlmfc"  "${TMP_VS_ROOT}"

	GZIP=-9 tar czf ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} -C "$TMP" VC
else
	echo "Package ${TARGET_PATH}/${PACKAGES_WIN32_TOOLCHAIN} already existed"
fi

echo "Done."
