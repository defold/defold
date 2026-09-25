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



# Packages the NDK's llvm-strip as bob's "strip_android" tool. Bob uses it to strip the Android
# engine and native extension libraries when bundling with --strip-executable.
#
# llvm-strip reads every Android ABI, so a single tool covers armv7/arm64/x86_64.
#
# USAGE: (linux and windows package from any host, darwin needs macOS for lipo)
# ./package_android_strip.sh linux      # requires patchelf
# ./package_android_strip.sh windows
# ./package_android_strip.sh darwin

set -e

ANDROID_NDK_VERSION=r25b
ANDROID_NDK=android-ndk-${ANDROID_NDK_VERSION}

HOST=$1
if [ "$HOST" == "" ]; then
	HOST=`uname | tr '[:upper:]' '[:lower:]'`
else
	shift
fi

case ${HOST} in
	linux)   PLATFORMS="x86_64-linux" ;;
	windows) PLATFORMS="x86_64-win32" ;;
	darwin)  PLATFORMS="x86_64-macos arm64-macos" ;;
	*) echo "Unsupported host '${HOST}'. Use one of: linux, windows, darwin"; exit 1 ;;
esac

# Only the macOS tool is self contained. The linux one needs the NDK's libc++ and the windows one
# needs libwinpthread, so those are packaged next to the tool and end up in the same libexec folder.
# They are kept under bin/ rather than lib/ on purpose: ext/lib/<platform> is a link path for the
# engine build, and these are runtime companions for one tool rather than libraries to build against.
case ${HOST} in
	linux)   COMPANIONS="lib64/libc++.so.1" ;;
	windows) COMPANIONS="bin/libwinpthread-1.dll" ;;
	darwin)  COMPANIONS="" ;;
esac

if [ "${HOST}" == "linux" ] && ! command -v patchelf > /dev/null; then
	echo "patchelf is required to package the linux tool (its runpath points into the NDK layout)."
	echo "Install it with 'brew install patchelf' or 'apt-get install patchelf'."
	exit 1
fi

ANDROID_NDK_FILENAME=${ANDROID_NDK}-${HOST}.zip
ANDROID_NDK_URL=https://dl.google.com/android/repository/${ANDROID_NDK_FILENAME}

PWD=`pwd`
TARGET_PATH=${PWD}/local_sdks
TMP=${TARGET_PATH}/_tmpdir/$HOST

if [ ! -e "${TMP}" ]; then
	mkdir -p ${TMP}
fi

# The NDK ships llvm-strip as a symlink to llvm-objcopy on some hosts, so both are needed for the
# `cp -L` below to resolve. LLVM dispatches on argv[0] and accepts any name whose stem ends in
# "strip", or is followed by a non alphanumeric character, so the tool still behaves as llvm-strip
# once renamed to strip_android.
NDK_PREBUILT=${ANDROID_NDK}/toolchains/llvm/prebuilt/${HOST}-x86_64
NDK_BIN=${NDK_PREBUILT}/bin

UNPACK="${NDK_BIN}/llvm-strip* ${NDK_BIN}/llvm-objcopy* ${NDK_PREBUILT}/AndroidVersion.txt"
for COMPANION in ${COMPANIONS}; do
	UNPACK="${UNPACK} ${NDK_PREBUILT}/${COMPANION}"
done

if [ ! -e "${TMP}/${NDK_BIN}" ]; then
	if [ ! -e "${TMP}/${ANDROID_NDK_FILENAME}" ]; then
		echo "Downloading NDK" ${ANDROID_NDK_URL}
		(cd ${TMP} && wget ${ANDROID_NDK_URL})
	fi

	echo "Unpacking llvm-strip from" ${ANDROID_NDK_FILENAME}
	(cd ${TMP} && unzip -q -o ${ANDROID_NDK_FILENAME} ${UNPACK})
fi

# Name the package after the LLVM version the NDK is built around. Read it from the NDK rather
# than running the tool, which is a foreign host binary when cross packaging.
LLVM_VERSION=`head -n 1 ${TMP}/${NDK_PREBUILT}/AndroidVersion.txt`
if [ "${LLVM_VERSION}" == "" ]; then
	echo "Unable to determine the LLVM version of ${ANDROID_NDK}"
	exit 1
fi

EXE_SUFFIX=
if [ "${HOST}" == "windows" ]; then
	EXE_SUFFIX=.exe
fi

for PLATFORM in ${PLATFORMS}; do
	PACKAGE=strip_android-${LLVM_VERSION}-${PLATFORM}
	STAGE=${TMP}/_stage/${PLATFORM}
	BIN=${STAGE}/bin/${PLATFORM}

	rm -rf ${STAGE}
	mkdir -p ${BIN}

	# -L so a symlinked llvm-strip is copied as the binary it points at
	cp -L ${TMP}/${NDK_BIN}/llvm-strip${EXE_SUFFIX} ${BIN}/strip_android${EXE_SUFFIX}

	for COMPANION in ${COMPANIONS}; do
		cp -L ${TMP}/${NDK_PREBUILT}/${COMPANION} ${BIN}/
	done

	# The darwin NDK ships a universal binary, keep the packages thin like the tools next to them
	if [ "${HOST}" == "darwin" ]; then
		ARCH=`echo ${PLATFORM} | cut -d'-' -f1`
		lipo -thin ${ARCH} ${BIN}/strip_android -output ${BIN}/strip_android.thin
		mv ${BIN}/strip_android.thin ${BIN}/strip_android
	fi

	# The NDK build looks for libc++ in a sibling lib64 folder, bob keeps everything in one folder
	if [ "${HOST}" == "linux" ]; then
		patchelf --set-rpath '$ORIGIN' ${BIN}/strip_android
	fi

	chmod 755 ${BIN}/strip_android${EXE_SUFFIX}

	echo "Creating archive" ${TARGET_PATH}/${PACKAGE}.tar.gz
	(cd ${STAGE} && tar -czf ${TARGET_PATH}/${PACKAGE}.tar.gz .)
done
