#!/usr/bin/env python
# Copyright 2020-2024 The Defold Foundation
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



"""
The idea is to be able to use locally installed tools if wanted, or rely on prebuilt packages
The order of priority is:
* Packages in tmp/dynamo_home
* Local packages on the host machine
"""

import os
import sys
import log
import run
import platform
from collections import defaultdict

DYNAMO_HOME=os.environ.get('DYNAMO_HOME', os.path.join(os.getcwd(), 'tmp', 'dynamo_home'))

SDK_ROOT=os.path.join(DYNAMO_HOME, 'ext', 'SDKs')

## **********************************************************************************************
# Darwin

# A list of minimum versions here: https://developer.apple.com/support/xcode/

VERSION_XCODE="15.1" # we also use this to match version on Github Actions
VERSION_XCODE_CLANG="15.0.0"
VERSION_MACOSX="14.2"
VERSION_IPHONEOS="17.2"
VERSION_IPHONESIMULATOR="17.2"
MACOS_ASAN_PATH="usr/lib/clang/%s/lib/darwin/libclang_rt.asan_osx_dynamic.dylib"

# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
VERSION_IPHONEOS_MIN="11.0"
VERSION_MACOSX_MIN="10.13"

SWIFT_VERSION="5.5"

VERSION_LINUX_CLANG="13.0.0"
PACKAGES_LINUX_CLANG="clang-%s" % VERSION_LINUX_CLANG
PACKAGES_LINUX_TOOLCHAIN="clang+llvm-%s-x86_64-linux-gnu-ubuntu-16.04" % VERSION_LINUX_CLANG

## **********************************************************************************************
# Android

ANDROID_NDK_VERSION='25b'
ANDROID_BUILD_TOOLS_VERSION = '33.0.1'
ANDROID_NDK_API_VERSION='19' # Android 4.4
ANDROID_64_NDK_API_VERSION='21' # Android 5.0

## **********************************************************************************************
# Win32

# The version we have prepackaged
VERSION_WINDOWS_SDK_10="10.0.20348.0"
VERSION_WINDOWS_MSVC_2022="14.37.32822"
PACKAGES_WIN32_TOOLCHAIN=f"Microsoft-Visual-Studio-2022-{VERSION_WINDOWS_MSVC_2022}"
PACKAGES_WIN32_SDK_10=f"WindowsKits-{VERSION_WINDOWS_SDK_10}"

## **********************************************************************************************
## used by build.py

PACKAGES_IOS_SDK="iPhoneOS%s.sdk" % VERSION_IPHONEOS
PACKAGES_IOS_SIMULATOR_SDK="iPhoneSimulator%s.sdk" % VERSION_IPHONESIMULATOR
PACKAGES_MACOS_SDK="MacOSX%s.sdk" % VERSION_MACOSX
PACKAGES_XCODE_TOOLCHAIN="XcodeDefault%s.xctoolchain" % VERSION_XCODE

## **********************************************************************************************

# The "pattern" is the path relative to the tmp/dynamo/ext/SDKs/ folder

defold_info = defaultdict(defaultdict)
defold_info['xcode']['version'] = VERSION_XCODE
defold_info['xcode']['pattern'] = PACKAGES_XCODE_TOOLCHAIN
defold_info['xcode-clang']['version'] = VERSION_XCODE_CLANG
defold_info['arm64-ios']['version'] = VERSION_IPHONEOS
defold_info['arm64-ios']['pattern'] = PACKAGES_IOS_SDK
defold_info['x86_64-ios']['version'] = VERSION_IPHONESIMULATOR
defold_info['x86_64-ios']['pattern'] = PACKAGES_IOS_SIMULATOR_SDK
defold_info['x86_64-macos']['version'] = VERSION_MACOSX
defold_info['x86_64-macos']['pattern'] = PACKAGES_MACOS_SDK
defold_info['arm64-macos']['version'] = VERSION_MACOSX
defold_info['arm64-macos']['pattern'] = PACKAGES_MACOS_SDK

defold_info['x86_64-win32']['version'] = VERSION_WINDOWS_SDK_10
defold_info['x86_64-win32']['pattern'] = "Win32/%s" % PACKAGES_WIN32_TOOLCHAIN
defold_info['win32']['version'] = defold_info['x86_64-win32']['version']
defold_info['win32']['pattern'] = defold_info['x86_64-win32']['pattern']

