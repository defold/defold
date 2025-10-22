#!/usr/bin/env python
# Copyright 2020-2025 The Defold Foundation
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
import re
import platform
from collections import defaultdict

DYNAMO_HOME=os.environ.get('DYNAMO_HOME', os.path.join(os.getcwd(), 'tmp', 'dynamo_home'))

SDK_ROOT=os.path.join(DYNAMO_HOME, 'ext', 'SDKs')

## **********************************************************************************************
# Darwin

# A list of minimum versions here: https://developer.apple.com/support/xcode/

VERSION_XCODE="16.2" # we also use this to match version on Github Actions
VERSION_XCODE_CLANG="16.0.0"
VERSION_MACOSX="15.2"
VERSION_IPHONEOS="18.2"
VERSION_IPHONESIMULATOR="18.2"
MACOS_ASAN_PATH="usr/lib/clang/%s/lib/darwin/libclang_rt.asan_osx_dynamic.dylib"

# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
VERSION_IPHONEOS_MIN="11.0"
VERSION_MACOSX_MIN="10.15"

SWIFT_VERSION="5.5"

VERSION_LINUX_CLANG="16.0.0"
PACKAGES_LINUX_CLANG="clang-16.0.0"
PACKAGES_LINUX_X86_64_TOOLCHAIN="clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04"
PACKAGES_LINUX_ARM64_TOOLCHAIN="clang+llvm-16.0.0-aarch64-linux-gnu"

## **********************************************************************************************
# Android

ANDROID_NDK_VERSION='25b'
ANDROID_TARGET_API_LEVEL = 35
ANDROID_PACKAGE = "android-%s" % ANDROID_TARGET_API_LEVEL
ANDROID_BUILD_TOOLS_VERSION = '35.0.1'
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
# Emscripten

EMSCRIPTEN_VERSION_STR  =  "4.0.6"
EMSCRIPTEN_SDK          = f"sdk-{EMSCRIPTEN_VERSION_STR}-64bit"
PACKAGES_EMSCRIPTEN_SDK = f"emsdk-{EMSCRIPTEN_VERSION_STR}"

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
defold_info['x86_64-linux']['pattern'] = 'x86_64-linux/clang-%s' % VERSION_LINUX_CLANG

defold_info['arm64-linux']['version'] = VERSION_LINUX_CLANG
defold_info['arm64-linux']['pattern'] = 'arm64-linux/clang-%s' % VERSION_LINUX_CLANG

## **********************************************************************************************

def log_verbose(verbose, msg):
    if verbose:
        log.log(msg)

def _get_latest_version_from_folders(path, replace_patterns=[]):
    dirs = [x for x in os.listdir(path)]
    if len(dirs) == 0:
        return None

    def _replace_pattern(s, patterns):
        for pattern, replace in patterns:
            s = s.replace(pattern, replace)
        # handle -ext to fix Android versions like android-34-ext12
        if '-ext' in s:
            s = re.sub(r'-ext\d+$', '', s)
        return s

    dirs.sort(key=lambda x: tuple(int(token) for token in _replace_pattern(x, replace_patterns).split('.')), reverse=True)
    return dirs[0]

def _get_host_exe_suffix():
    if sys.platform == 'win32':
        return '.exe'
    return ''

class SDKException(Exception):
    pass

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

def get_android_sdk_path(root):
    path = os.path.join(root, "android-sdk")
    if not os.path.exists(path):
        raise SDKException(f"Path {path} not found")
    return path

def get_android_ndk_path(root):
    path = os.path.join(root, "android-ndk-r%s" % ANDROID_NDK_VERSION)
    if not os.path.exists(path):
        raise SDKException(f"Path {path} not found")
    return path

def get_android_build_tools_path(root):
    sdkfolder = get_android_sdk_path(root)
    path = os.path.join(sdkfolder, "build-tools", ANDROID_BUILD_TOOLS_VERSION)
    if not os.path.exists(path):
        raise SDKException(f"Path {path} not found")
    return path

def get_android_jar_path(root):
    sdkfolder = get_android_sdk_path(root)
    path = os.path.join(sdkfolder, 'platforms', 'android-%s' % ANDROID_TARGET_API_LEVEL, 'android.jar')
    if not os.path.exists(path):
        raise SDKException(f"Path {path} not found")
    return path


