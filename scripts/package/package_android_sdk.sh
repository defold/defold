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



# USAGE:
#
#     ./package_android_sdk.sh
#
# * Needs to be run on each host platform
# * Needs Java 8 (or possible 9 or 10 with a hack) due to Google's installer
#

set -e

ANDROID_PLATFORM=35
ANDROID_TARGET_API_LEVEL=35
ANDROID_BUILD_TOOLS_VERSION=35.0.1

PLATFORM=android-${ANDROID_PLATFORM}

HOST=`uname | tr '[:upper:]' '[:lower:]'`
SDKMANAGER="sdkmanager"
D8TOOL="d8"

TOOLHOSTNAME=$HOST
if [ "$HOST" == "darwin" ]; then
	TOOLHOSTNAME="mac"
fi

if [ "$TERM" == "cygwin" ]; then
	HOST="win"
	TOOLHOSTNAME="win"
	SDKMANAGER="sdkmanager.bat"
	D8TOOL="d8.bat"
fi

if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* ]]; then
	HOST="win"
	TOOLHOSTNAME="win"
	SDKMANAGER="sdkmanager.bat"
	D8TOOL="d8.bat"
fi

echo TOOLHOSTNAME $TOOLHOSTNAME

# Contains the sdkmanager, which is used to install the sdk
ANDROID_TOOLS_FILENAME=commandlinetools-${TOOLHOSTNAME}-6200805_latest.zip
ANDROID_TOOLS_URL=https://dl.google.com/android/repository/${ANDROID_TOOLS_FILENAME}

PWD=`pwd`
TARGET_PATH=${PWD}/local_sdks
TMP=${TARGET_PATH}/_tmpdir/${HOST}

ANDROID_SDK_NAME=android-sdk-${HOST}-android-${ANDROID_TARGET_API_LEVEL}-${ANDROID_BUILD_TOOLS_VERSION}
ANDROID_SDK_INSTALLDIR=${TMP}/${ANDROID_SDK_NAME}

TMP_ANDROID_HOME=${TMP}/android_home/${ANDROID_SDK_NAME}


if [ ! -e "${TMP}" ]; then
	mkdir -p ${TMP}
fi


if [ ! -e "${TMP}/${ANDROID_TOOLS_FILENAME}" ]; then
	echo "Downloading sdkmanager" ${ANDROID_TOOLS_URL}
	(cd ${TMP} && wget -q ${ANDROID_TOOLS_URL})
else
	echo "Found downloaded package" ${TMP}/${ANDROID_TOOLS_FILENAME}
fi

if [ ! -d "${ANDROID_SDK_INSTALLDIR}" ]; then
	echo "Unpacking sdkmanager" ${ANDROID_TOOLS_URL}
	mkdir -p ${ANDROID_SDK_INSTALLDIR}
	(cd ${ANDROID_SDK_INSTALLDIR} && unzip -q ${TMP}/${ANDROID_TOOLS_FILENAME})
else
	echo "Found unpacked sdkmanager" ${ANDROID_SDK_INSTALLDIR}
fi

echo "cd ${ANDROID_SDK_INSTALLDIR}"

# Doesn't work with JDK for arm64 on mac

# Since Android SDK only works with JDK8, here's a config hack (for JDK 9 and 10, not 11)
# e.g. export PATH=/usr/lib/jdk/jdk-10.0.2/bin:$PATH
#export JAVA_OPTS="-XX:+IgnoreUnrecognizedVMOptions --add-modules java.se.ee"

if [ ! -e ${ANDROID_SDK_INSTALLDIR}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/${D8TOOL} ]; then
	mkdir -p ${TMP_ANDROID_HOME}

	(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/${SDKMANAGER} --verbose --sdk_root=${TMP_ANDROID_HOME} "tools" "platform-tools" )
	(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/${SDKMANAGER} --verbose --sdk_root=${TMP_ANDROID_HOME} "extras;android;m2repository" "platforms;android-${ANDROID_TARGET_API_LEVEL}" "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" )

	# make sure it installed properly!
	ls -la ${TMP_ANDROID_HOME}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/${D8TOOL}

	echo "Removing folders..."
	rm -rf ${TMP_ANDROID_HOME}/extras/android/m2repository/com/android/support/test
	rm -rf ${TMP_ANDROID_HOME}/emulator
else
	echo "Found" ${TMP_ANDROID_HOME}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/${D8TOOL}
	echo "Skipping reinstallation"
fi

OUT_ARCHIVE=${TARGET_PATH}/${ANDROID_SDK_NAME}.tar.gz
if [ ! -e ${OUT_ARCHIVE} ]; then
	echo "Packing ${OUT_ARCHIVE}..."

	(cd ${TMP_ANDROID_HOME}/.. && GZIP=-9 tar -czf ${OUT_ARCHIVE} ${ANDROID_SDK_NAME})

	echo "Wrote ${OUT_ARCHIVE}"
else
	echo "Found ${OUT_ARCHIVE}"
fi