defold_info['win10sdk']['version'] = VERSION_WINDOWS_SDK_10
defold_info['win10sdk']['pattern'] = "Win32/%s" % PACKAGES_WIN32_SDK_10

defold_info['x86_64-linux']['version'] = VERSION_LINUX_CLANG
defold_info['x86_64-linux']['pattern'] = 'linux/clang-%s' % VERSION_LINUX_CLANG

## **********************************************************************************************
## DARWIN


def _convert_darwin_platform(platform):
    if platform in ('x86_64-macos','arm64-macos'):
        return 'macosx'
    if platform in ('arm64-ios',):
        return 'iphoneos'
    if platform in ('x86_64-ios',):
        return 'iphonesimulator'
    return 'unknown'

def _get_xcode_local_path():
    return run.shell_command('xcode-select -print-path')

# "xcode-select -print-path" will give you "/Applications/Xcode.app/Contents/Developer"
def get_local_darwin_toolchain_path():
    default_path = '%s/Toolchains/XcodeDefault.xctoolchain' % _get_xcode_local_path()
    if os.path.exists(default_path):
        return default_path
    return '/Library/Developer/CommandLineTools'

def get_local_darwin_toolchain_version():
    if not os.path.exists('/usr/bin/xcodebuild'):
        return VERSION_XCODE
    # Xcode 14.2
    # Build version 14C18
    xcode_version_full = run.shell_command('/usr/bin/xcodebuild -version')
    xcode_version_lines = xcode_version_full.split("\n")
    xcode_version = xcode_version_lines[0].split()[1].strip()
    return xcode_version

def get_local_darwin_clang_version():
    # Apple clang version 14.0.0 (clang-1400.0.29.202)
    # Target: x86_64-apple-darwin22.3.0
    version_full = run.shell_command('clang --version')
    version_lines = version_full.split("\n")
    version = version_lines[0].split()[3].strip()
    return version

def get_local_darwin_sdk_path(platform):
    return run.shell_command('xcrun -f --sdk %s --show-sdk-path' % _convert_darwin_platform(platform)).strip()

def get_local_darwin_sdk_version(platform):
    return run.shell_command('xcrun -f --sdk %s --show-sdk-platform-version' % _convert_darwin_platform(platform)).strip()


## **********************************************************************************************
## Android


def get_android_local_sdk_path():
    path = os.environ.get('ANDROID_HOME', None)

    if path is None:
        if sys.platform == 'darwin':
            path = os.path.expanduser('~/Library/android/sdk')

    if path and os.path.exists(path):
        return path

    return None

def _get_latest_version_from_folders(path):
    dirs = [ x for x in os.listdir(path)]
    dirs.sort(key=lambda x: tuple(int(token) for token in x.split('.')))
    return dirs[0]

def get_android_local_ndk_path(platform):
    sdk_root = get_android_local_sdk_path()
    ndk_root = os.path.join(sdk_root, 'ndk')
    version = _get_latest_version_from_folders(ndk_root)
    return os.path.join(ndk_root, version)

def get_android_local_build_tools_path(platform):
    path = get_android_local_sdk_path()
    build_tools_path = os.path.join(path, 'build-tools')
    version = _get_latest_version_from_folders(build_tools_path)
    return os.path.join(build_tools_path, version)

def get_android_local_sdk_version(platform):
    return os.path.basename(get_android_local_build_tools_path(platform))

## **********************************************************************************************

# ANDROID_HOME


## **********************************************************************************************

# Linux

_is_wsl = None

def is_wsl():
    global _is_wsl
    if _is_wsl is not None:
        return _is_wsl

    """ Checks if we're running on native Linux on in WSL """
    _is_wsl = False
    if 'Linux' == platform.system():
        with open("/proc/version") as f:
            data = f.read()
            _is_wsl = "Microsoft" in data
    return _is_wsl

def get_local_compiler_from_bash():
    path = run.shell_command('which clang++')
    if path != None:
        return "clang++"
    path = run.shell_command('which g++')
    if path != None:
        return "g++"
    return None

def get_local_compiler_path():
    tool = get_local_compiler_from_bash()
    if tool is None:
        return None

    path = run.shell_command('which %s' % tool)
    substr = '/bin'
    if substr in path:
        i = path.find(substr)
        path = path[:i]
        return path
    return None

