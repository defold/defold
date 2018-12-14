#! /usr/bin/env bash

set -e

DEFOLD_HOME="${DYNAMO_HOME}/../.."
TARGET_DIR="${DEFOLD_HOME}/new_packages"

PLATFORMS="/Applications/Xcode.app/Contents/Developer/Platforms"
XCODE="/Applications/Xcode.app/Contents/Developer/Toolchains"

VERBOSE=

if [ ! -d "$TARGET_DIR" ]; then
	mkdir -p "$TARGET_DIR"
fi

# E.g. make_archive ./iPhoneOS.sdk ./iPhoneOS12.1.sdk
function make_archive() {
	local src=$1
	local tgtname=$(basename $2)
	local srcname=$(basename $1)
	shift
	shift
	local archive=${TARGET_DIR}/${tgtname}.tar.gz
	echo Packaging ${src} to ${archive}

	local tarflags=czf
	if [ "${VERBOSE}" != "" ]; then
		tarflags=${tarflags}v
	fi

	local sed_expr="-s /^${srcname}/${tgtname}/ ${srcname}/"

	echo EXTRA ARGS: $@
	echo tar ${tarflags} ${archive} $@ ${sed_expr} ${src}
	tar ${tarflags} ${archive} $@ ${sed_expr} ${src}
}

function package_platform() {
	local platform=$1
	pushd $PLATFORMS/${platform}.platform/Developer/SDKs/
	PLATFORM_SYMLINK=$(find . -iname "${platform}*" -maxdepth 1 -type l)
	PLATFORM_FOLDER=$(readlink ${PLATFORM_SYMLINK})

	echo FOUND $PLATFORM_SYMLINK "->" $PLATFORM_FOLDER
	make_archive $PLATFORM_FOLDER $PLATFORM_SYMLINK
	popd
}

function package_xcode() {
	local folder=$(find $XCODE -iname "Xcode*" -maxdepth 1 -type d)
	# split XcodeDefault.xctoolchain -> (XcodeDefault, xctoolchain)
	local _name=$(basename $folder)
	local name=${_name%%.*}
	local namesuffix=${_name#*.}
	echo SPLIT  $name  " and " $namesuffix
	local version=$(/usr/bin/xcodebuild -version | grep -e "Xcode" | awk '{print $2}')
	local target=${name}${version}.${namesuffix}

	pushd ${XCODE}
	echo FOUND ${folder} "->" ${target}
	make_archive $(basename ${folder}) ${target} --exclude ${_name}/usr/lib/swift --exclude ${_name}/usr/lib/swift_static --exclude ${_name}/Developer/Platforms
	popd
}

# package_platform "iPhoneOS"
# package_platform "iPhoneSimulator"
# package_platform "MacOSX"

package_xcode "XcodeDefault"

echo "PACKAGES"
ls -la ${TARGET_DIR}/*.gz

# NOTE: You can unpack the tar files and prettify them in one go
# https://unix.stackexchange.com/questions/11018/how-to-choose-directory-name-during-untarring
