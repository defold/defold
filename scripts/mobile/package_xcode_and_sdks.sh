#! /usr/bin/env bash

# Note: I wasn't able to rename the top folder when packaging, since it messed up symlinks (also the packages became unnecessarily bloated)

# You can unpack the tar files and prettify them in one go
# $ mkdir iPhoneOS12.1.sdk && tar xf ../new_packages/iPhoneOS12.1.sdk.tar.gz -C iPhoneOS12.1.sdk --strip-components 1

# Original command lines:
# $ (cd /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs && tar zcf ~/work/defold/footest/iPhoneOS12.1.sdk.tar.gz iPhoneOS.sdk)
# $ (cd /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs && tar zcf ~/work/defold/footest/iPhoneOS12.1.sdk.tar.gz iPhoneOS.sdk)

set -e

DEFOLD_HOME="${DYNAMO_HOME}/../.."
TARGET_DIR="${DEFOLD_HOME}/new_packages"

PLATFORMS="/Applications/Xcode.app/Contents/Developer/Platforms"
XCODE="/Applications/Xcode.app/Contents/Developer/Toolchains"

VERBOSE=

if [ ! -d "$TARGET_DIR" ]; then
	mkdir -p "$TARGET_DIR"
fi

# E.g. make_archive ./iPhoneOS.sdk ./iPhoneOS12.1.sdk -> ${TARGET_DIR}/iPhoneOS12.1.sdk.tar.gz
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

	echo EXTRA ARGS: $@
	echo tar ${tarflags} ${archive} $@ ${src}
	tar ${tarflags} ${archive} $@ ${src}
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

	echo FOUND ${XCODE}/${_name} "->" ${target}

	pushd ${XCODE}

	EXTRA_ARGS="--exclude ${_name}/usr/lib/swift --exclude ${_name}/usr/lib/swift_static --exclude ${_name}/Developer/Platforms --exclude ${_name}/usr/lib/sourcekitd.framework"
	for f in ${_name}/usr/bin/swift*
	do
		EXTRA_ARGS="--exclude ${f} ${EXTRA_ARGS}"
	done

	echo MAKING ARCHIVE ${_name} ${target}
	make_archive ${_name} ${target} ${EXTRA_ARGS}
	popd
}

package_platform "iPhoneOS"
package_platform "iPhoneSimulator"
package_platform "MacOSX"

package_xcode "XcodeDefault"

echo "PACKAGES"
ls -la ${TARGET_DIR}/*.gz