def get_android_local_sdk_path(verbose=False):
    path = os.environ.get('ANDROID_HOME', None)
    if path is None:
        log_verbose(verbose, f"  No ANDROID_HOME detected")

        # on macOS, it doesn't set an environment variable
        if sys.platform == 'darwin':
            path = os.path.expanduser('~/Library/android/sdk')
        elif sys.platform == 'win32':
            path = os.path.expandvars('${LOCALAPPDATA}/Android/Sdk')
        elif sys.platform == 'linux':
            path = os.path.expandvars('~/Android/Sdk')

        if path is not None:
            path = os.path.normpath(path)

    if path and os.path.exists(path):
        log_verbose(verbose, f"  Detected sdk path {path}")
        return path

    raise SDKException(f"Path {path} not found")

def get_android_local_ndk_path(platform, verbose=False):
    sdk_root = get_android_local_sdk_path()
    ndk_root = os.path.join(sdk_root, 'ndk')
    if not os.path.exists(ndk_root):
        raise SDKException(f"  Failed to find {ndk_root}")
    version = _get_latest_version_from_folders(ndk_root)
    if not version:
        raise SDKException(f"  No ndk versions installed in {ndk_root}")
    return os.path.join(ndk_root, version)

def get_android_local_build_tools_path(platform):
    path = get_android_local_sdk_path()
    build_tools_path = os.path.join(path, 'build-tools')
    if not os.path.exists(build_tools_path):
        raise SDKException(f"  Failed to find {build_tools_path}")

    version = _get_latest_version_from_folders(build_tools_path)
    return os.path.join(build_tools_path, version)

def get_android_local_sdk_version(platform):
    return os.path.basename(get_android_local_build_tools_path(platform))

def get_android_local_jar_path(verbose=False):
    sdkfolder = get_android_local_sdk_path()
    platforms_folder = os.path.join(sdkfolder, 'platforms')
    android_version = _get_latest_version_from_folders(platforms_folder, [('android-', '')])
    path = os.path.join(sdkfolder, 'platforms', android_version, 'android.jar')
    if not os.path.exists(path):
        raise SDKException(f"Path {path} not found")
    return path

def get_android_bintools_path(ndk, platform):
    ndk_os = 'linux'
    if sys.platform == 'darwin':
        ndk_os = 'darwin'
    elif sys.platform == 'win32':
        ndk_os = 'windows'
    return f'{ndk}/toolchains/llvm/prebuilt/{ndk_os}-x86_64/bin'

def get_android_api_version(platform):
    if platform == 'arm64-android':
        return ANDROID_64_NDK_API_VERSION
    else:
        return ANDROID_NDK_API_VERSION

def get_android_clang_name(platform, api_version):
    if platform == 'arm64-android':
        return f'aarch64-linux-android{api_version}-clang'
    else:
        return f'armv7a-linux-androideabi{api_version}-clang'


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

def _fatal(msg):
    print("sdk.py: %s" % msg)
    sys.exit(1)

def get_windows_include_dirs(vs_root, sdk_includes_root):
    includes = [os.path.join(vs_root,'include'),
                os.path.join(vs_root,'atlmfc','include'),
                os.path.join(sdk_includes_root,'ucrt'),
                os.path.join(sdk_includes_root,'winrt'),
                os.path.join(sdk_includes_root,'um'),
                os.path.join(sdk_includes_root,'shared')]

    for x in includes:
        if not os.path.exists(x):
            _fatal("Path does not exist: %s" % x)

    return ','.join(includes)

def get_windows_lib_dirs(vs_root, sdk_libs_root, arch):
    libdirs = [ os.path.join(vs_root,'lib',arch),
                os.path.join(vs_root,'atlmfc','lib',arch),
                os.path.join(sdk_libs_root,'ucrt',arch),
                os.path.join(sdk_libs_root,'um',arch)]

    for x in libdirs:
        if not os.path.exists(x):
            _fatal("Path does not exist: %s" % x)

    return ','.join(libdirs)

