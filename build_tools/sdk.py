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
import platform
from collections import defaultdict

DYNAMO_HOME=os.environ.get('DYNAMO_HOME', os.path.join(os.getcwd(), 'tmp', 'dynamo_home'))

SDK_ROOT=os.path.join(DYNAMO_HOME, 'ext', 'SDKs')

## **********************************************************************************************
# Darwin

VERSION_XCODE="13.2.1"
VERSION_MACOSX="12.1"
VERSION_IPHONEOS="15.2"
VERSION_IPHONESIMULATOR="15.2"

# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
VERSION_IPHONEOS_MIN="8.0"
VERSION_MACOSX_MIN="10.7"

VERSION_XCODE_CLANG="13.0.0"
SWIFT_VERSION="5.5"

## **********************************************************************************************
# Android

ANDROID_NDK_VERSION='20'

## **********************************************************************************************
# Win32

# The version we have prepackaged
VERSION_WINDOWS_SDK_10="10.0.18362.0"
VERSION_WINDOWS_MSVC_2019="14.25.28610"
PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-2019-{0}".format(VERSION_WINDOWS_MSVC_2019)
PACKAGES_WIN32_SDK_10="WindowsKits-{0}".format(VERSION_WINDOWS_SDK_10)

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
defold_info['armv7-darwin']['version'] = VERSION_IPHONEOS
defold_info['armv7-darwin']['pattern'] = PACKAGES_IOS_SDK
defold_info['arm64-darwin']['version'] = VERSION_IPHONEOS
defold_info['arm64-darwin']['pattern'] = PACKAGES_IOS_SDK
defold_info['x86_64-ios']['version'] = VERSION_IPHONESIMULATOR
defold_info['x86_64-ios']['pattern'] = PACKAGES_IOS_SIMULATOR_SDK
defold_info['x86_64-darwin']['version'] = VERSION_MACOSX
defold_info['x86_64-darwin']['pattern'] = PACKAGES_MACOS_SDK


defold_info['x86_64-win32']['version'] = VERSION_WINDOWS_SDK_10
defold_info['x86_64-win32']['pattern'] = "Win32/%s" % PACKAGES_WIN32_TOOLCHAIN
defold_info['win32']['version'] = defold_info['x86_64-win32']['version']
defold_info['win32']['pattern'] = defold_info['x86_64-win32']['pattern']

defold_info['win10sdk']['version'] = VERSION_WINDOWS_SDK_10
defold_info['win10sdk']['pattern'] = "Win32/%s" % PACKAGES_WIN32_SDK_10


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

## **********************************************************************************************

# Windows

windows_info = None

def get_windows_local_sdk_info(platform):
    global windows_info

    if windows_info is not None:
        return windows_info

    vswhere_path = '%s/../../scripts/windows/vswhere2/vswhere2.exe' % os.environ['DYNAMO_HOME']
    if not os.path.exists(vswhere_path):
        vswhere_path = './scripts/windows/vswhere2/vswhere2.exe'
        vswhere_path = path.normpath(vswhere_path)
        if not os.path.exists(vswhere_path):
            print "Couldn't find executable '%s'" % vswhere_path
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

def check_defold_sdk(sdkfolder, platform):
    folders = []

    if platform in ('x86_64-darwin', 'armv7-darwin', 'arm64-darwin', 'x86_64-ios'):
        folders.append(_get_defold_path(sdkfolder, 'xcode'))
        folders.append(_get_defold_path(sdkfolder, platform))

    if platform in ('x86_64-win32', 'win32'):
        folders.append(os.path.join(sdkfolder, 'Win32','WindowsKits','10'))
        folders.append(os.path.join(sdkfolder, 'Win32','MicrosoftVisualStudio14.0','VC'))

    if platform in ('armv7-android', 'arm64-android'):
        folders.append(os.path.join(sdkfolder, "android-ndk-r%s" % ANDROID_NDK_VERSION))
        folders.append(os.path.join(sdkfolder, "android-sdk"))

    ok = True
    for f in folders:
        if not os.path.exists(f):
            log.log("Missing SDK in %s" % f)
            ok = False
    return ok


def check_local_sdk(platform):
    if platform in ('x86_64-darwin', 'armv7-darwin', 'arm64-darwin', 'x86_64-ios'):
        xcode_version = get_local_darwin_toolchain_version()
        if not xcode_version:
            return False
    if platform in ('win32', 'x86_64-win32'):
        info = get_windows_local_sdk_info(platform)
        return info is not None

    return True


def _get_defold_sdk_info(sdkfolder, platform):
    info = {}
    if platform in ('x86_64-darwin','x86_64-ios','armv7-darwin','arm64-darwin'):
        info['xcode'] = {}
        info['xcode']['version'] = VERSION_XCODE
        info['xcode']['path'] = _get_defold_path(sdkfolder, 'xcode')
        info['xcode-clang'] = defold_info['xcode-clang']
        info[platform] = {}
        info[platform]['version'] = defold_info[platform]['version']
        info[platform]['path'] = _get_defold_path(sdkfolder, 'xcode')

    if platform in ('win32', 'x86_64-win32'):
        windowsinfo = get_windows_packaged_sdk_info(sdkfolder, platform)
        return _setup_info_from_windowsinfo(windowsinfo, platform)

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

    if platform in ('win32', 'x86_64-win32'):
        windowsinfo = get_windows_local_sdk_info(platform)
        return _setup_info_from_windowsinfo(windowsinfo, platform)

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