def get_local_compiler_version():
    tool = get_local_compiler_from_bash()
    if tool is None:
        return None
    return run.shell_command('%s -dumpversion' % tool).strip()


## **********************************************************************************************

# Windows

windows_info = None

def get_windows_local_sdk_info(platform):
    global windows_info

    if windows_info is not None:
        return windows_info

    if sys.platform != 'win32':
        # we cannot currently use vswhere.exe on this platform
        # todo: check using `wine`
        return None

    vswhere_path = '%s/../../scripts/windows/vswhere2/vswhere2.exe' % os.environ.get('DYNAMO_HOME', '.')
    if not os.path.exists(vswhere_path):
        vswhere_path = './scripts/windows/vswhere2/vswhere2.exe'
        vswhere_path = os.path.normpath(vswhere_path)
        if not os.path.exists(vswhere_path):
            print ("Couldn't find executable '%s'" % vswhere_path)
            return None

    sdk_root = run.shell_command('%s --sdk_root' % vswhere_path).strip()
    sdk_version = run.shell_command('%s --sdk_version' % vswhere_path).strip()
    includes = run.shell_command('%s --includes' % vswhere_path).strip()
    lib_paths = run.shell_command('%s --lib_paths' % vswhere_path).strip()
    bin_paths = run.shell_command('%s --bin_paths' % vswhere_path).strip()
    vs_root = run.shell_command('%s --vs_root' % vswhere_path).strip()
    vs_version = run.shell_command('%s --vs_version' % vswhere_path).strip()

    if platform == 'win32':
        arch64 = 'x64'
        arch32 = 'x86'
        bin_paths = bin_paths.replace(arch64, arch32)
        lib_paths = lib_paths.replace(arch64, arch32)

    info = {}
    info['sdk_root'] = sdk_root
    info['sdk_version'] = sdk_version
    info['includes'] = includes
    info['lib_paths'] = lib_paths
    info['bin_paths'] = bin_paths
    info['vs_root'] = vs_root
    info['vs_version'] = vs_version
    windows_info = info
    return windows_info

def get_windows_packaged_sdk_info(sdkdir, platform):
    global windows_info
    if windows_info is not None:
        return windows_info

    # We return these mappings in a format that the waf tools would have returned (if they worked, and weren't very very slow)
    msvcdir = os.path.join(sdkdir, 'Win32', 'MicrosoftVisualStudio14.0')
    windowskitsdir = os.path.join(sdkdir, 'Win32', 'WindowsKits')

    arch = 'x64'
    if platform == 'win32':
        arch = 'x86'

    # Since the programs(Windows!) can update, we do this dynamically to find the correct version
    ucrt_dirs = [ x for x in os.listdir(os.path.join(windowskitsdir,'10','Include'))]
    ucrt_dirs = [ x for x in ucrt_dirs if x.startswith('10.0')]
    ucrt_dirs.sort(key=lambda x: int((x.split('.'))[2]))
    ucrt_version = ucrt_dirs[-1]
    if not ucrt_version.startswith('10.0'):
        conf.fatal("Unable to determine ucrt version: '%s'" % ucrt_version)

    msvc_version = [x for x in os.listdir(os.path.join(msvcdir,'VC','Tools','MSVC'))]
    msvc_version = [x for x in msvc_version if x.startswith('14.')]
    msvc_version.sort(key=lambda x: map(int, x.split('.')))
    msvc_version = msvc_version[-1]
    if not msvc_version.startswith('14.'):
        conf.fatal("Unable to determine msvc version: '%s'" % msvc_version)

    msvc_path = (os.path.join(msvcdir,'VC', 'Tools', 'MSVC', msvc_version, 'bin', 'Host'+arch, arch),
                os.path.join(windowskitsdir,'10','bin',ucrt_version,arch))

    includes = [os.path.join(msvcdir,'VC','Tools','MSVC',msvc_version,'include'),
                os.path.join(msvcdir,'VC','Tools','MSVC',msvc_version,'atlmfc','include'),
                os.path.join(windowskitsdir,'10','Include',ucrt_version,'ucrt'),
                os.path.join(windowskitsdir,'10','Include',ucrt_version,'winrt'),
                os.path.join(windowskitsdir,'10','Include',ucrt_version,'um'),
                os.path.join(windowskitsdir,'10','Include',ucrt_version,'shared')]

    libdirs = [ os.path.join(msvcdir,'VC','Tools','MSVC',msvc_version,'lib',arch),
                os.path.join(msvcdir,'VC','Tools','MSVC',msvc_version,'atlmfc','lib',arch),
                os.path.join(windowskitsdir,'10','Lib',ucrt_version,'ucrt',arch),
                os.path.join(windowskitsdir,'10','Lib',ucrt_version,'um',arch)]

    info = {}
    info['sdk_root'] = os.path.join(windowskitsdir,'10')
    info['sdk_version'] = ucrt_version
    info['includes'] = ','.join(includes)
    info['lib_paths'] = ','.join(libdirs)
    info['bin_paths'] = ','.join(msvc_path)
    info['vs_root'] = msvcdir
    info['vs_version'] = msvc_version
    windows_info = info
    return windows_info

