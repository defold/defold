#!/usr/bin/env bash


IOS_GCC=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++
IOS_AR=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar
IOS_SDK=/Users/mathiaswesterdahl/work/defold/tmp/dynamo_home/ext/SDKs/iPhoneOS10.3.sdk

function RemoveTarget {
	local name=$1
	if [ -f $name ]; then
		rm $name
		echo Removed $name
	fi
}

function CompileiOS {
	local name=$1
	local src=$2
	local targetdir=$3
	local arch=$4

	#archs=("armv7" "arm64")
	#for arch in "${archs[@]}"
	#do
		local archname=$arch-ios
		local target=$targetdir/$archname/lib$name.dylib
		
		RemoveTarget $target
		mkdir -p $(dirname $target)

		$IOS_GCC -dynamiclib -arch $arch -fPIC -isysroot $IOS_SDK -O2 -miphoneos-version-min=6.0 -fomit-frame-pointer -fno-strict-aliasing -fno-exceptions $src -o $target

		echo Wrote $target
	#done
}

set -e
pushd ../../

CompileiOS testlib lib/arm64-ios/library.cpp lib arm64

popd