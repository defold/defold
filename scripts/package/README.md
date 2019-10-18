# Packaging the SDK's

The basic principle is to "zip it up" and put it in a cloud storage that the build scripts can access later on.

Some script can be run on a single host, and yet package the sdk for another host.
However, some scripts need to be run on a certain host (for instance various installers)

Usually, the folders can be packaged as-is, but it's generally preferrable to remove redundant folders in order to minimize download and installation sizes.
(E.g. for the extender Docker container)

## Installation

This step is automated given the proper archives are available on the download link.
You can configure the download link via a command line option:

	 ./scripts/build.py --package-path=<url> install_ext --platform=<platform>

or by changing the `CDN_PACKAGES_URL` in the `build.py`.

As a result, all packages should be unpacked under the path `$DYNAMO_HOME/ext/SDKs`:

	./tmp/dynamo_home/ext/SDKs
	├── android-ndk-r20
	└── android-sdk

## Android

### Prerequisites

To package the SDK, you need to run it on Linux/macOS/Windows.
The NDK can be packaged on any host for any host.

The Android `sdkmanager` tool requires Java 8 to run.
Version 9 or 10 _can_ work with some hacks, but 11 won't work at all (see sdk script for particular hack details)

### Packaging

#### NDK

Run the script (on any host):

	$ ./scripts/package/package_android_ndk.sh darwin
	$ ./scripts/package/package_android_ndk.sh linux
	$ ./scripts/package/package_android_ndk.sh windows

and it will output the package in:

	_tmpdir/darwin/android-ndk-r20-darwin-x86_64.tar.gz
	_tmpdir/linux/android-ndk-r20-linux-x86_64.tar.gz
	_tmpdir/windows/android-ndk-r20-windows-x86_64.tar.gz

#### SDK

*Since this step is running an actual installer, it is required to be run on the respective host platform.*

Run the script (hostsystem is any of darwin/linux/windows):

	$ ./scripts/package/package_android_sdk.sh <hostsystem>

and it will output a package (depending on hostsystem):

	_tmpdir/darwin/android-sdk-darwin-android-29.tar.gz
	_tmpdir/linux/android-sdk-linux-android-29.tar.gz
	_tmpdir/windows/android-sdk-windows-android-29.tar.gz

#### Tools

Download and unpack this this:

	$ wget https://dl.google.com/android/repository/sdk-tools-linux-3859397.zip
	$ unzip sdk-tools-linux-3859397.zip

And repackage into a tar file:

	$ tar -czf android-sdk-tools-linux-3859397.tag.gz tools


## iOS + macOS

### Prerequisites

This script cannot download the sdk's by itself, but instead relies on the user having downloaded the latest XCode on the local machine

### Packaging

Run the script

	./scripts/package/package_xcode_and_sdks.sh

and it will output files in `new_packages`:

	./new_packages/MacOSX10.14.sdk.tar.gz
	./new_packages/XcodeDefault10.2.1.xctoolchain.tar.gz
	./new_packages/iPhoneOS12.2.sdk.tar.gz
	./new_packages/iPhoneSimulator12.2.sdk.tar.gz

## Windows

...

## HTML5

The installation of the HTML5 compiler is done by calling the build function `install_ems`:

	$ ./scripts/build.py install_ems

## Linux

Since it's difficult to package the C++ build pipeline for Linux, we simply rely on installing the correct compiler versions on each Linux system
