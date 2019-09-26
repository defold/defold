#!/usr/bin/env bash

# USAGE:
#
#     ./package_android_sdk.sh
#
# * Needs to be run on each host platform
# * Needs Java 8 (or possible 9 or 10 with a hack) due to Google's installer
#

set -e

ANDROID_PLATFORM=29
ANDROID_TARGET_API_LEVEL=29
ANDROID_BUILD_TOOLS_VERSION=23.0.2

PLATFORM=android-${ANDROID_PLATFORM}

HOST=`uname | tr '[:upper:]' '[:lower:]'`

# Contains the sdkmanager, which is used to install the sdk
ANDROID_TOOLS_FILENAME=sdk-tools-${HOST}-4333796.zip
ANDROID_TOOLS_URL=https://dl.google.com/android/repository/${ANDROID_TOOLS_FILENAME}

PWD=`pwd`
TMP=${PWD}/_tmpdir/${HOST}

ANDROID_SDK_NAME=android-sdk-${HOST}-android-${ANDROID_TARGET_API_LEVEL}
ANDROID_SDK_INSTALLDIR=${TMP}/${ANDROID_SDK_NAME}

if [ ! -e "${ANDROID_SDK_INSTALLDIR}" ]; then
	mkdir -p ${ANDROID_SDK_INSTALLDIR}
fi

if [ ! -e "${TMP}" ]; then
	mkdir -p ${TMP}
fi


if [ ! -e "${TMP}/${ANDROID_TOOLS_FILENAME}" ]; then
	echo "Downloading sdkmanager" ${ANDROID_TOOLS_URL}
	(cd ${TMP} && wget -q ${ANDROID_TOOLS_URL})
	(cd ${ANDROID_SDK_INSTALLDIR} && unzip -q ${TMP}/${ANDROID_TOOLS_FILENAME})
else
	echo "Found sdkmanager" ${TMP}/${ANDROID_TOOLS_FILENAME}
fi

echo "cd ${ANDROID_SDK_INSTALLDIR}"

# Since Android SDK only works with JDK8, here's a config hack (for JDK 9 and 10, not 11)
# e.g. export PATH=/usr/lib/jdk/jdk-10.0.2/bin:$PATH
#export JAVA_OPTS="-XX:+IgnoreUnrecognizedVMOptions --add-modules java.se.ee"

if [ ! -e ${ANDROID_SDK_INSTALLDIR}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/dx ]; then
	(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose "tools" "platform-tools" )
	(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose "extras;android;m2repository" "platforms;android-${ANDROID_TARGET_API_LEVEL}" "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" )

	# make sure it installed properly!
	(cd ${ANDROID_SDK_INSTALLDIR} && ls -la ./build-tools/${ANDROID_BUILD_TOOLS_VERSION}/dx )

	echo "Removing folders..."
	(cd ${TMP} && rm -rf ${ANDROID_SDK_NAME}/extras/android/m2repository/com/android/support/test)
else
	echo "Found" ${ANDROID_SDK_INSTALLDIR}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}/dx
	echo "Skipping reinstallation"
fi

OUT_ARCHIVE=${TMP}/${ANDROID_SDK_NAME}.tar.gz
if [ ! -e ${OUT_ARCHIVE} ]; then
	echo "Packing ${ANDROID_SDK_NAME}..."

	(cd ${TMP} && tar -czf ${OUT_ARCHIVE} ${ANDROID_SDK_NAME} )

	echo "Wrote ${OUT_ARCHIVE}"
else
	echo "Found ${OUT_ARCHIVE}"
fi
