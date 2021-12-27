#!/usr/bin/env python

"""
The idea is to be able to use locally installed tools if wanted, or rely on prebuilt packages
The order of priority is:
* Packages in tmp/dynamo_home
* Local packages on the host machine
"""

import os
import log
import run
from collections import defaultdict

DYNAMO_HOME=os.environ.get('DYNAMO_HOME', os.path.join(os.getcwd(), 'tmp', 'dynamo_home'))

SDK_ROOT=os.path.join(DYNAMO_HOME, 'ext', 'SDKs')

VERSION_XCODE="13.2.1"
VERSION_MACOSX="12.1"
VERSION_IPHONEOS="15.2"
VERSION_IPHONESIMULATOR="15.2"

#IOS_SDK_VERSION="14.5"
#IOS_SIMULATOR_SDK_VERSION="14.5"
#DARWIN_TOOLCHAIN_ROOT=os.path.join(SDK_ROOT,'XcodeDefault%s.xctoolchain' % XCODE_VERSION)

# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
VERSION_IPHONEOS_MIN="8.0"
VERSION_MACOSX_MIN="10.7"

VERSION_XCODE_CLANG="13.0.0"
SWIFT_VERSION="5.5"

## **********************************************************************************************
## used by build.py

PACKAGES_IOS_SDK="iPhoneOS%s.sdk" % VERSION_IPHONEOS
PACKAGES_IOS_SIMULATOR_SDK="iPhoneSimulator%s.sdk" % VERSION_IPHONESIMULATOR
PACKAGES_MACOS_SDK="MacOSX%s.sdk" % VERSION_MACOSX
PACKAGES_XCODE_TOOLCHAIN="XcodeDefault%s.xctoolchain" % VERSION_XCODE

## **********************************************************************************************

defold_info = defaultdict(defaultdict)
defold_info['xcode']['version'] = VERSION_XCODE
defold_info['xcode']['pattern'] = PACKAGES_XCODE_TOOLCHAIN
defold_info['xcode-clang']['version'] = VERSION_XCODE_CLANG
defold_info['armv7-darwin']['version'] = VERSION_IPHONEOS
defold_info['armv7-darwin']['pattern'] = PACKAGES_IOS_SDK
defold_info['arm64-darwin']['version'] = VERSION_IPHONEOS
defold_info['arm64-darwin']['pattern'] = PACKAGES_IOS_SDK
defold_info['x86_64-ios']['version'] = VERSION_IPHONESIMULATOR
defold_info['x86_64-ios']['pattern'] = PACKAGES_IOS_SIMULATOR_SDK
defold_info['x86_64-darwin']['version'] = VERSION_MACOSX
defold_info['x86_64-darwin']['pattern'] = PACKAGES_MACOS_SDK

## **********************************************************************************************
## DARWIN


def _convert_darwin_platform(platform):
    if platform in ('x86_64-darwin',):
        return 'macosx'
    if platform in ('armv7-darwin','arm64-darwin'):
        return 'iphoneos'
    if platform in ('x86_64-ios',):
        return 'iphonesimulator'
    return 'unknown'

def get_local_darwin_toolchain_path():
    default_path = '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain'
    if os.path.exists(default_path):
        return default_path

    path = run.shell_command('xcrun -f --show-sdk-path') # /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    substr = 'Contents/Developer'
    if substr in path:
        i = path.find(substr)
        path = path[:i + len(substr)] + 'XcodeDefault.xctoolchain'
        return path

    return None

def get_local_darwin_toolchain_version():
    if not os.path.exists('/usr/bin/xcodebuild'):
        return None
    # Xcode 12.5.1
    # Build version 12E507
    xcode_version_full = run.shell_command('/usr/bin/xcodebuild -version')
    xcode_version_lines = xcode_version_full.split("\n")
    xcode_version = xcode_version_lines[0].split()[1].strip()
    return xcode_version

def get_local_darwin_sdk_path(platform):
    return run.shell_command('xcrun -f --sdk %s --show-sdk-path' % _convert_darwin_platform(platform)).strip()

def get_local_darwin_sdk_version(platform):
    return run.shell_command('xcrun -f --sdk %s --show-sdk-platform-version' % _convert_darwin_platform(platform)).strip()


## **********************************************************************************************

# ANDROID_HOME

def _get_defold_path(sdkfolder, platform):
    return os.path.join(sdkfolder, defold_info[platform]['pattern'])

def check_defold_sdk(sdkfolder, platform):
    folders = []

    if platform in ('x86_64-darwin', 'armv7-darwin', 'arm64-darwin', 'x86_64-ios'):
        folders.append(_get_defold_path(sdkfolder, 'xcode'))
        folders.append(_get_defold_path(sdkfolder, platform))

    if platform in ('x86_64-win32', 'win32'):
        folders.append(os.path.join(sdkfolder, 'Win32','WindowsKits','10'))
        folders.append(os.path.join(sdkfolder, 'Win32','MicrosoftVisualStudio14.0','VC'))
    if platform in ('armv7-android', 'arm64-android'):
        folders.append(os.path.join(sdkfolder, PACKAGES_ANDROID_NDK))
        folders.append(os.path.join(sdkfolder, PACKAGES_ANDROID_SDK))

    ok = True
    for f in folders:
        if not os.path.exists(f):
            log("Missing SDK in %s" % f)
            ok = False
    return ok


def check_local_sdk(platform):
    if platform in ('x86_64-darwin', 'armv7-darwin', 'arm64-darwin', 'x86_64-ios'):
        xcode_version = get_local_darwin_toolchain_version()
        if not xcode_version:
            return False
    return True


def _get_defold_sdk_info(sdkfolder, platform):
    info = {}
    if platform in ('x86_64-darwin','x86_64-ios','armv7-darwin','arm64-darwin'):
        info['xcode'] = {}
        info['xcode']['version'] = VERSION_XCODE
        info['xcode']['path'] = _get_defold_path(sdkfolder, 'xcode')
        info[platform] = {}
        info[platform]['version'] = defold_info[platform]['version']
        info[platform]['path'] = _get_defold_path(sdkfolder, 'xcode')

    return info

def _get_local_sdk_info(platform):
    info = {}
    if platform in ('x86_64-darwin','x86_64-ios','armv7-darwin','arm64-darwin'):
        info['xcode'] = {}
        info['xcode']['version'] = get_local_darwin_toolchain_version()
        info['xcode']['path'] = get_local_darwin_toolchain_path()
        info[platform] = {}
        info[platform]['version'] = get_local_darwin_sdk_version(platform)
        info[platform]['path'] = get_local_darwin_sdk_path(platform)

    return info

# It's only cached for the duration of one build
cached_platforms = defaultdict(defaultdict)

def get_sdk_info(sdkfolder, platform):
    if platform in cached_platforms:
        return cached_platforms[platform]

    if check_defold_sdk(sdkfolder, platform):
        result = _get_defold_sdk_info(sdkfolder, platform)
        cached_platforms[platform] = result
        return result

    if check_local_sdk(platform):
        result = _get_local_sdk_info(platform)
        cached_platforms[platform] = result
        return result
    return None

def get_toolchain_root(sdkinfo, platform):
    if platform in ('x86_64-darwin','x86_64-ios','armv7-darwin','arm64-darwin'):
        return sdkinfo['xcode']['path']
    return None