def get_windows_bin_dirs(vs_root, sdk_bin_root, arch):
    bindirs = [ os.path.join(vs_root,'bin', 'Host%s'%arch, arch),
                os.path.join(sdk_bin_root, arch)]

    for x in bindirs:
        if not os.path.exists(x):
            _fatal("Path does not exist: %s" % x)

    return ','.join(bindirs)

def get_windows_info(vs_root, vs_version, sdk_root, sdk_version, platform):
    arch = 'x64'
    if platform == 'win32':
        arch = 'x86'

    sdk_includes_root = os.path.join(sdk_root, 'Include', sdk_version)
    sdk_libs_root = os.path.join(sdk_root, 'Lib', sdk_version)
    sdk_bin_root = os.path.join(sdk_root, 'bin', sdk_version)

    includes = get_windows_include_dirs(vs_root, sdk_includes_root)
    lib_paths = get_windows_lib_dirs(vs_root, sdk_libs_root, arch)
    bin_paths = get_windows_bin_dirs(vs_root, sdk_bin_root, arch)

    info = {}
    info['sdk_root'] = sdk_root
    info['sdk_version'] = sdk_version
    info['includes'] = includes
    info['lib_paths'] = lib_paths
    info['bin_paths'] = bin_paths
    info['vs_root'] = vs_root
    info['vs_version'] = vs_version
    return info


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

    sdk_root = run.shell_command('%s --sdk_root' % vswhere_path).strip() # C:\Program Files (x86)\Windows Kits\10\
    sdk_version = run.shell_command('%s --sdk_version' % vswhere_path).strip()
    includes = run.shell_command('%s --includes' % vswhere_path).strip()
    lib_paths = run.shell_command('%s --lib_paths' % vswhere_path).strip()
    bin_paths = run.shell_command('%s --bin_paths' % vswhere_path).strip()
    vs_root = run.shell_command('%s --vs_root' % vswhere_path).strip()
    vs_version = run.shell_command('%s --vs_version' % vswhere_path).strip()

    windows_info = get_windows_info(vs_root, vs_version, sdk_root, sdk_version, platform)
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

    vs_root = os.path.join(msvcdir,'VC','Tools','MSVC',msvc_version)
    sdk_root = os.path.join(windowskitsdir,'10')

    windows_info = get_windows_info(vs_root, msvc_version, sdk_root, ucrt_version, platform)
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
# Emscripten

def get_defold_emsdk():
    emsdk = os.path.join(SDK_ROOT, 'emsdk-' + EMSCRIPTEN_VERSION_STR)
    if emsdk is None:
        raise SDKException(f"Failed to installed EMSDK installation")
    return emsdk

def get_defold_emsdk_cache():
    emsdk = get_defold_emsdk()
    return os.path.join(emsdk, '.defold_cache')

def get_defold_emsdk_config():
    emsdk = get_defold_emsdk()
    return os.path.join(emsdk, '.emscripten')

def _find_node_in_emsdk(emsdk):
    node_dir = _get_latest_version_from_folders(os.path.join(emsdk, 'node'), [('_64bit', '')])
    node = os.path.join(emsdk, 'node', node_dir, 'bin', 'node' + _get_host_exe_suffix())
    if os.path.exists(node):
        return os.path.normpath(node).replace('\\', '/')
    raise SDKException(f"Failed to find local EMSDK_NODE installation: {node}")

def get_defold_emsdk_node():
    return _find_node_in_emsdk(get_defold_emsdk())

# ------------------------------------------------------------

def _get_local_emsdk():
    emsdk = os.environ.get('EMSDK', None)
    if emsdk is None:
        raise SDKException(f"Failed to find local EMSDK installation")
    return emsdk

# https://emscripten.org/docs/tools_reference/emcc.html?highlight=em_cache
def _get_local_emsdk_cache():
    emsdk = _get_local_emsdk()
    return os.path.join(emsdk, '.defold_cache')

# https://emscripten.org/docs/tools_reference/emcc.html?highlight=em_config
def _get_local_emsdk_config():
    emsdk = _get_local_emsdk()
    return os.path.join(emsdk, '.emscripten')

def _get_local_emsdk_node():
    node = os.environ.get('EMSDK_NODE', None)
    if node is not None:
        return node
    return _find_node_in_emsdk(_get_local_emsdk())

