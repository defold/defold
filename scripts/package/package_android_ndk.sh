#!/usr/bin/env bash

# USAGE: (works from same host machine)
# ./package_android_ndk.sh linux
# ./package_android_ndk.sh darwin
# ./package_android_ndk.sh windows

set -e

ANDROID_NDK_VERSION=r20
ANDROID_NDK=android-ndk-${ANDROID_NDK_VERSION}

HOST=$1
shift

if [ "$HOST" == "" ]; then
	HOST=`uname | tr '[:upper:]' '[:lower:]'`
fi

# Contains the entire NDK (we only need the standalone toochain)
ANDROID_NDK_BASENAME=android-ndk-${ANDROID_NDK_VERSION}-${HOST}-x86_64
ANDROID_NDK_FILENAME=${ANDROID_NDK_BASENAME}.zip
ANDROID_NDK_URL=https://dl.google.com/android/repository/${ANDROID_NDK_FILENAME}

# Contains the sdkmanager, which is used to install the sdk
ANDROID_TOOLS_FILENAME=sdk-tools-${HOST}-4333796.zip
ANDROID_TOOLS_URL=https://dl.google.com/android/repository/${ANDROID_TOOLS_FILENAME}

PWD=`pwd`
TMP=${PWD}/_tmpdir/$HOST

if [ ! -e "${TMP}" ]; then
	mkdir -p ${TMP}
fi

if [ ! -e "${TMP}/${ANDROID_NDK_FILENAME}" ]; then
	echo "Downloading NDK" ${ANDROID_NDK_URL}
	(cd ${TMP} && wget ${ANDROID_NDK_URL})
fi

if [ ! -e "${TMP}/${ANDROID_NDK}" ]; then
	echo "Unpacking NDK" ${ANDROID_NDK_FILENAME}
	(cd ${TMP} && unzip -q ${ANDROID_NDK_FILENAME})
fi

if [ ! -e "${TMP}/${ANDROID_NDK_BASENAME}.tar.gz" ]; then
	# Cleanup the package, shrinking 2.9GB down to 1.6GB (extracted)
	echo "Cleaning NDK" ${TMP}/${ANDROID_NDK}

	(cd ${TMP} && rm -rf ${ANDROID_NDK}/prebuilt)
	# keep: (cd ${TMP} && rm -rf ${ANDROID_NDK}/sources) // android_native_app_glue.h

	# keep: (cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-16)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-17)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-18)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-19)
	# there is no no 20
	# keep: (cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-21)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-22)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-23)

	# We keep the 24 version for support of vulkan
	#(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-24)

	# there is no 25
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-26)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-27)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-28)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-29)

	# keep
	#(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.9)
	#(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/aarch64-linux-android-4.9)
	#(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/renderscript)

	(cd ${TMP} && rm -rf ${ANDROID_NDK}/sysroot/usr/lib/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/sysroot/usr/lib/x86_64-linux-android)

	# (cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86-4.9)
	# (cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86_64-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/x86_64-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/x86_64-linux-android)

	echo "Creating NDK archive" ${TMP}/${ANDROID_NDK_BASENAME}.tar.gz
	(cd ${TMP} && tar -czf ${ANDROID_NDK_BASENAME}.tar.gz ${ANDROID_NDK})
else
	echo "Found ${TMP}/${ANDROID_NDK_BASENAME}.tar.gz"
fi