def _setup_info_from_windowsinfo(windowsinfo, platform):

    info = {}
    info[platform] = {}
    info[platform]['version'] = windowsinfo['sdk_version']
    info[platform]['path'] = windowsinfo['sdk_root']

    info['msvc'] = {}
    info['msvc']['version'] = windowsinfo['vs_version']
    info['msvc']['path'] = windowsinfo['vs_root']

    info['bin_paths'] = {}
    info['bin_paths']['version'] = info[platform]['version']
    info['bin_paths']['path'] = windowsinfo['bin_paths'].split(',')

    info['lib_paths'] = {}
    info['lib_paths']['version'] = info[platform]['version']
    info['lib_paths']['path'] = windowsinfo['lib_paths'].split(',')

    info['includes'] = {}
    info['includes']['version'] = info[platform]['version']
    info['includes']['path'] = windowsinfo['includes'].split(',')

    return info


## **********************************************************************************************

def _get_defold_path(sdkfolder, platform):
    return os.path.join(sdkfolder, defold_info[platform]['pattern'])

def check_defold_sdk(sdkfolder, platform, verbose=False):
    folders = []
    if verbose:
        log.log("check_defold_sdk: %s %s" % (sdkfolder, platform))

    if platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        folders.append(_get_defold_path(sdkfolder, 'xcode'))
        folders.append(_get_defold_path(sdkfolder, platform))

    elif platform in ('x86_64-win32', 'win32'):
        folders.append(os.path.join(sdkfolder, 'Win32','WindowsKits','10'))
        folders.append(os.path.join(sdkfolder, 'Win32','MicrosoftVisualStudio14.0','VC'))

    elif platform in ('armv7-android', 'arm64-android'):
        folders.append(os.path.join(sdkfolder, "android-ndk-r%s" % ANDROID_NDK_VERSION))
        folders.append(os.path.join(sdkfolder, "android-sdk"))

    elif platform in ('x86_64-linux',):
        folders.append(os.path.join(sdkfolder, "linux"))

    if not folders:
        log.log("sdk.py: No SDK folders specified for %s" %platform)
        return False

    count = 0
    for f in folders:
        if not os.path.exists(f):
            if verbose:
                log.log("  Missing SDK in %s" % f)
        else:
            count = count + 1
    result = count == len(folders)

    if verbose:
        if not result:
            log.log("  No prepackaged sdk found.")
        else:
            log.log("  Found prepackaged sdk folders:")
            for f in folders:
                log.log("    %s" % f)

    return result

def check_local_sdk(platform, verbose=False):
    if platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        xcode_version = get_local_darwin_toolchain_version()
        if not xcode_version:
            return False
    elif platform in ('win32', 'x86_64-win32'):
        info = get_windows_local_sdk_info(platform)
        return info is not None

    elif platform in ('armv7-android', 'arm64-android'):
        path = get_android_local_sdk_path()
        return path is not None

    return True


