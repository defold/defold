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



# USAGE:
# ./package_android_ndk_r10e.sh

set -e

ANDROID_NDK_VERSION=r10e
ANDROID_NDK=android-ndk-${ANDROID_NDK_VERSION}

HOST=$1
shift

if [ "$HOST" == "" ]; then
	HOST=`uname | tr '[:upper:]' '[:lower:]'`
fi

ANDROID_NDK_BASENAME=android-ndk-${ANDROID_NDK_VERSION}-${HOST}-x86_64
ANDROID_NDK_FILENAME=${ANDROID_NDK_BASENAME}.zip
ANDROID_NDK_URL=https://dl.google.com/android/repository/${ANDROID_NDK_FILENAME}

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

	(cd ${TMP} && rm -rf ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++abi)
	#(cd ${TMP} && rm -rf ${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.9)

	(cd ${TMP} && rm -rf ${ANDROID_NDK}/samples)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-3)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-4)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-5)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-8)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-12)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-13)
	# keep: (cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-14)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/platforms/android-15)
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

	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm-3.5)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm-3.6)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86-4.8)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/x86_64-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/mipsel-linux-android-4.8)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/mips64el-linux-android-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/mipsel-linux-android-4.9)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/x86_64-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/i686-linux-android)
	(cd ${TMP} && rm -rf ${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/x86_64-linux-android)

	echo "Creating NDK archive" ${TMP}/${ANDROID_NDK_BASENAME}.tar.gz
	(cd ${TMP} && tar -czf ${ANDROID_NDK_BASENAME}.tar.gz ${ANDROID_NDK})
else
	echo "Found "${TMP}/${ANDROID_NDK_BASENAME}.tar.gz""
fi
