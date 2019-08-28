#!/usr/bin/env bash

set -e

ANDROID_NDK_VERSION=r10e
ANDROID_NDK=android-ndk-${ANDROID_NDK_VERSION}
# Yikes!?
ANDROID_PLATFORM=21
ANDROID_TARGET_API_LEVEL=23
ANDROID_BUILD_TOOLS_VERSION=23.0.2

# Do we really need 14 for 32 bit?
PLATFORM=android-${ANDROID_PLATFORM}

HOST=`uname | tr '[:upper:]' '[:lower:]'`

# Contains the entire NDK (we only need the standalone toochain)
ANDROID_NDK_FILENAME=android-ndk-${ANDROID_NDK_VERSION}-${HOST}-x86_64.zip
ANDROID_NDK_URL=https://dl.google.com/android/repository/${ANDROID_NDK_FILENAME}

# Contains the sdkmanager, which is used to install the sdk
ANDROID_TOOLS_FILENAME=sdk-tools-${HOST}-3859397.zip
ANDROID_TOOLS_URL=https://dl.google.com/android/repository/${ANDROID_TOOLS_FILENAME}

TOOL=~/android/${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh

PWD=`pwd`
TMP=${PWD}/_tmpdir

TOOLCHAIN_NAME=android-toolchain-${HOST}-${ANDROID_NDK_VERSION}
TOOLCHAIN_INSTALLDIR=${TMP}/${TOOLCHAIN_NAME}
if [ ! -e "${TOOLCHAIN_INSTALLDIR}" ]; then
	mkdir -p ${TOOLCHAIN_INSTALLDIR}
fi

ANDROID_SDK_NAME=android-sdk-${HOST}-android-${ANDROID_TARGET_API_LEVEL}
ANDROID_SDK_INSTALLDIR=${TMP}/${ANDROID_SDK_NAME}
if [ ! -e "${ANDROID_SDK_INSTALLDIR}" ]; then
	mkdir -p ${ANDROID_SDK_INSTALLDIR}
fi

if [ ! -e "${TMP}" ]; then
	mkdir -p ${TMP}
fi

if [ ! -e "${TMP}/${ANDROID_NDK_FILENAME}" ]; then
	echo "Downloading NDK" ${ANDROID_NDK_URL}
	(cd ${TMP} && wget -q ${ANDROID_NDK_URL})
	(cd ${TMP} && unzip -q ${ANDROID_NDK_FILENAME})
fi

if [ ! -e "${TMP}/${ANDROID_TOOLS_FILENAME}" ]; then
	echo "Downloading sdkmanager" ${ANDROID_TOOLS_URL}
	(cd ${TMP} && wget -q ${ANDROID_TOOLS_URL})
	(cd ${ANDROID_SDK_INSTALLDIR} && unzip -q ${TMP}/${ANDROID_TOOLS_FILENAME})
fi

${TOOL} --arch=arm --platform=${PLATFORM} --toolchain=arm-linux-androideabi-4.8 --install-dir=${TOOLCHAIN_INSTALLDIR}
${TOOL} --arch=arm64 --platform=${PLATFORM} --toolchain=aarch64-linux-android-4.9 --install-dir=${TOOLCHAIN_INSTALLDIR}
(cd ${TMP} && tar -czf ${PWD}/${TOOLCHAIN_NAME}.tar.gz ${TOOLCHAIN_NAME})

echo "CD ${ANDROID_SDK_INSTALLDIR}"
(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose "tools" "platform-tools" )
(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose "extras;android;m2repository" "platforms;android-${ANDROID_TARGET_API_LEVEL}" "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" )
(cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose --uninstall "emulator" )
#(cd ${ANDROID_SDK_INSTALLDIR} && rm -rf ${ANDROID_SDK_INSTALLDIR}/emulator)
# make sure it installed properly!
(cd ${ANDROID_SDK_INSTALLDIR} && ls -la ./build-tools/${ANDROID_BUILD_TOOLS_VERSION}/dx )
(cd ${TMP} && tar -czf ${PWD}/${ANDROID_SDK_NAME}.tar.gz ${ANDROID_SDK_NAME} )