def _get_defold_sdk_info(sdkfolder, platform):
    info = {}
    if platform in ('x86_64-macos', 'arm64-macos','x86_64-ios','arm64-ios'):
        info['xcode'] = {}
        info['xcode']['version'] = VERSION_XCODE
        info['xcode']['path'] = _get_defold_path(sdkfolder, 'xcode')
        info['xcode-clang'] = defold_info['xcode-clang']['version']
        info['asan'] = {}
        info['asan']['path'] = os.path.join(info['xcode']['path'], MACOS_ASAN_PATH%info['xcode-clang'])
        info[platform] = {}
        info[platform]['version'] = defold_info[platform]['version']
        info[platform]['path'] = _get_defold_path(sdkfolder, platform) # what we use for sysroot

    elif platform in ('x86_64-linux',):
        info[platform] = {}
        info[platform]['version'] = defold_info[platform]['version']
        info[platform]['path'] = _get_defold_path(sdkfolder, platform)

    elif platform in ('win32', 'x86_64-win32'):
        windowsinfo = get_windows_packaged_sdk_info(sdkfolder, platform)
        return _setup_info_from_windowsinfo(windowsinfo, platform)

    elif platform in ('armv7-android', 'arm64-android'):
        info['version'] = ANDROID_BUILD_TOOLS_VERSION
        info['sdk'] = os.path.join(sdkfolder, "android-sdk")
        info['ndk'] = os.path.join(sdkfolder, "android-ndk-r%s" % ANDROID_NDK_VERSION)
        info['build_tools'] = os.path.join(info['sdk'], "build-tools" , ANDROID_BUILD_TOOLS_VERSION)
        if platform == 'arm64-android':
            info['api'] = ANDROID_64_NDK_API_VERSION
        else:
            info['api'] = ANDROID_NDK_API_VERSION

    return info

def _get_local_sdk_info(platform):
    info = {}
    if platform in ('x86_64-macos', 'arm64-macos','x86_64-ios','arm64-ios'):
        info['xcode'] = {}
        info['xcode']['version'] = get_local_darwin_toolchain_version()
        info['xcode']['path'] = get_local_darwin_toolchain_path()
        info['xcode-clang'] = get_local_darwin_clang_version()
        info['asan'] = {}
        info['asan']['path'] = os.path.join(info['xcode']['path'], MACOS_ASAN_PATH%info['xcode-clang'])
        info[platform] = {}
        info[platform]['version'] = get_local_darwin_sdk_version(platform)
        info[platform]['path'] = get_local_darwin_sdk_path(platform) # what we use for sysroot

        if not os.path.exists(info['asan']['path']):
            print("sdk.py: Couldn't find '%s'" % info['asan']['path'], file=sys.stderr)

    elif platform in ('x86_64-linux',):
        info[platform] = {}
        info[platform]['version'] = get_local_compiler_version()
        info[platform]['path'] = get_local_compiler_path()

    elif platform in ('win32', 'x86_64-win32'):
        windowsinfo = get_windows_local_sdk_info(platform)
        return _setup_info_from_windowsinfo(windowsinfo, platform)

    elif platform in ('armv7-android', 'arm64-android'):
        info['version'] = get_android_local_sdk_version(platform)
        info['sdk'] = get_android_local_sdk_path()
        info['ndk'] = get_android_local_ndk_path(platform)
        info['build_tools'] = get_android_local_build_tools_path(platform)
        if platform == 'arm64-android':
            info['api'] = ANDROID_64_NDK_API_VERSION
        else:
            info['api'] = ANDROID_NDK_API_VERSION

    return info

# It's only cached for the duration of one build
cached_platforms = defaultdict(defaultdict)

def get_sdk_info(sdkfolder, platform, verbose=False):
    if platform in cached_platforms:
        return cached_platforms[platform]

    if check_defold_sdk(sdkfolder, platform, verbose):
        result = _get_defold_sdk_info(sdkfolder, platform)
        cached_platforms[platform] = result
        return result

    if check_local_sdk(platform, verbose):
        result = _get_local_sdk_info(platform)
        cached_platforms[platform] = result
        return result

    return None

def get_toolchain_root(sdkinfo, platform):
    if platform in ('x86_64-macos','arm64-macos','x86_64-ios','arm64-ios'):
        return sdkinfo['xcode']['path']
    if platform in ('x86_64-linux',):
        return sdkinfo['x86_64-linux']['path']
    return None

def get_host_platform():
    machine = platform.machine().lower()
    if machine == 'amd64':
        machine = 'x86_64'
    is64bit = machine.endswith('64')

    if sys.platform == 'linux':
        return '%s-linux' % machine
    elif sys.platform == 'win32':
        return '%s-win32' % machine
    elif sys.platform == 'darwin':
        return '%s-macos' % machine

    raise Exception("Unknown host platform: %s, %s" % (sys.platform, machine))