## **********************************************************************************************

def _get_defold_path(sdkfolder, platform):
    return os.path.join(sdkfolder, defold_info[platform]['pattern'])

def check_defold_sdk(sdkfolder, host_platform, platform, verbose=False):
    folders = []
    log_verbose(verbose, f"check_defold_sdk: {platform} {sdkfolder}")

    if sdk_vendor is not None and sdk_vendor.supports_platform(platform):
        folders = sdk_vendor.get_defold_sdk_folders(platform)

    elif platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        folders.append(_get_defold_path(sdkfolder, 'xcode'))
        folders.append(_get_defold_path(sdkfolder, platform))

    elif platform in ('x86_64-win32', 'win32'):
        folders.append(os.path.join(sdkfolder, 'Win32','WindowsKits','10'))
        folders.append(os.path.join(sdkfolder, 'Win32','MicrosoftVisualStudio14.0','VC'))

    elif platform in ('armv7-android', 'arm64-android'):
        folders.append(get_android_sdk_path(sdkfolder))
        folders.append(get_android_ndk_path(sdkfolder))

    elif platform in ('x86_64-linux','arm64-linux'):
        folders.append(os.path.join(sdkfolder, host_platform))

    elif platform in ('wasm-web','wasm_pthread-web','js-web'):
        folders.append(get_defold_emsdk())

    if not folders:
        log.log("sdk.py: No SDK folders specified for %s" %platform)
        return False

    count = 0
    msg = ''
    for f in folders:
        if not os.path.exists(f):
            msg += f"  Missing SDK in {f}\n"
        else:
            count = count + 1
    result = count == len(folders)

    if not result:
        raise SDKException(msg + "  No prepackaged sdk found.")
    else:
        log_verbose(verbose, "  Found prepackaged sdk folders:")
        for f in folders:
            log_verbose(verbose, "    %s" % f)

    return result

def check_local_sdk(platform, verbose=False):
    log_verbose(verbose, f"check_local_sdk: {platform}")

    if platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        xcode_version = get_local_darwin_toolchain_version()
        if not xcode_version:
            raise SDKException(f"Failed to find XCode version")

    elif platform in ('win32', 'x86_64-win32'):
        info = get_windows_local_sdk_info(platform)
        if info is None:
            raise SDKException(f"Failed to find Visual Studio")

    elif platform in ('armv7-android', 'arm64-android'):
        path = get_android_local_sdk_path()
        ndkpath = get_android_local_ndk_path(platform, verbose)
        return path is not None and ndkpath is not None

    return True


def _get_defold_sdk_info(sdkfolder, host_platform, platform):
    info = {}

    if sdk_vendor is not None and sdk_vendor.supports_platform(platform):
        return sdk_vendor.get_sdk_info(platform)

    elif platform in ('x86_64-macos', 'arm64-macos','x86_64-ios','arm64-ios'):
        info['xcode'] = {}
        info['xcode']['version'] = VERSION_XCODE
        info['xcode']['path'] = _get_defold_path(sdkfolder, 'xcode')
        info['xcode-clang'] = defold_info['xcode-clang']['version']
        info['asan'] = {}
        info['asan']['path'] = os.path.join(info['xcode']['path'], MACOS_ASAN_PATH%info['xcode-clang'])
        info[platform] = {}
        info[platform]['version'] = defold_info[platform]['version']
        info[platform]['path'] = _get_defold_path(sdkfolder, platform) # what we use for sysroot

    elif platform in ('x86_64-linux','arm64-linux'):
        info[platform] = {}
        info[platform]['version'] = defold_info[platform]['version']
        # We download the package for the host platform, and rely on its ability cross compile
        info[platform]['path'] = _get_defold_path(sdkfolder, host_platform)

    elif platform in ('win32', 'x86_64-win32'):
        windowsinfo = get_windows_packaged_sdk_info(sdkfolder, platform)
        return _setup_info_from_windowsinfo(windowsinfo, platform)

    elif platform in ('armv7-android', 'arm64-android'):
        info['version']     = ANDROID_BUILD_TOOLS_VERSION
        info['sdk']         = get_android_sdk_path(sdkfolder)
        info['ndk']         = get_android_ndk_path(sdkfolder)
        info['build_tools'] = get_android_build_tools_path(sdkfolder)
        info['jar']         = get_android_jar_path(sdkfolder)
        info['bintools']    = get_android_bintools_path(info['ndk'], platform)
        info['api']         = get_android_api_version(platform)
        info['clangname']   = get_android_clang_name(platform, info['api'])

    elif platform in ('js-web', 'wasm-web', 'wasm_pthread-web'):
        info['emsdk'] = {}
        info['emsdk']['path'] = get_defold_emsdk()
        info['emsdk']['cache'] = get_defold_emsdk_cache()
        info['emsdk']['config'] = get_defold_emsdk_config()
        info['emsdk']['node'] = get_defold_emsdk_node()

    return info

