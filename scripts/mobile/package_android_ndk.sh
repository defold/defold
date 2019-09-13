#!/usr/bin/env bash

set -e

ANDROID_NDK_VERSION=r20
ANDROID_NDK=android-ndk-${ANDROID_NDK_VERSION}

ANDROID_PLATFORM=29
ANDROID_TARGET_API_LEVEL=29
ANDROID_BUILD_TOOLS_VERSION=23.0.2

PLATFORM=android-${ANDROID_PLATFORM}

HOST=`uname | tr '[:upper:]' '[:lower:]'`

# Contains the entire NDK (we only need the standalone toochain)
ANDROID_NDK_BASENAME=android-ndk-${ANDROID_NDK_VERSION}-${HOST}-x86_64
ANDROID_NDK_FILENAME=${ANDROID_NDK_BASENAME}.zip
ANDROID_NDK_URL=https://dl.google.com/android/repository/${ANDROID_NDK_FILENAME}

# Contains the sdkmanager, which is used to install the sdk
ANDROID_TOOLS_FILENAME=sdk-tools-${HOST}-4333796.zip
ANDROID_TOOLS_URL=https://dl.google.com/android/repository/${ANDROID_TOOLS_FILENAME}

PWD=`pwd`
TMP=${PWD}/_tmpdir

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
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/sources)

	# keep: (cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-16)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-17)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-18)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-19)
	# there is no no 20
	# keep: (cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-21)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-22)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-23)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-24)
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

	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86_64-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/x86_64-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/x86_64-linux-android)

	echo "Creating NDK archive" ${TMP}/${ANDROID_NDK_BASENAME}.tar.gz
	(cd ${TMP} && tar -czf ${ANDROID_NDK_BASENAME}.tar.gz ${ANDROID_NDK})
fi


# if [ ! -e "${TMP}/${ANDROID_TOOLS_FILENAME}" ]; then
# 	echo "Downloading sdkmanager" ${ANDROID_TOOLS_URL}
# 	(cd ${TMP} && wget -q ${ANDROID_TOOLS_URL})
# 	(cd ${ANDROID_SDK_INSTALLDIR} && unzip -q ${TMP}/${ANDROID_TOOLS_FILENAME})
# fi


# echo "cd ${ANDROID_SDK_INSTALLDIR}"

# # Since Android SDK only works with JDK8, here's a config hack (for JDK 9 and 10, not 11)
# # e.g. export PATH=/usr/lib/jdk/jdk-10.0.2/bin:$PATH
# #export JAVA_OPTS="-XX:+IgnoreUnrecognizedVMOptions --add-modules java.se.ee"

# (cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose "tools" "platform-tools" )
# (cd ${ANDROID_SDK_INSTALLDIR} && echo y | ./tools/bin/sdkmanager --verbose "extras;android;m2repository" "platforms;android-${ANDROID_TARGET_API_LEVEL}" "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" )

# # make sure it installed properly!
# (cd ${ANDROID_SDK_INSTALLDIR} && ls -la ./build-tools/${ANDROID_BUILD_TOOLS_VERSION}/dx )

# echo "Removing folders..."
# (cd ${TMP} && rm -rf ${ANDROID_SDK_NAME}/extras/android/m2repository/com/android/support/test)

# echo "Packing ${ANDROID_SDK_NAME}..."

# (cd ${TMP} && tar -czf ${PWD}/${ANDROID_SDK_NAME}.tar.gz ${ANDROID_SDK_NAME} )

# echo "Wrote ${PWD}/${ANDROID_SDK_NAME}.tar.gz"