def _get_local_sdk_info(platform, verbose=False):
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

    elif platform in ('x86_64-linux','arm64-linux'):
        info[platform] = {}
        info[platform]['version'] = get_local_compiler_version()
        info[platform]['path'] = get_local_compiler_path()

    elif platform in ('win32', 'x86_64-win32'):
        windowsinfo = get_windows_local_sdk_info(platform)
        return _setup_info_from_windowsinfo(windowsinfo, platform)

    elif platform in ('armv7-android', 'arm64-android'):
        ndk_os = 'linux'
        if sys.platform == 'darwin':
            ndk_os = 'darwin'
        elif sys.platform == 'win32':
            ndk_os = 'windows'

        info['version']     = get_android_local_sdk_version(platform)
        info['sdk']         = get_android_local_sdk_path(verbose)
        info['ndk']         = get_android_local_ndk_path(platform, verbose)
        info['jar']         = get_android_local_jar_path(verbose)
        info['build_tools'] = get_android_local_build_tools_path(platform)
        info['bintools']    = get_android_bintools_path(info['ndk'], platform)
        info['api']         = get_android_api_version(platform)
        info['clangname']   = get_android_clang_name(platform, info['api'])

    elif platform in ('js-web', 'wasm-web', 'wasm_pthread-web'):
        info['emsdk'] = {}
        info['emsdk']['path'] = _get_local_emsdk()
        info['emsdk']['cache'] = _get_local_emsdk_cache()
        info['emsdk']['config'] = _get_local_emsdk_config()
        info['emsdk']['node'] = _get_local_emsdk_node()

    return info

# ********************************************************************
# vendor

def dummy_check_vendor_sdk(sdkfolder, platform, verbose):
    pass

check_vendor_sdk = dummy_check_vendor_sdk

try:
    import sdk_vendor
    sdk_vendor.SDKException = SDKException # give access to the esception
    check_vendor_sdk = sdk_vendor.check_vendor_sdk
except ModuleNotFoundError as e:
    # Currently, the output is parsed by other scripts
    if "No module named 'sdk_vendor'" in str(e):
        pass
    else:
        raise e
except Exception as e:
    print("Failed to import sdk_vendor.py:")
    raise e

if 'sdk_vendor' not in sys.modules:
    sdk_vendor = None

# ********************************************************************

def get_host_platform():
    machine = platform.machine().lower()
    if machine == 'amd64':
        machine = 'x86_64'
    is64bit = machine.endswith('64')

    if sys.platform == 'linux':
        if machine == 'aarch64':
            machine = 'arm64'
        return '%s-linux' % machine
    elif sys.platform == 'win32':
        if machine == 'arm64':
            machine = 'x86_64' # we don't support arm64 windows targets yet
        return '%s-win32' % machine
    elif sys.platform == 'darwin':
        return '%s-macos' % machine

    raise Exception("Unknown host platform: %s, %s" % (sys.platform, machine))

# It's only cached for the duration of one build
cached_platforms = defaultdict(defaultdict)


def get_sdk_info(sdkfolder, platform, verbose=False):
    if platform in cached_platforms:
        return cached_platforms[platform]

    host_platform = get_host_platform()
    try:
        if check_vendor_sdk(sdkfolder, platform, verbose):
            result = _get_defold_sdk_info(sdkfolder, host_platform, platform)
            cached_platforms[platform] = result
            return result
    except SDKException as e:
        log_verbose(verbose, e)

    try:
        if check_defold_sdk(sdkfolder, host_platform, platform, verbose):
            result = _get_defold_sdk_info(sdkfolder, host_platform, platform)
            cached_platforms[platform] = result
            return result
    except SDKException as e:
        log_verbose(verbose, e)

    try:
        if check_local_sdk(platform, verbose):
            result = _get_local_sdk_info(platform, verbose)
            cached_platforms[platform] = result
            return result
    except SDKException as e:
        log_verbose(verbose, e)

    return None

def _create_hello_world(path):
    parent = os.path.dirname(path)
    if not os.path.exists(parent):
        os.makedirs(parent)
    with open(path, 'w', encoding='utf-8') as f:
        s  = "#include <stdio.h>\n"
        s += "int main(int argc, char** argv)\n"
        s += "{\n"
        s += "    printf(\"hello world!\\n\");\n"
        s += "    return 0;\n"
        s += "}\n"
        s += ""
        f.write(s)

def _get_clang_arch_from_platform(platform):
    if platform == 'x86_64-linux':  return 'x86_64-unknown-linux-gnu'
    if platform == 'arm64-linux':   return 'aarch64-unknown-linux-gnu'
    if platform == 'x86_64-macos':  return 'x86_64-apple-darwin19'
    if platform == 'arm64-macos':   return 'arm64-apple-darwin19'
    return None

class TestSdkException(Exception):
    pass

def _compile_file_clang(platform, info, srcfile, exefile, verbose):
    # if we can rely on the PATH variable
    use_local_path = False
    if platform in ['arm64-linux', 'x86_64-linux', 'arm64-macos', 'x86_64-macos', 'arm64-ios', 'x86_64-ios']:
        use_local_path = True

        clang = run.shell_command(f'which clang++')
        if verbose:
            log.log(clang)

        if not clang:
            raise TestSdkException("Path not found for clang!")

    clang = 'clang++'
    sysroot = ''
    arch = ''

    if platform in ['arm64-android', 'armv7-android']:
        clang = os.path.join(info['bintools'], info['clangname'])

    elif platform in ['arm64-ios', 'x86_64-ios']:
        sysroot = '-isysroot' + info[platform]['path']
        arch = '-arch ' + platform.split('-')[0]

    if not use_local_path:
        if not os.path.exists(clang):
            raise TestSdkException(f"Path not found for clang: '{clang}'")

    target = _get_clang_arch_from_platform(platform)
    if target is not None:
        target = f'--target={target}'
    else:
        target = ''

    cmd = f'{clang} {sysroot} {target} {arch} {srcfile} -o {exefile}'
    return run.shell_command(cmd)

def _test_compiler_clang(platform, info, can_run, verbose):
    testdir = os.path.join(os.environ['DYNAMO_HOME'], 'sdktest')
    testfile = os.path.join(testdir, 'hello.cpp')
    exefile = os.path.join(testdir, 'a.out')
    _create_hello_world(testfile)
    output = _compile_file_clang(platform, info, testfile, exefile, verbose)

    if verbose:
        output = run.shell_command(f'file {exefile}')
        log.log(output)

    if can_run:
        output = run.shell_command(f'{exefile}')
        if verbose:
            log.log(output)


def test_sdk(platform, info, verbose=False):
    host = get_host_platform()
    use_clang = False

    can_run = host == platform
    if not can_run:
        can_run = host == 'arm64-macos' and platform == 'x86_64-macos'

    if platform in ['arm64-linux', 'x86_64-linux',
                    'arm64-macos', 'x86_64-macos',
                    'arm64-ios', 'x86_64-ios',
                    'arm64-android', 'armv7-android']:
        use_clang = True

    try:
        if use_clang:
            _test_compiler_clang(platform, info, can_run, verbose)

    except TestSdkException as e:
        log.log(e)
        return False

    return True

def get_toolchain_root(sdkinfo, platform):
    if platform in ('x86_64-macos','arm64-macos','x86_64-ios','arm64-ios'):
        return sdkinfo['xcode']['path']
    if platform in ('x86_64-linux','arm64-linux'):
        return sdkinfo[platform]['path']
    return None
