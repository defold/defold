#!/usr/bin/env python
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

# add build_tools folder to the import search path
import sys, os, platform
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath, splitext
sys.path.append(os.path.join(normpath(join(dirname(abspath(__file__)), '..')), "build_tools"))

import shutil, zipfile, re, itertools, json, platform, math, mimetypes, hashlib
import optparse, pprint, subprocess, urllib, urllib.parse, tempfile, time
import github
import build_android
import build_ios
import codesigning
import run
import s3
import sdk
from cross_build import DEFOLD_PLATFORMS_FILE, get_configured_platforms, get_platform_root, get_platforms_config_path, load_platforms_config, save_platforms_config, write_merged_platform_sdks
from private_hooks import call_hook, has_hook_module
import wasm_runner
import release_to_github
import release_to_steam
import release_to_egs
import releasenotes
import BuildUtility
import http_cache
import sdk_merge
import solution_msvs
import solution_msvs_xbox
import solution_xcode
from datetime import datetime
from urllib.parse import urlparse
from glob import glob
from threading import Thread, Event
from queue import Queue
from configparser import ConfigParser
from BuildTimeTracker import BuildTimeTracker

BASE_PLATFORMS = [  'x86_64-linux', 'arm64-linux',
                    'x86_64-macos', 'arm64-macos',
                    'win32', 'x86_64-win32',
                    'x86_64-ios', 'arm64-ios',
                    'armv7-android', 'arm64-android',
                    'wasm-web', 'wasm_pthread-web']

_CMAKE_FEATURE_FLAG_MAP = {
    '--with-asan': 'WITH_ASAN',
    '--with-ubsan': 'WITH_UBSAN',
    '--with-tsan': 'WITH_TSAN',
    '--with-valgrind': 'WITH_VALGRIND',
    '--with-openal': 'WITH_OPENAL',
    '--with-opengl': 'WITH_OPENGL',
    '--with-vulkan': 'WITH_VULKAN',
    '--with-vulkan-validation': 'WITH_VULKAN_VALIDATION',
    '--with-dx12': 'WITH_DX12',
    '--with-metal': 'WITH_METAL',
    '--with-opus': 'WITH_OPUS',
    '--with-webgpu': 'WITH_WEBGPU'
}

_CMAKE_FEATURE_LIST_OPTIONS = {
    '--enable-feature': 'DEFOLD_ENABLE_FEATURES',
    '--disable-feature': 'DEFOLD_DISABLE_FEATURES'
}

JAVA_RUNTIME_FLAGS = '--sun-misc-unsafe-memory-access=allow --enable-native-access=ALL-UNNAMED'
MINIMUM_PYTHON_VERSION = (3, 12)

def get_legacy_private_target_platforms():
    try:
        import build_vendor
    except ModuleNotFoundError as e:
        if "No module named 'build_vendor'" in str(e):
            return []
        raise
    return build_vendor.get_target_platforms() if hasattr(build_vendor, 'get_target_platforms') else []

class build_private(object):
    _target_platform = None

    @classmethod
    def set_target_platform(cls, platform):
        cls._target_platform = platform

    @classmethod
    def _call(cls, platform, name, default, *args):
        platform = platform or cls._target_platform
        return call_hook('build', platform, name, default, *args) if platform else default

    @classmethod
    def get_target_platforms(cls):
        return get_configured_platforms() + get_legacy_private_target_platforms()

    @classmethod
    def get_install_host_packages(cls, platform): # Returns the packages that should be installed for the host
        return cls._call(None, 'get_install_host_packages', [], platform)

    @classmethod
    def get_install_target_packages(cls, platform): # Returns the packages that should be installed for the target
        return cls._call(platform, 'get_install_target_packages', [], platform)

    @classmethod
    def install_sdk(cls, configuration, platform): # Installs the sdk for the private platform
        return cls._call(platform, 'install_sdk', None, configuration, platform)

    @classmethod
    def is_library_supported(cls, platform, library):
        return cls._call(platform, 'is_library_supported', True, platform, library)

    @classmethod
    def is_repo_private(cls):
        return cls._call(None, 'is_repo_private', has_hook_module('build', cls._target_platform))

    @classmethod
    def get_tag_suffix(cls):
        return cls._call(None, 'get_tag_suffix', '')

    @classmethod
    def can_run_tests(cls, platform, log_fn, env, device):
        return cls._call(platform, 'can_run_tests', False, log_fn, env, device, get_platform_root(platform))

assert(hasattr(build_private, 'get_target_platforms'))
assert(hasattr(build_private, 'get_install_host_packages'))
assert(hasattr(build_private, 'get_install_target_packages'))
assert(hasattr(build_private, 'install_sdk'))
assert(hasattr(build_private, 'is_library_supported'))
assert(hasattr(build_private, 'is_repo_private'))
assert(hasattr(build_private, 'get_tag_suffix'))

def get_target_platforms():
    platforms = BASE_PLATFORMS + build_private.get_target_platforms()
    return list(dict.fromkeys(platforms))

def get_default_target_platforms():
    return BASE_PLATFORMS

PACKAGES_ALL=[
    "protobuf-3.20.1",
    "junit-4.6",
    "jsign-4.2",
    "bundletool-all",
    "openal-1.1",
    "maven-3.0.1",
    "vecmath",
    "vpx-1.7.0",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "defold-robot-0.7.0",
    "bullet-2.77",
    "libunwind-395b27b68c5453222378bc5fe4dab4c6db89816a",
    "jctest-0.14",
    "vulkan-v1.4.307",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

PACKAGES_HOST=[
    "vpx-1.7.0",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1"]

PACKAGES_IOS_X86_64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "glfw-2.7.1",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

PACKAGES_IOS_64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "moltenvk-1474891",
    "glfw-2.7.1",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

PACKAGES_MACOS_X86_64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "vpx-1.7.0",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "spirv-cross-97709575",
    "spirv-tools-b21dda0e",
    "glslang-42d9adf5",
    "moltenvk-1474891",
    "lipo-4c7c275",
    "sassc-5472db213ec223a67482df2226622be372921847",
    "glfw-3.4",
    "tint-22b958",
    "astcenc-30aabb3",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038",
    "gltf-validator-2.0.0-dev.3.10",
    "aapt2-36.1.0",
    "codesign_allocate",
    "ogg-1.1.1",
    "strip",
    "strip_android-12.0.9",
    "zipalign"]

PACKAGES_MACOS_ARM64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "vpx-1.7.0",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "spirv-cross-97709575",
    "spirv-tools-b21dda0e",
    "glslang-42d9adf5",
    "moltenvk-1474891",
    "lipo-4c7c275",
    "glfw-3.4",
    "tint-22b958",
    "astcenc-30aabb3",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038",
    "gltf-validator-2.0.0-dev.3.10",
    "aapt2-36.1.0",
    "codesign_allocate",
    "ogg-1.1.1",
    "strip",
    "strip_android-12.0.9",
    "zipalign"]

PACKAGES_WIN32=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "glut-3.7.6",
    "bullet-2.77",
    "vulkan-v1.4.307",
    "glfw-3.4",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

PACKAGES_WIN32_64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "glut-3.7.6",
    "sassc-5472db213ec223a67482df2226622be372921847",
    "bullet-2.77",
    "glslang-42d9adf5",
    "spirv-cross-97709575",
    "spirv-tools-d24a39a7",
    "vulkan-v1.4.307",
    "lipo-4c7c275",
    "glfw-3.4",
    "tint-22b958",
    "astcenc-30aabb3",
    "directx-headers-1.611.0",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038",
    "gltf-validator-2.0.0-dev.3.10",
    "aapt2-36.1.0",
    "ogg-1.1.1",
    "strip_android-12.0.9",
    "strip_android_aarch64-12.0.9",
    "zipalign"]

PACKAGES_LINUX_X86_64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "bullet-2.77",
    "glslang-ba5c010c",
    "spirv-cross-97709575",
    "spirv-tools-d24a39a7",
    "vpx-1.7.0",
    "vulkan-v1.4.307",
    "tremolo-b0cb4d1",
    "lipo-4c7c275",
    "glfw-3.4",
    "tint-7bd151a780",
    "sassc-5472db213ec223a67482df2226622be372921847",
    "astcenc-30aabb3",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038",
    "gltf-validator-2.0.0-dev.3.10",
    "aapt2-36.1.0",
    "apkc-0.1.0",
    "ogg-1.1.1",
    "strip_android-12.0.9",
    "strip_android_aarch64-12.0.9",
    "zipalign"]

PACKAGES_LINUX_ARM64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "bullet-2.77",
    "glslang-2fed4fc0",
    "spirv-cross-97709575",
    "spirv-tools-4fab7435",
    "vpx-1.7.0",
    "vulkan-v1.4.307",
    "tremolo-b0cb4d1",
    "lipo-4c7c275",
    "glfw-3.4",
    "tint-7bd151a780",
    "astcenc-30aabb3",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038",
    "gltf-validator-2.0.0-dev.3.10"]

PACKAGES_ANDROID=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "glfw-2.7.1",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "vkquality-1.1-2642a0d",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]
PACKAGES_ANDROID.append(sdk.ANDROID_PACKAGE)

PACKAGES_ANDROID_64=[
    "protobuf-3.20.1",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "glfw-2.7.1",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "vkquality-1.1-2642a0d",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]
PACKAGES_ANDROID_64.append(sdk.ANDROID_PACKAGE)

PACKAGES_EMSCRIPTEN=[
    "protobuf-3.20.1",
    "bullet-2.77",
    "glfw-2.7.1",
    "wagyu-69",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-13.2.1",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

PACKAGES_NODE_MODULES=["xhr2-0.1.0"]

PLATFORM_PACKAGES = {
    'win32':            PACKAGES_WIN32,
    'x86_64-win32':     PACKAGES_WIN32_64,
    'x86_64-linux':     PACKAGES_LINUX_X86_64,
    'arm64-linux':      PACKAGES_LINUX_ARM64,
    'x86_64-macos':     PACKAGES_MACOS_X86_64,
    'arm64-macos':      PACKAGES_MACOS_ARM64,
    'arm64-ios':        PACKAGES_IOS_64,
    'x86_64-ios':       PACKAGES_IOS_X86_64,
    'armv7-android':    PACKAGES_ANDROID,
    'arm64-android':    PACKAGES_ANDROID_64,
    'wasm-web':         PACKAGES_EMSCRIPTEN,
    'wasm_pthread-web': PACKAGES_EMSCRIPTEN
}

BOB_TOOL_PLATFORMS = [
    'x86_64-macos',
    'arm64-macos',
    'x86_64-linux',
    'arm64-linux',
    'x86_64-win32'
]

# SDKs that include host-side protoc/native-extension pipeline tools.
SDK_PIPELINE_TOOL_PLATFORMS = (
    'x86_64-macos',
    'arm64-macos',
    'x86_64-linux',
    'arm64-linux',
    'x86_64-win32'
)

BOB_TOOL_PACKAGE_PREFIXES = (
    'aapt2-',
    'apkc-',
    'glslang-',
    'gltf-validator-',
    'lipo-',
    'luajit-',
    'ogg-',
    'spirv-tools-',
    'strip_android-',
    'strip_android_aarch64-',
    'tint-',
)

BOB_TOOL_PACKAGES = ('codesign_allocate', 'strip', 'zipalign')

BOB_EXTRA_PLATFORM_PACKAGES = {
    'armv7-android': ["vkquality-1.1-2642a0d"],
    'arm64-android': [sdk.ANDROID_PACKAGE, "vkquality-1.1-2642a0d"]
}

DMSDK_PACKAGES_ALL="vectormathlibrary-r1649".split()

CDN_PACKAGES_URL=os.environ.get("DM_PACKAGES_URL", None)
DEFAULT_ARCHIVE_DOMAIN=os.environ.get("DM_ARCHIVE_DOMAIN", "d.defold.com")
DEFAULT_RELEASE_REPOSITORY=os.environ.get("DM_RELEASE_REPOSITORY") or os.environ.get("GITHUB_REPOSITORY") or release_to_github.get_current_repo()

PACKAGES_NODE_MODULE_XHR2="xhr2-v0.1.0"
PACKAGES_ANDROID_NDK="android-ndk-r{0}".format(sdk.ANDROID_NDK_VERSION)
PACKAGES_ANDROID_SDK="android-sdk"

NODE_MODULE_LIB_DIR = os.path.join("ext", "lib", "node_modules")

SHELL = os.environ.get('SHELL', 'bash')
# Don't use WSL from the msys/cygwin terminal
if os.environ.get('TERM','') in ('cygwin',):
    if 'WD' in os.environ:
        SHELL= '%s\\bash.exe' % os.environ['WD'] # the binary directory

ENGINE_LIBS = "testmain dlib jni texc modelc shaderc ddf platform graphics font particle lua hid input physics resource extension script render rig gameobject gui sound liveupdate crash gamesys tools record profiler engine sdk".split()
HOST_LIBS = "testmain dlib jni texc modelc shaderc".split()

EXTERNAL_WAF_LIBS = "box2d box2d_v2 glfw bullet3d opus".split()
EXTERNAL_CMAKE_LIBS = "vkquality".split()
EXTERNAL_LIBS = EXTERNAL_WAF_LIBS + EXTERNAL_CMAKE_LIBS
EXTERNAL_PACKAGE_VERSIONS = {
    "vkquality": "1.1-2642a0d",
}

def get_host_platform():
    return sdk.get_host_platform()

def format_exes(name, platform):
    prefix = ''
    suffix = ['']
    if platform in ['win32', 'x86_64-win32', 'x86_64-xbone']:
        suffix = ['.exe']
    elif 'android' in platform:
        prefix = 'lib'
        suffix = ['.so']
    elif platform in ['wasm-web', 'wasm_pthread-web']:
        prefix = ''
        suffix = ['.js', '.wasm']
    elif platform in ['arm64-nx64']:
        prefix = ''
        suffix = ['.nss', '.nso']
    elif platform in ['x86_64-ps4', 'x86_64-ps5']:
        prefix = ''
        suffix = ['.elf']
    else:
        suffix = ['']

    exes = []
    for suff in suffix:
        exes.append('%s%s%s' % (prefix, name, suff))
    return exes

def format_lib(name, platform):
    prefix = 'lib'
    suffix = ''
    if 'macos' in platform or 'ios' in platform:
        suffix = '.dylib'
    elif 'win32' in platform:
        prefix = ''
        suffix = '.dll'
    else:
        suffix = '.so'
    return '%s%s%s' % (prefix, name, suffix)

class ThreadPool(object):
    def __init__(self, worker_count):
        self.workers = []
        self.work_queue = Queue()

        for _ in range(worker_count):
            w = Thread(target = self.worker, daemon=True)
            w.start()
            self.workers.append(w)

    def worker(self):
        func, args, future = self.work_queue.get()
        while func:
            try:
                result = func(*args)
                future.result = result
            except Exception as e:
                future.result = e
            future.event.set()
            func, args, future = self.work_queue.get()

class Future(object):
    def __init__(self, pool, f, *args):
        self.result = None
        self.event = Event()
        pool.work_queue.put([f, args, self])

    def __call__(self):
        try:
            # In order to respond to ctrl+c wait with timeout...
            while not self.event.is_set():
                self.event.wait(0.1)
        except KeyboardInterrupt as e:
            sys.exit(0)

        if isinstance(self.result, Exception):
            raise self.result
        else:
            return self.result

def download_sdk(conf, url, targetfolder, strip_components=1, force_extract=False, format='z'):
    if not os.path.exists(targetfolder) or force_extract:
        if not os.path.exists(os.path.dirname(targetfolder)):
            os.makedirs(os.path.dirname(targetfolder))
        path = conf.get_local_or_remote_file(url)
        conf._extract_tgz_rename_folder(path, targetfolder, strip_components, format=format)
    else:
        print ("SDK already installed:", targetfolder)

class Configuration(object):
    def __init__(self,
                 defold_home = None,
                 dynamo_home = None,
                 target_platform = None,
                 skip_tests = False,
                 test_device = None,
                 ios_identity = None,
                 ios_mobileprovision = None,
                 ios_team_id = None,
                 ios_bundle_id_prefix = None,
                 keep_bob_uncompressed = False,
                 codesign = False,
                 skip_docs = False,
                 incremental = False,
                 skip_builtins = False,
                 skip_bob_light = False,
                 disable_ccache = False,
                 generate_compile_commands = False,
                 no_colors = False,
                 archive_domain = None,
                 package_path = None,
                 external_package = None,
                 set_version = None,
                 channel = None,
                 engine_artifacts = None,
                 waf_options = [],
                 save_env_path = None,
                 private_repo = None,
                 private_platform = None,
                 notarization_username = None,
                 notarization_password = None,
                 notarization_itc_provider = None,
                 github_token = None,
                 github_target_repo = None,
                 github_sha1 = None,
                 version = None,
                 codesigning_identity = None,
                 gcloud_projectid = None,
                 gcloud_location = None,
                 gcloud_keyringname = None,
                 gcloud_keyname = None,
                 gcloud_certfile = None,
                 gcloud_keyfile = None,
                 verbose = False):

        if sys.platform == 'win32':
            home = os.environ['USERPROFILE']
        else:
            home = os.environ['HOME']

        self.defold_home = os.path.normpath(join(os.path.dirname(__file__), '..'))
        self.dynamo_home = dynamo_home if dynamo_home else join(self.defold_home, 'tmp', 'dynamo_home')
        self.ext = join(self.dynamo_home, 'ext')
        self.dmsdk = join(self.dynamo_home, 'sdk')
        self.defold = normpath(join(dirname(abspath(__file__)), '..'))
        self.defold_root = os.getcwd()
        self.host = get_host_platform()
        self.target_platform = target_platform
        self.sdk_info = None

        self.build_utility = BuildUtility.BuildUtility(self.target_platform, self.host, self.dynamo_home)

        self.skip_tests = skip_tests
        self.test_device = test_device
        self.ios_identity = ios_identity
        self.ios_mobileprovision = ios_mobileprovision
        self.ios_team_id = ios_team_id
        self.ios_bundle_id_prefix = ios_bundle_id_prefix
        self.keep_bob_uncompressed = keep_bob_uncompressed
        self.codesign = codesign
        self.skip_docs = skip_docs
        self.incremental = incremental
        self.skip_builtins = skip_builtins
        self.skip_bob_light = skip_bob_light
        self.disable_ccache = disable_ccache
        self.generate_compile_commands = generate_compile_commands
        self.no_colors = no_colors
        self.archive_path = "s3://%s/archive" % (archive_domain)
        self.archive_domain = archive_domain
        self.package_path = package_path
        self.external_package = external_package
        self.set_version = set_version
        self.channel = channel
        self.engine_artifacts = engine_artifacts
        self.waf_options = waf_options
        self.save_env_path = save_env_path
        self.private_repo = private_repo
        self.private_platform = private_platform
        self.notarization_username = notarization_username
        self.notarization_password = notarization_password
        self.notarization_itc_provider = notarization_itc_provider
        self.github_token = github_token
        self.github_target_repo = github_target_repo
        self.github_sha1 = github_sha1
        self.version = version
        self.codesigning_identity = codesigning_identity
        self.gcloud_projectid = gcloud_projectid
        self.gcloud_location = gcloud_location
        self.gcloud_keyringname = gcloud_keyringname
        self.gcloud_keyname = gcloud_keyname
        self.gcloud_certfile = gcloud_certfile
        self.gcloud_keyfile = gcloud_keyfile
        self.verbose = verbose
        self.build_tracker = BuildTimeTracker(logger=self._log)

        if self.github_token is None:
            self.github_token = os.environ.get("GITHUB_TOKEN")

        self.thread_pool = None
        self.futures = []

        if version is None:
            with open('VERSION', 'r') as f:
                self.version = f.readlines()[0].strip()

        self._create_common_dirs()

    def make_solution(self, target_platform=None, build_dir=None):
        """
        Generate an IDE solution from the top-level CMakeLists.txt for the given target platform.

        - macOS/iOS: Xcode
        - Windows: Visual Studio
        - Android: Prints guidance to open CMakeLists.txt in Android Studio
        - Other platforms: falls back to Ninja/Unix Makefiles by default
        """
        tp = target_platform or self.target_platform
        if not tp:
            raise RuntimeError('make_solution: target_platform must be specified')

        private_root = get_platform_root(tp)
        if tp == 'x86_64-xbone':
            if not private_root:
                raise RuntimeError('make_solution: x86_64-xbone requires a configured private Xbox repo. Run add_private_repo with --platform=x86_64-xbone first.')

        build_type = self._find_cmake_build_type(self.waf_options)
        build_tests = 'OFF' if '--skip-build-tests' in self.waf_options else 'ON'
        build_home = self._platform_build_home(tp)

        # Android guidance
        if 'android' in tp:
            self._log('Android: Open the top-level CMakeLists.txt directly in Android Studio to create a project.')
            return
        ios_signing_cmake_args = []
        if tp == build_ios.IOS_DEVICE_PLATFORM:
            try:
                ios_signing_cmake_args = build_ios.xcode_solution_signing_cmake_args(
                    identity=self.ios_identity,
                    mobileprovision=self.ios_mobileprovision,
                    team_id=self.ios_team_id,
                    env=os.environ,
                    log_fn=self._log)
            except build_ios.IOSTestError as e:
                self.fatal(str(e))
            if not ios_signing_cmake_args:
                self._warn(build_ios.IOS_XCODE_UNSIGNED_WARNING)

        # Choose generator
        generator = None
        arch_args = []
        msvs_instance = None
        windows_sdk_version = None

        if tp.endswith('-macos') or tp.endswith('-ios'):
            generator = 'Xcode'
        elif tp == 'x86_64-xbone' or solution_msvs.is_visual_studio_platform(tp):
            msvs_selection = solution_msvs.latest_selection(self._log)
            generator = msvs_selection['generator']
            msvs_instance = msvs_selection.get('instance')
            windows_sdk_version = solution_msvs.latest_windows_sdk_version()
            arch_args = solution_msvs.arch_args(tp)

        # Keep IDE solutions in the public checkout even when the target uses
        # a private platform repo for source overlays and platform modules.
        solution_output_dir = None
        if not build_dir:
            build_dir = os.path.join(self.defold_root, 'solutions', tp)
        if tp == 'x86_64-xbone':
            solution_output_dir = os.path.abspath(build_dir)
            build_dir = os.path.join(solution_output_dir, 'cmake')

        target_name = f"Defold-{tp}"
        cmake_cmd = [
            'cmake', '-S', self.defold_root, '-B', build_dir,
            f'-DTARGET_PLATFORM={tp}',
            f'-DCMAKE_BUILD_TYPE={build_type}',
            f'-DBUILD_TESTS={build_tests}',
            f'-DDEFOLD_SOLUTION_NAME:STRING={target_name}',
            f'-DDEFOLD_BUILD_HOME:PATH={build_home}',
            f'-DDEFOLD_SDK_ROOT:PATH={self.dynamo_home}',
            f'-DCMAKE_INSTALL_PREFIX:PATH={self.dynamo_home}',
            f'-DDEFOLD_TEST_COLORS:BOOL={"OFF" if self.no_colors else "ON"}'
        ]
        cmake_cmd += build_ios.ios_test_cmake_args(
            tp,
            bundle_id_prefix=self.ios_bundle_id_prefix,
            env=os.environ)
        cmake_cmd += self._cmake_feature_defines()
        cmake_cmd += ios_signing_cmake_args
        if generator:
            cmake_cmd += ['-G', generator]
        if arch_args:
            cmake_cmd += arch_args
        if solution_msvs.is_visual_studio_generator(generator):
            cmake_cmd += solution_msvs.cmake_args(generator, msvs_instance, windows_sdk_version)
            cmake_cmd += ['-DDEFOLD_MSVC_IDE_SOLUTION:BOOL=ON']
            if tp == 'x86_64-xbone':
                cmake_cmd += [f'-DDEFOLD_XBONE_PRIVATE_REPO_ROOT:PATH={private_root}']

        cmake_configure_state = self._cmake_configure_state(build_dir, cmake_cmd)
        cmake_configure_state_path = join(build_dir, '.defold_cmake_configure.json')
        previous_cmake_configure_state = None
        if os.path.exists(cmake_configure_state_path):
            with open(cmake_configure_state_path, 'r') as f:
                previous_cmake_configure_state = json.load(f)

        cmake_cache = join(build_dir, 'CMakeCache.txt')
        if os.path.exists(cmake_cache) and not (
                self._cmake_configure_state_matches(cmake_configure_state, previous_cmake_configure_state, False)
                and self._cmake_cache_matches_configure_state(cmake_cache, cmake_configure_state)):
            self._clean_cmake_builddir(build_dir)

        self._mkdirs(build_dir)

        # Generate solution
        self._log(f'CMake solution settings: platform={tp}, build_type={build_type}, build_tests={build_tests}')
        if solution_msvs.is_visual_studio_generator(generator):
            solution_msvs.log_selection(self._log, generator, msvs_instance, windows_sdk_version)
        self._log('Generating solution with command: %s' % ' '.join(cmake_cmd))
        run.env_command(self._form_env(), cmake_cmd)
        with open(cmake_configure_state_path, 'w') as f:
            json.dump(cmake_configure_state, f, indent=2, sort_keys=True)

        old_project_name = 'defold_libraries'
        solution_build_dir = os.path.abspath(build_dir)
        final_path = solution_build_dir

        if generator == 'Xcode':
            final_path = os.path.join(solution_build_dir, f"{target_name}.xcodeproj")
        elif solution_msvs.is_visual_studio_generator(generator):
            final_path = solution_msvs.final_solution_path(build_dir, target_name)

        stale_solution_paths = [
            os.path.join(solution_build_dir, f"{old_project_name}.xcodeproj"),
        ]
        for stale_path in stale_solution_paths:
            if stale_path != final_path and os.path.exists(stale_path):
                try:
                    if os.path.isdir(stale_path):
                        shutil.rmtree(stale_path)
                    else:
                        os.remove(stale_path)
                except Exception as e:
                    self._log(f"Warning: Failed to remove stale generated solution {stale_path}: {e}")
        if solution_msvs.is_visual_studio_generator(generator):
            solution_msvs.cleanup_stale_solutions(build_dir, final_path, old_project_name, self._log)

        if generator and not os.path.exists(final_path):
            self._log(f"Warning: Expected solution file was not found: {final_path}")

        if solution_msvs.is_visual_studio_generator(generator):
            if tp == 'x86_64-xbone':
                solution_msvs.organize_solution(final_path, self.defold_root, self._log)
                xbone_solution_path = solution_msvs_xbox.generate_xbone_solution(
                    solution_output_dir or solution_build_dir,
                    build_dir,
                    target_name,
                    private_root,
                    self.defold_root,
                    self._log,
                    final_path,
                    generator,
                    windows_sdk_version)
                if xbone_solution_path:
                    final_path = xbone_solution_path
            else:
                solution_msvs.organize_solution(final_path, self.defold_root, self._log)

        if generator == 'Xcode' and os.path.exists(final_path):
            solution_xcode.configure_project(
                final_path,
                build_type,
                tp,
                self.defold_home,
                self.defold,
                self.dynamo_home,
                self._log)

        self._log(f'Solution generated: {final_path}')

    def __del__(self):
        if len(getattr(self, "futures", [])) > 0:
            print('ERROR: Pending futures (%d)' % len(self.futures))
            os._exit(5)

    def get_python(self):
        self.check_python()
        return [sys.executable]

    def _create_common_dirs(self):
        for p in ['ext/lib/python', 'share', 'lib/wasm-web/js', 'lib/wasm_pthread-web/js']:
            self._mkdirs(join(self.dynamo_home, p))

    def _mkdirs(self, path):
        if not os.path.exists(path):
            os.makedirs(path)

    def _log(self, msg):
        print(str(msg))
        sys.stdout.flush()
        sys.stderr.flush()

    def _colorize(self, msg, color_code):
        if self.no_colors or os.environ.get('NOCOLOR'):
            return msg
        return '\033[%sm%s\033[0m' % (color_code, msg)

    def _warn(self, msg):
        self._log(self._colorize(msg, '33'))

    def _remove_tree(self, path):
        if os.path.exists(path):
            self._log('Removing %s' % path)
            shutil.rmtree(path)

    def distclean(self):
        self._remove_tree(self.dynamo_home)

        for builddir in glob(join(self.defold_root, 'engine/*/build')):
            self._remove_tree(builddir)
        self._remove_tree(join(self.defold_root, 'share/extender/build'))
        self._remove_tree(join(self.defold_root, 'build', 'cmake'))
        self._remove_tree(join(self.defold_root, 'engine', 'build'))
        self._remove_tree(join(self.defold_root, 'solutions'))

        # remove engine test dir specifically
        self._remove_tree(join(self.defold_root, 'engine/engine/src/test/build'))

        # Recreate dirs
        self._create_common_dirs()
        self._log('distclean done.')

    def _clean_cmake_builddir(self, builddir):
        try:
            self._remove_tree(builddir)
            return
        except PermissionError as e:
            vs_state_dir = join(builddir, '.vs')
            if not os.path.isdir(vs_state_dir):
                raise
            self._log(f'Warning: Could not remove {builddir}: {e}')
            self._log(f'Keeping locked Visual Studio state directory: {vs_state_dir}')

        for name in os.listdir(builddir):
            path = join(builddir, name)
            if normpath(path).lower() == normpath(vs_state_dir).lower():
                continue
            if os.path.isdir(path):
                shutil.rmtree(path)
            else:
                os.remove(path)

    def clean(self):
        """
        Remove generated engine build outputs without removing installed SDK
        packages, installed jars, or built documentation artifacts.
        """
        for builddir in glob(join(self.defold_root, 'engine/*/build')):
            self._remove_tree(builddir)
        self._remove_tree(join(self.defold_root, 'share/extender/build'))
        self._remove_tree(join(self.defold_root, 'engine', 'build'))

        self._remove_tree(join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob/build'))
        self._remove_tree(join(self.defold_root, 'engine/engine/src/test/build'))
        self._log('clean done.')

    def _extract_tgz(self, file, path):
        self._log('Extracting %s to %s' % (file, path))
        self._mkdirs(path)
        suffix = os.path.splitext(file)[1]
        fmts = {'.gz': 'z', '.xz': 'J', '.bzip2': 'j'}
        cmd = ['tar', 'xf%s' % fmts.get(suffix, 'z'), file]
        if os.name == 'nt':
            cmd.append('--unlink-first')
        run.env_command(self._form_env(), cmd, cwd = path)

    def _extract_tgz_rename_folder(self, src, target_folder, strip_components=1, format=None):
        src = src.replace('\\', '/')

        force_local = ''
        if os.environ.get('GITHUB_SHA', None) is not None and os.environ.get('TERM', '') == 'cygwin':
            force_local = '--force-local' # to make tar not try to "connect" because it found a colon in the source file

        self._log('Extracting %s to %s/' % (src, target_folder))
        parentdir, dirname = os.path.split(target_folder)
        old_dir = os.getcwd()
        os.chdir(parentdir)
        self._mkdirs(dirname)

        if format is None:
            suffix = os.path.splitext(src)[1]
            fmts = {'.gz': 'z', '.xz': 'J', '.bzip2': 'j'}
            format = fmts.get(suffix, 'z')
        cmd = ['tar', 'xf%s' % format, src, '-C', dirname]
        if strip_components:
            cmd.extend(['--strip-components', '%d' % strip_components])
        if os.name == 'nt':
            cmd.append('--unlink-first')
        if force_local:
            cmd.append(force_local)

        run.env_command(self._form_env(), cmd)
        os.chdir(old_dir)

    def _extract_zip(self, file, path):
        self._log('Extracting %s to %s' % (file, path))

        def _extract_zip_entry( zf, info, extract_dir ):
            zf.extract( info.filename, path=extract_dir )
            out_path = os.path.join( extract_dir, info.filename )
            perm = info.external_attr >> 16
            os.chmod( out_path, perm )

        with zipfile.ZipFile(file, 'r') as zf:
            for info in zf.infolist():
                _extract_zip_entry( zf, info, path )

    def _extract(self, file, path):
        if os.path.splitext(file)[1] == '.zip':
            self._extract_zip(file, path)
        else:
            self._extract_tgz(file, path)

    def _is_bob_tool_package(self, package):
        return package in BOB_TOOL_PACKAGES or package.startswith(BOB_TOOL_PACKAGE_PREFIXES)

    def install_bob_tool_packages(self):
        def make_package_path(root, platform, package):
            return join(root, 'packages', package) + '-%s.tar.gz' % platform

        installed_packages = set()
        for platform in BOB_TOOL_PLATFORMS:
            packages = [package for package in PLATFORM_PACKAGES.get(platform, []) if self._is_bob_tool_package(package)]
            packages.extend(BOB_EXTRA_PLATFORM_PACKAGES.get(platform, []))
            if not packages:
                continue
            print("Installing Bob tool packages for %s" % platform)
            for package in packages:
                package_path = make_package_path(self.defold_root, platform, package)
                if package_path in installed_packages:
                    continue
                self._extract_tgz(package_path, self.ext)
                installed_packages.add(package_path)

        for platform, packages in BOB_EXTRA_PLATFORM_PACKAGES.items():
            print("Installing Bob extra packages for %s" % platform)
            for package in packages:
                package_path = make_package_path(self.defold_root, platform, package)
                if package_path in installed_packages:
                    continue
                self._extract_tgz(package_path, self.ext)
                installed_packages.add(package_path)

    def _copy(self, src, dst):
        self._log('Copying %s -> %s' % (src, dst))
        shutil.copy(src, dst)

    def _copy_tree(self, src, dst):
        self._log('Copying %s -> %s' % (src, dst))
        shutil.copytree(src, dst)

    def _download(self, url):
        self._log('Downloading %s' % (url))
        path = http_cache.download(url, lambda count, total: self._log('Downloading %s %.2f%%' % (url, 100 * count / float(total))))
        if not path:
            self._log('Downloading %s failed' % (url))
        return path

    def _check_package_path(self):
        if self.package_path is None:
            print("No package path provided. Use either --package-path option or DM_PACKAGES_URL environment variable")
            sys.exit(1)

    def install_waf(self):
        def make_package_path(root, platform, package):
            return join(root, 'packages', package) + '-%s.tar.gz' % platform
        print("Installing waf")
        waf_package = "waf-2.1.9"
        waf_path = make_package_path(self.defold_root, 'common', waf_package)
        self._extract_tgz(waf_path, self.ext)

    def _install_python_packages(self, packages):
        target = join(self.ext, 'lib', 'python')
        wheelhouse = join(self.defold_root, 'packages', 'python')
        self._mkdirs(target)

        if packages:
            run.env_command(self._form_env(), self.get_python() + [
                '-m', 'pip',
                '-q', '-q',
                'install',
                '--no-index',
                '--find-links', wheelhouse,
                '--only-binary', ':all:',
                '--upgrade',
                '--no-compile',
                '-t', target,
            ] + packages)

    def install_release_dependencies(self):
        print("Installing release python dependencies")
        self._install_python_packages(
            [
                'boto3==1.36.3',
                'requests==2.34.2',
            ])

    def install_ext(self):
        def make_package_path(root, platform, package):
            return join(root, 'packages', package) + '-%s.tar.gz' % platform

        def make_package_paths(root, platform, packages):
            return [make_package_path(root, platform, package) for package in packages]

        def make_private_package_path(platform, package):
            private_root = get_platform_root(self.target_platform)
            package_roots = [root for root in [private_root, self.defold_root] if root]
            paths = [make_package_path(root, platform, package) for root in package_roots]
            for path in paths:
                if os.path.exists(path):
                    return path
            self.fatal("Could not find private package %s for %s. Looked in:\n  %s" % (package, platform, "\n  ".join(paths)))

        def make_private_package_paths(platform, packages):
            return [make_private_package_path(platform, package) for package in packages]

        if self._build_engine_with_waf():
            self.install_waf()

        print("Installing common packages")
        for p in PACKAGES_ALL:
            self._extract_tgz(make_package_path(self.defold_root, 'common', p), self.ext)

        for p in DMSDK_PACKAGES_ALL:
            self._extract_tgz(make_package_path(self.defold_root, 'common', p), self.dmsdk)

        # TODO: Make sure the order of install does not affect the outcome!

        base_platforms = self.get_base_platforms()
        target_platform = self.target_platform
        other_platforms = set(PLATFORM_PACKAGES.keys()).difference(set(base_platforms), set([target_platform, self.host]))

        if target_platform in ['wasm-web', 'wasm_pthread-web']:
            node_modules_dir = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR)
            for package in PACKAGES_NODE_MODULES:
                path = join(self.defold_root, 'packages', package + '.tar.gz')
                name = package.split('-')[0]
                self._extract_tgz(path, join(node_modules_dir, name))

        installed_packages = set()

        for platform in other_platforms:
            packages = PLATFORM_PACKAGES.get(platform, [])
            package_paths = make_package_paths(self.defold_root, platform, packages)
            print("Installing %s packages " % platform)
            for path in package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(package_paths)

        for base_platform in base_platforms:
            packages = list(PACKAGES_HOST)
            packages.extend(PLATFORM_PACKAGES.get(base_platform, []))
            package_paths = make_package_paths(self.defold_root, base_platform, packages)
            package_paths.extend(make_private_package_paths(base_platform, build_private.get_install_host_packages(base_platform)))
            package_paths = [path for path in package_paths if path not in installed_packages]
            if len(package_paths) != 0:
                print("Installing %s packages" % base_platform)
                for path in package_paths:
                    self._extract_tgz(path, self.ext)
                installed_packages.update(package_paths)

        # For easier usage with the extender server, we want the linux protoc tool available
        if target_platform in ('x86_64-macos', 'arm64-macos', 'x86_64-win32', 'x86_64-linux'):
            protobuf_packages = filter(lambda x: "protobuf" in x, PACKAGES_HOST)
            package_paths = make_package_paths(self.defold_root, 'x86_64-linux', protobuf_packages)
            print("Installing %s protobuf packages " % 'x86_64-linux')
            for path in package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(package_paths)

        target_package_paths = make_package_paths(self.defold_root, self.target_platform, PLATFORM_PACKAGES.get(self.target_platform, []))
        target_package_paths.extend(make_private_package_paths(self.target_platform, build_private.get_install_target_packages(self.target_platform)))
        target_package_paths = [path for path in target_package_paths if path not in installed_packages]

        if len(target_package_paths) != 0:
            print("Installing %s packages" % self.target_platform)
            for path in target_package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(target_package_paths)

        print("Installing python wheels")
        self._install_python_packages([
            'Markdown==3.3.7',
            'Pygments==2.12.0',
            'boto3==1.36.3',
            'protobuf==3.20.1',
            'PyYAML==6.0.3',
            'pystache==0.6.8',
            'rangehttpserver==1.4.0',
            'requests==2.34.2',
        ])

        print("Installing javascripts")
        for n in 'web-pre.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        for n in 'web-pre-engine.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        print("Installing profiles etc")
        for n in itertools.chain(*[ glob('share/*%s' % ext) for ext in ['.mobileprovision', '.xcent', '.supp']]):
            self._copy(join(self.defold_root, n), join(self.dynamo_home, 'share'))

        # Simple way to reduce number of warnings in the build
        proto_path = os.path.join(self.dynamo_home, 'share', 'proto')
        if not os.path.exists(proto_path):
            os.makedirs(proto_path)

    def get_local_or_remote_file(self, path):
        if os.path.isdir(self.package_path): # is is a local path?
            if os.path.exists(path):
                return os.path.normpath(os.path.abspath(path))
            print("Could not find local file:", path)
            sys.exit(1)
        dirname, basename = os.path.split(path)
        path = dirname + "/" + urllib.parse.quote(basename)
        path = self._download(path) # it should be an url
        if path is None:
            print("Error. Could not download %s" % path)
            sys.exit(1)
        return path

    def _get_python_version(self):
        return ".".join([str(v) for v in sys.version_info[:3]])

    def check_python(self, print_check = False):
        python_version = self._get_python_version()
        required_version = ".".join([str(v) for v in MINIMUM_PYTHON_VERSION])
        if sys.version_info[:2] < MINIMUM_PYTHON_VERSION:
            self.fatal("The build scripts require Python %s+! Found Python %s: %s" % (required_version, python_version, sys.executable))
        if print_check:
            self._log("Found Python: %s (%s)" % (sys.executable, python_version))

    def has_sdk(self, sdkfolder, target_platform):
        return None != sdk.get_sdk_info(sdkfolder, target_platform, False)

    def _find_program(self, platform, name, paths):
        name = format_exes(name, platform)[0]
        for path in paths:
            fullpath = os.path.join(path, name)
            if os.path.isfile(fullpath):
                return fullpath
        return None

    def _get_emsdk_node_candidate(self):
        if not wasm_runner.is_web_platform(self.target_platform):
            return None

        sdk_info = self.sdk_info
        if not sdk_info:
            sdkfolder = join(self.ext, 'SDKs')
            sdk_info = sdk.get_sdk_info(sdkfolder, self.target_platform, False)

        if not sdk_info:
            return None

        return sdk_info.get('emsdk', {}).get('node')

    def _find_wasm_test_runner(self):
        node_candidate = self._get_emsdk_node_candidate()
        node_candidates = [node_candidate] if node_candidate else []
        return wasm_runner.find_wasm_runner(node_candidates = node_candidates)

    def _format_wasm_runner_error(self, errors):
        message = "Bun or Node.js is required to run wasm-web tests. Use --skip-tests to build without running tests."
        if errors:
            message += "\nChecked runners:\n  " + "\n  ".join(errors)
        return message

    def check_sdk(self):
        self.check_python(print_check = True)

        sdkfolder = join(self.ext, 'SDKs')

        self.sdk_info = sdk.get_sdk_info(sdkfolder, self.target_platform, True)

        # TODO: Make sure this check works for all platforms
        if not self.sdk_info:
            if not self.verbose:
                # Do it again, with verbose on, so that we can get more info straight away:
                sdk.get_sdk_info(sdkfolder, self.target_platform, True)

            url = "https://github.com/defold/defold/blob/dev/README_BUILD.md#important-prerequisite---platform-sdks"
            self._log(f"Failed to get sdk info for platform {self.target_platform}.")
            self._log(f" * Is the local sdk setup correctly?")
            self._log(f" * Or have you called `install_sdk`?")
            self._log(f"We recommend you follow the setup guide found here: {url}")
            sys.exit(1)

        if self.verbose:
            print("SDK info:")
            pprint.pprint(self.sdk_info)


        result = sdk.test_sdk(self.target_platform, self.sdk_info, verbose = self.verbose)
        if not result:
            self.fatal("Failed sdk check")

        if wasm_runner.is_web_platform(self.target_platform):
            runner, errors = self._find_wasm_test_runner()
            if runner:
                self._log("Found wasm test runner: %s" % runner.description())
            elif self.skip_tests:
                self._log("Warning: %s" % self._format_wasm_runner_error(errors))
            else:
                self.fatal(self._format_wasm_runner_error(errors))

        cmake = shutil.which('cmake')
        if not cmake:
            self.fatal("CMake not found in PATH")
        self._log(f"Found CMake: {cmake}")

        ninja = shutil.which('ninja')
        if not ninja:
            self.fatal("Ninja not found in PATH")
        self._log(f"Found Ninja: {ninja}")

        cmake_target_platform = self._cmake_target_platform(target_platform)
        args = ["cmake", f"-DTARGET_PLATFORM={cmake_target_platform}", "-P", join(self.defold_root, "scripts/cmake/check_install.cmake")]
        if self.verbose:
            args.insert(1, '-DDEFOLD_VERBOSE=ON')

        output = run.command(args)
        self._log(output)

    def verify_sdk(self):
        was_verbose = self.verbose
        self.verbose = True
        self.check_sdk()

        def _test_compiler_cmd(self, prefix, verbose):
            return '%s %s/ext/bin/waf --prefix=%s distclean configure build --skip-tests --skip-build-tests %s' % (' '.join(self.get_python()), self.dynamo_home, prefix, verbose and '-v' or '')

        args = _test_compiler_cmd(self, self.dynamo_home, was_verbose)
        args = args.split()
        self._log('Testing compiler for platform %s' % (target_platform))
        cwd = join(self.defold_root, 'engine/sdk/test/toolchain')
        plf_args = ['--platform=%s' % target_platform]
        run.env_command(self._form_env(), args + plf_args + self._waf_forward_options(), cwd = cwd)

    def install_sdk(self):
        sdkfolder = join(self.ext, 'SDKs')
        target_platform = self.target_platform

        # check host tools availability
        has_host_sdk = False
        if sdk.get_host_platform() != target_platform:
            has_host_sdk = self.has_sdk(sdkfolder, sdk.get_host_platform())

        if target_platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
            # macOS SDK
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_MACOS_SDK), join(sdkfolder, sdk.PACKAGES_MACOS_SDK))
            download_sdk(self,'%s/%s.darwin.tar.gz' % (self.package_path, sdk.PACKAGES_XCODE_TOOLCHAIN), sdkfolder, force_extract=True)

        if target_platform in ('arm64-ios', 'x86_64-ios'):
            # iOS SDK
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_IOS_SDK), join(sdkfolder, sdk.PACKAGES_IOS_SDK))
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_IOS_SIMULATOR_SDK), join(sdkfolder, sdk.PACKAGES_IOS_SIMULATOR_SDK))

        if 'win32' in target_platform or ('win32' in self.host and not has_host_sdk):
            if self.package_path is None:
                self.fatal("The package path isn't specified. Either define DM_PACKAGES_URL or use --package-path.")
            win32_sdk_folder = join(self.ext, 'SDKs', 'Win32')
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_WIN32_SDK), join(win32_sdk_folder, 'WindowsKits', '10') )
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_WIN32_TOOLCHAIN), join(win32_sdk_folder, 'MicrosoftVisualStudio14.0'), strip_components=0 )

        if target_platform in ('wasm-web', 'wasm_pthread-web'):
            emsdk_folder = sdk.get_defold_emsdk()
            download_sdk(self,'%s/%s-%s.tar.gz' % (self.package_path, sdk.PACKAGES_EMSCRIPTEN_SDK, self.host), emsdk_folder)

            if not os.path.isfile(sdk.get_defold_emsdk_config()):
                print("Activating emsdk")

                os.environ['EMSCRIPTEN'] = emsdk_folder
                os.environ['EM_CONFIG'] = sdk.get_defold_emsdk_config()
                os.environ['EM_CACHE'] = sdk.get_defold_emsdk_cache()
                self._activate_ems(emsdk_folder, join(emsdk_folder, 'upstream', 'emscripten'), sdk.EMSCRIPTEN_VERSION_STR)

            # On OSX, the file system is already case insensitive, so no need to duplicate the files as we do on the extender server

        if target_platform in ('armv7-android', 'arm64-android'):
            host = self.host
            if 'win32' in host:
                host = 'win'
            elif 'linux' in host:
                host = 'linux'
            elif 'macos' in host:
                host = 'darwin' # our packages are still called darwin

            # Android NDK
            download_sdk(self, '%s/%s-%s.tar.gz' % (self.package_path, PACKAGES_ANDROID_NDK, host), join(sdkfolder, PACKAGES_ANDROID_NDK))
            # Android SDK
            download_sdk(self, '%s/%s-%s-android-%s-%s.tar.gz' % (self.package_path, PACKAGES_ANDROID_SDK, host, sdk.ANDROID_TARGET_API_LEVEL, sdk.ANDROID_BUILD_TOOLS_VERSION), join(sdkfolder, PACKAGES_ANDROID_SDK))

        build_private.install_sdk(self, target_platform)

    def _activate_ems(self, emsdk, bin_dir, version):
        run.env_command(self._form_env(), [join(emsdk, 'emsdk'), 'activate', version, '--embedded'])

        # prewarm the cache
        # Although this method might be more "correct", it also takes 10 minutes more than we'd like on CI
        #run.env_command(self._form_env(), ['%s/embuilder.py' % self._form_ems_path(), 'build', 'SYSTEM', 'MINIMAL'])
        # .. so we stick with the old version of prewarming

        # Compile a file warm up the emscripten caches (libc etc)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.c', delete=False) as f:
            c_file = f.name
            f.write('int main() { return 0; }')
        with tempfile.NamedTemporaryFile(suffix='.js', delete=False) as f:
            exe_file = f.name
        run.env_command(self._form_env(), [f'{bin_dir}/emcc', c_file, '-o', '%s' % exe_file])

    def _git_sha1(self, ref = None):
        return self.build_utility.git_sha1(ref)

    def _ziptree(self, path, outfile = None, directory = None):
        # Directory is similar to -C in tar
        if not outfile:
            outfile = tempfile.NamedTemporaryFile(delete = False)

        zip = zipfile.ZipFile(outfile, 'w', zipfile.ZIP_DEFLATED)
        for root, dirs, files in os.walk(path):
            for f in files:
                p = os.path.join(root, f)
                an = p
                if directory:
                    an = os.path.relpath(p, directory)
                zip.write(p, an)

        zip.close()
        return outfile.name

    def _add_files_to_zip(self, zip, paths, basedir=None, topfolder=None, path_filter=None, path_mapper=None):
        for p in paths:
            if not os.path.isfile(p):
                continue
            an = p
            if basedir:
                an = os.path.relpath(p, basedir)
            an = an.replace('\\', '/')
            if path_filter and not path_filter(an):
                continue
            if path_mapper:
                an = path_mapper(an)
            if topfolder:
                an = os.path.join(topfolder, an)
            an = an.replace('\\', '/')
            if an in zip.NameToInfo:
                continue
            zip.write(p, an)

    def _add_file_to_zip(self, zip, src, dst):
        if not os.path.isfile(src):
            self._log("Path is not a file: '%s'" % src)
        zip.write(src, dst)

    def is_cross_platform(self):
        return self.host != self.target_platform

    def is_desktop_target(self):
        return self.target_platform in ['x86_64-linux', 'arm64-linux', 'x86_64-macos', 'arm64-macos', 'x86_64-win32']

    def _package_platform_sdk_headers(self, path):
        with open(path, 'wb') as outfile:
            zip = zipfile.ZipFile(outfile, 'w', zipfile.ZIP_DEFLATED)

            basedir = self.dynamo_home
            topfolder = 'defoldsdk'

            def is_header(path):
                return file.endswith('.h') or file.endswith('.hpp')

            # Includes
            includes = []
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "sdk/include")):
                for file in files:
                    if is_header(file):
                        includes.append(os.path.join(root, file))

            # proto _ddf.h + "res_*.h"
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "include")):
                for file in files:
                    if is_header(file) and ('ddf' in file or file.startswith('res_')):
                        includes.append(os.path.join(root, file))

            self._add_files_to_zip(zip, includes, basedir, topfolder)

            zip.close()

    def _create_sha256_signature_file(self, input_filepath):
        file_sha256 = hashlib.sha256()
        with open(input_filepath, 'rb') as source_archive:
            for byte_block in iter(lambda: source_archive.read(4096), b""):
                file_sha256.update(byte_block)
            source_archive.close()

        print("File {} sha256 signature is {}".format(input_filepath, file_sha256.hexdigest()))
        sig_filename = None
        with open(splitext(input_filepath)[0] + '.sha256', 'w') as sig_file:
            sig_filename = sig_file.name
            sig_file.write(file_sha256.hexdigest())
            sig_file.close()
        return sig_filename

    # package the native SDK, return the path to the zip file
    # and path to zip sha256 signature file
    def _package_platform_sdk(self, platform=None):
        platform = platform or self.target_platform
        sdk_archive_path = join(self.dynamo_home, 'defoldsdk.zip')
        with open(sdk_archive_path, 'wb') as outfile:
            zip = zipfile.ZipFile(outfile, 'w', zipfile.ZIP_DEFLATED)

            topfolder = 'defoldsdk'
            defold_home = os.path.normpath(os.path.join(self.dynamo_home, '..', '..'))

            def is_header(path):
                return file.endswith('.h') or file.endswith('.hpp')

            # Includes
            includes = []
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "sdk/include")):
                for file in files:
                    if is_header(file):
                        includes.append(os.path.join(root, file))

            # proto _ddf.h + "res_*.h"
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "include")):
                for file in files:
                    if is_header(file) and ('ddf' in file or file.startswith('res_')):
                        includes.append(os.path.join(root, file))

            self._add_files_to_zip(zip, includes, self.dynamo_home, topfolder)

            # Configs
            configs = ['extender/build.yml']
            configs = [os.path.join(self.dynamo_home, x) for x in configs]
            self._add_files_to_zip(zip, configs, self.dynamo_home, topfolder)

            # Variants
            variants = []
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "extender/variants")):
                for file in files:
                    if file.endswith('.appmanifest'):
                        variants.append(os.path.join(root, file))

            self._add_files_to_zip(zip, variants, self.dynamo_home, topfolder)

            def _findlibs(libdirs):
                if isinstance(libdirs, str):
                    libdirs = [libdirs]

                paths = []
                tried = []
                found_dir = False
                for libdir in libdirs:
                    tried.append(libdir)
                    if not os.path.isdir(libdir):
                        continue
                    found_dir = True
                    paths += [os.path.join(libdir, x) for x in os.listdir(libdir) if os.path.splitext(x)[1] in ('.a', '.dylib', '.so', '.lib', '.dll')]

                if not found_dir:
                    raise FileNotFoundError("No library directory found: %s" % ", ".join(tried))
                return paths

            def _findjars(jardir, ends_with):
                paths = os.listdir(jardir)
                paths = [os.path.join(jardir, x) for x in paths if x.endswith(ends_with)]
                return paths

            def _findjslibs(libdir):
                paths = os.listdir(libdir)
                paths = [os.path.join(libdir, x) for x in paths if os.path.splitext(x)[1] in ('.js',)]
                return paths

            def _findfiles(directory, exts):
                paths = []
                for root, dirs, files in os.walk(directory):
                    for f in files:
                        if os.path.splitext(f)[1] in exts:
                            paths.append(os.path.join(root, f))
                return paths

            def _sdk_lib_path_filter(path):
                basename = os.path.basename(path)
                root, _ = os.path.splitext(basename)
                shared_name = root[3:] if root.startswith('lib') else root
                if shared_name.endswith('_shared') and not (shared_name == 'dlib_shared' and platform in SDK_PIPELINE_TOOL_PLATFORMS):
                    return False
                return True

            def _sdk_lib_path_mapper(path):
                # We currently still use the old "win32" folder for our x86 files
                if path.startswith('lib/x86-win32/'):
                    path = 'lib/win32/' + path[len('lib/x86-win32/'):]
                elif path.startswith('ext/lib/x86-win32/'):
                    path = 'ext/lib/win32/' + path[len('ext/lib/x86-win32/'):]
                return path

            # Dynamo libs
            libdirs = [os.path.join(self.dynamo_home, 'lib/%s' % platform)]
            if platform == 'win32':
                libdirs.append(os.path.join(self.dynamo_home, 'lib/x86-win32'))
            paths = _findlibs(libdirs)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder, _sdk_lib_path_filter, _sdk_lib_path_mapper)
            # External libs
            libdirs = [os.path.join(self.dynamo_home, 'ext/lib/%s' % platform)]
            if platform == 'win32':
                libdirs.append(os.path.join(self.dynamo_home, 'ext/lib/x86-win32'))
            paths = _findlibs(libdirs)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder, _sdk_lib_path_filter, _sdk_lib_path_mapper)

            if platform in ['armv7-android', 'arm64-android']:
                # Android Jars (Dynamo)
                jardir = os.path.join(self.dynamo_home, 'share/java')
                paths = _findjars(jardir, ('android.jar', 'dlib.jar', 'r.jar'))
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

                # Android Jars (external)
                external_jars = ("glfw_android.jar", "vkquality.jar")
                jardir = os.path.join(self.dynamo_home, 'ext/share/java')
                paths = _findjars(jardir, external_jars)
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # Win32 resource files
            if platform in ['win32', 'x86_64-win32']:
                resource_dirs = [os.path.join(self.dynamo_home, 'lib/%s' % platform)]
                if platform == 'win32':
                    resource_dirs.append(os.path.join(self.dynamo_home, 'lib/x86-win32'))
                paths = []
                for resource_dir in resource_dirs:
                    paths.extend([
                        os.path.join(resource_dir, 'defold.ico'),
                        os.path.join(resource_dir, 'engine.rc')
                    ])
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder, path_mapper=_sdk_lib_path_mapper)

            # the port scripts contain the necessary files, only need to include them once
            if platform in ['wasm-web']:
                wagyu_port_files = []
                for root, dirs, files in os.walk(os.path.join(self.dynamo_home, 'ext/wagyu-port')):
                    for f in files:
                        _, ext = os.path.splitext(f)
                        if ext in ('.pyc',):
                            continue
                        path = os.path.join(root, f)
                        wagyu_port_files.append(path)

                if not wagyu_port_files:
                    raise Exception("Failed to find wagyu-port folder")

                self._add_files_to_zip(zip, wagyu_port_files, self.dynamo_home, topfolder)

            if platform in ['wasm-web', 'wasm_pthread-web']:
                for subdir in [f'lib/{platform}/js/', f'ext/lib/{platform}/js/']:
                    jsdir = os.path.join(self.dynamo_home, subdir)
                    paths = _findjslibs(jsdir)
                    self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            if platform in ['x86_64-ps4', 'x86_64-ps5']:
                memory_init = os.path.join(self.dynamo_home, 'ext/lib/%s/memory_init.o' % platform)
                self._add_files_to_zip(zip, [memory_init], self.dynamo_home, topfolder)

            # .proto files
            for d in ['share/proto/', 'ext/include/google/protobuf']:
                protodir = os.path.join(self.dynamo_home, d)
                paths = _findfiles(protodir, ('.proto',))
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # third-party headers
            for d in ['ext/include/vulkan', 'ext/include/vk_video']:
                protodir = os.path.join(self.dynamo_home, d)
                paths = _findfiles(protodir, ('.h','.hpp', '.hxx', '.idl'))
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            self._add_files_to_zip(zip, [
                os.path.join(self.dynamo_home, 'ext/include/glfw/glfw3.h'),
                os.path.join(self.dynamo_home, 'ext/include/glfw/glfw3native.h')
            ], self.dynamo_home, topfolder)

            # C# files
            for d in ['sdk/cs']:
                protodir = os.path.join(self.dynamo_home, d)
                paths = _findfiles(protodir, ('.csproj','.cs'))
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # pipeline tools
            if platform in SDK_PIPELINE_TOOL_PLATFORMS:
                # protoc
                protoc = os.path.join(self.dynamo_home, 'ext/bin/%s/protoc' % platform)
                ddfc_py = os.path.join(self.dynamo_home, 'bin/ddfc.py')
                ddfc_cxx = os.path.join(self.dynamo_home, 'bin/ddfc_cxx')
                ddfc_cxx_bat = os.path.join(self.dynamo_home, 'bin/ddfc_cxx.bat')
                ddfc_java = os.path.join(self.dynamo_home, 'bin/ddfc_java')

                # protoc plugin (ddfc.py) needs our dlib_shared too
                plugin_pb2 = os.path.join(self.dynamo_home, 'lib/python/plugin_pb2.py')
                ddf_init = os.path.join(self.dynamo_home, 'lib/python/ddf/__init__.py')
                ddf_extensions_pb2 = os.path.join(self.dynamo_home, 'lib/python/ddf/ddf_extensions_pb2.py')
                ddf_math_pb2 = os.path.join(self.dynamo_home, 'lib/python/ddf/ddf_math_pb2.py')
                dlib_init = os.path.join(self.dynamo_home, 'lib/python/dlib/__init__.py')

                self._add_files_to_zip(zip, [protoc, ddfc_py, ddfc_java, ddfc_cxx, ddfc_cxx_bat, plugin_pb2, ddf_init, ddf_extensions_pb2, ddf_math_pb2, dlib_init], self.dynamo_home, topfolder)

                # workaround for extender running on x86_64-darwin still:
                if platform == 'x86_64-macos':
                    protoc_src = os.path.join(self.dynamo_home, 'ext/bin/%s/protoc' % platform)
                    protoc_dst = '%s/ext/bin/%s/protoc' % (topfolder, "x86_64-darwin")
                    self._add_file_to_zip(zip, protoc_src, protoc_dst)

                # we don't want to run "pip install" on individual sdk files, so we copy the python files as-is
                protobuf_files = []
                for root, dirs, files in os.walk(os.path.join(self.dynamo_home, 'ext/lib/python/google')):
                    for f in files:
                        _, ext = os.path.splitext(f)
                        print (root, f)
                        if ext in ('.pyc',):
                            continue
                        path = os.path.join(root, f)
                        protobuf_files.append(path)

                if not protobuf_files:
                    raise Exception("Failed to find python protobuf folder")

                self._add_files_to_zip(zip, protobuf_files, self.dynamo_home, topfolder)

                # bob pipeline classes include only in one sdk
                if platform in ('x86_64-linux'):
                    bob_light = os.path.join(self.dynamo_home, 'share/java/bob-light.jar')
                    self._add_files_to_zip(zip, [bob_light], self.dynamo_home, topfolder)


            # For logging, print all paths in zip:
            for x in zip.namelist():
                print(x)

            zip.close()

            sig_filename = self._create_sha256_signature_file(sdk_archive_path)
            return outfile.name, sig_filename
        return None, None

    def build_platform_sdk(self):
        # Helper function to make it easier to build a platform sdk locally
        try:
            path, sig_path = self._package_platform_sdk(self.target_platform)
        except Exception as e:
            print ("Failed to package sdk for platform %s: %s" % (self.target_platform, e))
        else:
            print ("Wrote %s, %s" % (path, sig_path))

    def generate_global_compile_commands_json(self):
        # Generates a "global" compile_commands.json file in the root directory that can be
        # used for example by EasyClangComplete in Sublime Text to get better code completion.
        #
        # Since the engine is built up using sub projects/libs, we generate compile_commands.json
        # files for each of these libraries during a regular build, and collect them and concat
        # them into one big "general"/project wide file here instead.
        #
        # Format of the compile_commands.json file is:
        # >  [
        # >     {
        # >       "file": <file that would be compiled>
        # >       "command": <compile command would be used on the file>,
        # >       "directory": <build directory>,
        # >     },
        # >  ]
        #
        # The method to concat them all is quite simple but seems to work just fine;
        #   - loop over engine library directories and find the compile_commands.json
        #     file in the build subdir, that should have been generated during build_engine
        #   - take all the contents of the file except the starting and ending square brackets
        #     and copy it over into the output json
        #

        self._log("Generating global compile_commands.json")

        # Put the output json in the defold root since its where EasyClangComplete would look for it
        output_path = os.path.join(self.defold_root, 'compile_commands.json')

        result_config = []
        # We loop over engine/<subdirs> and look for engine/<subdir>/build/compile_commands.json
        engine_path = os.path.join(self.defold_root, 'engine')
        for engine_subpath in os.listdir(engine_path):
            potential_json_path = os.path.join(engine_path, engine_subpath, "build", "compile_commands.json")

            if os.path.exists(potential_json_path):
                self._log("Adding %s" % potential_json_path)

                with open(potential_json_path, 'r') as input_file:
                    sub_config = json.load(input_file)
                    for elem in sub_config:
                        result_config.append(elem)
                    input_file.close()
        with open(output_path, 'w') as output_file:
            json.dump(result_config, output_file)
            output_file.close()

    def build_builtins(self):
        with open(join(self.dynamo_home, 'share', 'builtins.zip'), 'wb') as f:
            self._ziptree(join(self.dynamo_home, 'content', 'builtins'), outfile = f, directory = join(self.dynamo_home, 'content'))

    def _strip_engine(self, path):
        """ Strips the debug symbols from an executable """
        if self.target_platform not in ['x86_64-linux','arm64-linux','x86_64-macos','arm64-macos','arm64-ios','x86_64-ios','armv7-android','arm64-android']:
            return False

        sdkfolder = join(self.ext, 'SDKs')
        sdk_info = self.sdk_info if self.sdk_info else sdk.get_sdk_info(sdkfolder, self.target_platform, self.verbose)
        strip = sdk.get_strip_executable(self.target_platform, sdk_info)

        run.shell_command("%s %s" % (strip, path))
        return True

    def archive_engine(self):
        sha1 = self._git_sha1()
        full_archive_path = join(sha1, 'engine', self.target_platform).replace('\\', '/')
        share_archive_path = join(sha1, 'engine', 'share').replace('\\', '/')
        java_archive_path = join(sha1, 'engine', 'share', 'java').replace('\\', '/')
        dynamo_home = self.dynamo_home
        self.full_archive_path = full_archive_path

        artifact_platform = self._engine_artifact_platform(self.target_platform)
        if artifact_platform == self.target_platform:
            bin_dir = self.build_utility.get_binary_path()
            lib_dir = self.build_utility.get_library_path()
        else:
            bin_dir = join(dynamo_home, 'bin', artifact_platform)
            lib_dir = join(dynamo_home, 'lib', artifact_platform)

        # upload editor 2.0 launcher
        if self.target_platform in ['x86_64-linux', 'arm64-linux', 'x86_64-macos', 'arm64-macos', 'x86_64-win32']:
            launcher_name = format_exes("launcher", self.target_platform)[0]
            launcherbin = join(bin_dir, launcher_name)
            self.upload_to_archive(launcherbin, '%s/%s' % (full_archive_path, launcher_name))

        # upload gdc tool on desktop platforms
        if self.is_desktop_target():
            gdc_name = format_exes("gdc", self.target_platform)[0]
            gdc_bin = join(bin_dir, gdc_name)
            gdc_target_name = format_exes("gdc_" + self.target_platform.replace('-', '_'), self.target_platform)[0]
            self.upload_to_archive(gdc_bin, '%s/%s' % (full_archive_path, gdc_target_name))

        # upload mouse_capture lib on desktop platforms
        if self.target_platform in ['x86_64-linux', 'x86_64-macos', 'arm64-macos', 'x86_64-win32']:
            mouse_capture_name = format_lib("mouse_capture_shared", self.target_platform)
            mouse_capture_lib = join(lib_dir, mouse_capture_name)
            self._log(mouse_capture_lib)
            self._log('%s/%s' % (full_archive_path, mouse_capture_name))
            self.upload_to_archive(mouse_capture_lib, '%s/%s' % (full_archive_path, mouse_capture_name))

        if self.is_desktop_target():
            fontc_name = format_lib("fontc_shared", self.target_platform)
            fontc_lib = join(lib_dir, fontc_name)
            self.upload_to_archive(fontc_lib, '%s/%s' % (full_archive_path, fontc_name))

        for n in ['dmengine', 'dmengine_release', 'dmengine_headless']:
            for engine_name in format_exes(n, self.target_platform):
                engine = join(bin_dir, engine_name)
                self.upload_to_archive(engine, '%s/%s' % (full_archive_path, engine_name))
                engine_stripped = join(bin_dir, engine_name + "_stripped")
                shutil.copy2(engine, engine_stripped)
                if self._strip_engine(engine_stripped):
                    self.upload_to_archive(engine_stripped, '%s/stripped/%s' % (full_archive_path, engine_name))
                if self.target_platform in ['win32', 'x86_64-win32', 'x86_64-xbone']:
                    pdb = join(bin_dir, os.path.splitext(engine_name)[0] + '.pdb')
                    self.upload_to_archive(pdb, '%s/%s' % (full_archive_path, os.path.basename(pdb)))
                if 'web' in self.target_platform:
                    engine_mem = join(bin_dir, engine_name + '.mem')
                    if os.path.exists(engine_mem):
                        self.upload_to_archive(engine_mem, '%s/%s.mem' % (full_archive_path, engine_name))
                    engine_symbols = join(bin_dir, engine_name + '.symbols')
                    if os.path.exists(engine_symbols):
                        self.upload_to_archive(engine_symbols, '%s/%s.symbols' % (full_archive_path, engine_name))
                    engine_dwarf = join(bin_dir, engine_name + '.debug.wasm')
                    if os.path.exists(engine_dwarf):
                        self.upload_to_archive(engine_dwarf, '%s/%s.debug.wasm' % (full_archive_path, engine_name))
                elif 'macos' in self.target_platform or 'ios' in self.target_platform:
                    engine_symbols = join(bin_dir, engine_name + '.dSYM.zip')
                    if os.path.exists(engine_symbols):
                        self.upload_to_archive(engine_symbols, '%s/%s' % (full_archive_path, os.path.basename(engine_symbols)))

        zip_archs = []
        if not self.skip_docs:
            zip_archs.append('ref-doc.zip')
        if not self.skip_builtins:
            zip_archs.append('builtins.zip')
        for zip_arch in zip_archs:
            self.upload_to_archive(join(dynamo_home, 'share', zip_arch), '%s/%s' % (share_archive_path, zip_arch))

        if self.target_platform in ['x86_64-linux']:
            # NOTE: It's arbitrary for which platform we archive dlib.jar. Currently set to linux 64-bit
            self.upload_to_archive(join(dynamo_home, 'share', 'java', 'dlib.jar'), '%s/dlib.jar' % (java_archive_path))
            self.upload_to_archive(join(dynamo_home, 'share', 'java', 'modelimporter.jar'), '%s/modelimporter.jar' % (java_archive_path))
            self.upload_to_archive(join(dynamo_home, 'share', 'java', 'fontrenderer.jar'), '%s/fontrenderer.jar' % (java_archive_path))
            self.upload_to_archive(join(dynamo_home, 'share', 'java', 'texturecompiler.jar'), '%s/texturecompiler.jar' % (java_archive_path))
            self.upload_to_archive(join(dynamo_home, 'share', 'java', 'shaderc.jar'), '%s/shaderc.jar' % (java_archive_path))

        if 'android' in self.target_platform:
            files = [
                ('share/java', 'classes.dex'),
                ('ext/share/java', 'android.jar'),
            ]
            for f in files:
                src = join(dynamo_home, f[0], f[1])
                self.upload_to_archive(src, '%s/%s' % (full_archive_path, f[1]))

            resources = self._ziptree(join(dynamo_home, 'ext', 'share', 'java', 'res'), directory = join(dynamo_home, 'ext', 'share', 'java'))
            self.upload_to_archive(resources, '%s/android-resources.zip' % (full_archive_path))

        if self.is_desktop_target():
            libs = ['dlib', 'texc', 'particle', 'modelc', 'shaderc']
            for lib in libs:
                lib_name = format_lib('%s_shared' % (lib), self.target_platform)
                lib_path = join(dynamo_home, 'lib', self.target_platform, lib_name)
                self.upload_to_archive(lib_path, '%s/%s' % (full_archive_path, lib_name))

        sdkpath, sdk_sig_path = self._package_platform_sdk(self.target_platform)
        self.upload_to_archive(sdkpath, '%s/defoldsdk.zip' % full_archive_path)
        self.upload_to_archive(sdk_sig_path, '%s/defoldsdk.sha256' % full_archive_path)

    def _can_run_tests(self):
        supported_tests = {}
        # E.g. on win64, we can test multiple platforms
        supported_tests['x86_64-win32'] = ['win32', 'x86_64-win32', 'arm64-nx64', 'x86_64-ps4', 'x86_64-ps5']
        supported_tests['x86_64-linux'] = []
        supported_tests['arm64-macos'] = ['x86_64-macos', 'arm64-macos', 'wasm-web', 'wasm_pthread-web']
        supported_tests['x86_64-macos'] = ['x86_64-macos', 'wasm-web', 'wasm_pthread-web']

        if 'android' in self.target_platform:
            can_run_android_tests = build_android.can_run_tests_android(self._log, env = self._form_env(), device = self.test_device)
            if self.test_device and not can_run_android_tests:
                self.fatal("Requested Android test device '%s' is not available" % self.test_device)

            if can_run_android_tests:
                android_tests = ['armv7-android', 'arm64-android']
                supported_tests['x86_64-macos'].extend(android_tests)
                supported_tests['arm64-macos'].extend(android_tests)
                supported_tests['x86_64-linux'].extend(android_tests)
                supported_tests['x86_64-win32'].extend(android_tests)

        if build_ios.is_ios_test_platform(self.target_platform):
            strict_ios_tests = not self.skip_tests and '--skip-build-tests' not in self.waf_options
            try:
                can_run_ios_tests = build_ios.can_run_tests_for_platform(
                    self.target_platform,
                    log_fn=self._log,
                    env=self._form_env(),
                    device=self.test_device,
                    identity=self.ios_identity,
                    mobileprovision=self.ios_mobileprovision,
                    team_id=self.ios_team_id,
                    bundle_id_prefix=self.ios_bundle_id_prefix,
                    strict=strict_ios_tests)
            except build_ios.IOSTestError as e:
                self.fatal(str(e))
            if self.test_device and not can_run_ios_tests:
                self.fatal("Requested iOS test target '%s' is not available" % self.test_device)

            if can_run_ios_tests:
                supported_tests['x86_64-macos'].append(self.target_platform)
                supported_tests['arm64-macos'].append(self.target_platform)

        if self.target_platform == 'x86_64-xbone':
            can_run_xbone_tests = build_private.can_run_tests(self.target_platform, self._log, self._form_env(), self.test_device)
            if self.test_device and not can_run_xbone_tests:
                self.fatal("Requested Xbox test console '%s' is not available" % self.test_device)

            if can_run_xbone_tests:
                supported_tests['x86_64-win32'].append('x86_64-xbone')

        can_run_platform = self.target_platform in supported_tests.get(self.host, []) or self.host == self.target_platform
        if not can_run_platform:
            return False

        if wasm_runner.is_web_platform(self.target_platform):
            if self.skip_tests:
                return False

            runner, errors = self._find_wasm_test_runner()
            if not runner:
                self.fatal(self._format_wasm_runner_error(errors))
            self._log("Found wasm test runner: %s" % runner.description())

        return True

    def _get_build_flags(self):
        supports_tests = self._can_run_tests()
        skip_tests = '--skip-tests' if self.skip_tests or not supports_tests else ''
        codesign = '--codesign' if self.codesign else ''
        disable_ccache = '--disable-ccache' if self.disable_ccache else ''
        generate_compile_commands = '--generate-compile-commands' if self.generate_compile_commands else ''
        return {'skip_tests':skip_tests, 'codesign':codesign, 'disable_ccache':disable_ccache, 'generate_compile_commands':generate_compile_commands, 'prefix':None}

    def get_base_platforms(self):
        # Base platforms is the platforms to build the base libs for.
        # The base libs are the libs needed to build bob, i.e. contains compiler code.

        platform_dependencies = {'x86_64-macos': ['x86_64-macos'],
                                 'arm64-macos': ['arm64-macos'],
                                 'x86_64-linux': [],
                                 'arm64-linux': [],
                                 'x86_64-win32': ['win32']}

        platforms = list(platform_dependencies.get(self.host, [self.host]))

        if not self.host in platforms:
            platforms.append(self.host)

        return platforms

# ------------------------------------------------------------
# Gen source files ->

    def _gen_sdk_source_lib(self, libname, args, cwd, info):
        self._log('Generating source for %s' % libname)
        libargs = args + ['-i', info]
        run.env_command(self._form_env(), libargs, cwd = cwd)

    def gen_sdk_source(self):
        print("Generating source!")
        cmd = self.get_python() + [os.path.normpath(join(self.defold_root, './scripts/dmsdk/gen_sdk.py'))]
        for info in sorted(glob(join(self.defold_root, 'engine/*/sdk_gen.json'))):
            cwd = dirname(info)
            lib = basename(cwd)
            self._gen_sdk_source_lib(lib, cmd, cwd, info)

# <- Gen source files
# ------------------------------------------------------------

    def _build_engine_with_waf(self):
        return '--with-waf' in self.waf_options

    def _waf_forward_options(self):
        return [option for option in self.waf_options if option != '--with-waf']

    def _build_engine_cmd_waf(self, skip_tests, codesign, disable_ccache, generate_compile_commands, prefix, incremental = None):
        prefix = prefix and prefix or self.dynamo_home
        incremental = self.incremental if incremental is None else incremental
        commands = "build install"
        if not incremental:
            commands = "distclean configure " + commands
        return '%s %s/ext/bin/waf --prefix=%s %s %s %s %s %s' % (' '.join(self.get_python()), self.dynamo_home, prefix, skip_tests, codesign, disable_ccache, generate_compile_commands, commands)

    def _has_waf_configure_state(self, cwd):
        return os.path.exists(join(cwd, 'build', 'c4che', '_cache.py'))

    def _build_engine_lib_waf(self, args, lib, platform, skip_tests, directory):
        skip_build_tests = []
        if skip_tests and '--skip-build-tests' not in self.waf_options:
            skip_build_tests.append('--skip-tests')
            skip_build_tests.append('--skip-build-tests')
        cwd = join(self.defold_root, '%s/%s' % (directory, lib))
        waf_args = list(args)
        # Propagate the ``--with-waf`` flag so that waf sees it and can
        # skip the CMake check.  The flag is added only when the build
        # configuration requested a pure‑Waf build.
        if self._build_engine_with_waf():
            waf_args.append('--with-waf')
        if not self._has_waf_configure_state(cwd) and 'configure' not in waf_args and 'build' in waf_args:
            waf_args.insert(waf_args.index('build'), 'configure')
        plf_args = ['--platform=%s' % platform]
        run.env_command(self._form_env(), waf_args + plf_args + self._waf_forward_options() + skip_build_tests, cwd = cwd)

    def _find_cmake_build_type(self, options):
        for x in options:
            if '--opt-level=' in x:
                opt_level = x.replace('--opt-level=', '')
                opt_level = int(opt_level)
                if opt_level < 2:
                    return 'Debug'
                return 'RelWithDebInfo'
        return 'RelWithDebInfo'

    def _cmake_feature_defines(self):
        defines = []
        feature_flags = dict((feature, 'OFF') for feature in _CMAKE_FEATURE_FLAG_MAP.values())
        feature_lists = {}
        index = 0
        while index < len(self.waf_options):
            option = self.waf_options[index]
            if not option.startswith('--with-'):
                feature_option = None
                feature_name = None
                for prefix in _CMAKE_FEATURE_LIST_OPTIONS:
                    if option == prefix and index + 1 < len(self.waf_options):
                        feature_option = prefix
                        feature_name = self.waf_options[index + 1]
                        index += 1
                        break
                    if option.startswith(prefix + '='):
                        feature_option = prefix
                        feature_name = option.split('=', 1)[1]
                        break
                if feature_option and feature_name:
                    feature_lists.setdefault(_CMAKE_FEATURE_LIST_OPTIONS[feature_option], [])
                    if feature_name not in feature_lists[_CMAKE_FEATURE_LIST_OPTIONS[feature_option]]:
                        feature_lists[_CMAKE_FEATURE_LIST_OPTIONS[feature_option]].append(feature_name)
                index += 1
                continue
            feature = _CMAKE_FEATURE_FLAG_MAP.get(option)
            if feature:
                feature_flags[feature] = 'ON'
            else:
                self._log(f"Warning: CMake build currently ignores '{option}'")
            index += 1
        for feature, value in sorted(feature_flags.items()):
            defines.append(f"-D{feature}:BOOL={value}")
        for option in _CMAKE_FEATURE_LIST_OPTIONS.values():
            feature_lists.setdefault(option, [])
        for option, features in feature_lists.items():
            defines.append(f"-D{option}:STRING={';'.join(features)}")
        return defines

    def _cmake_target_platform(self, platform):
        if platform == 'win32':
            return 'x86-win32'
        return platform

    def _platform_build_home(self, platform):
        return self.defold_root

    def _engine_artifact_platform(self, platform):
        # Waf still writes 32-bit Windows artifacts to win32; CMake uses the
        # explicit arch tuple x86-win32 while archive/package names stay win32.
        if self._build_engine_with_waf():
            return platform
        return self._cmake_target_platform(platform)

    def _cmake_top_build_dir(self, platform):
        build_home = self._platform_build_home(platform)
        return join(build_home, 'engine', 'build', platform)

    def _cmake_configure_inputs_mtime(self, roots):
        watched_files = ('CMakeLists.txt',)
        watched_suffixes = ('.cmake',)
        watched_dirs = ('scripts/cmake', 'engine', 'share')
        skipped_dirs = {'.git', '.gradle', '__pycache__', 'build', 'tmp', 'node_modules'}
        latest = 0

        for root in roots:
            if not root or not os.path.isdir(root):
                continue
            for watched_dir in watched_dirs:
                path = join(root, watched_dir)
                if not os.path.isdir(path):
                    continue
                for dirpath, dirnames, filenames in os.walk(path):
                    dirnames[:] = [d for d in dirnames if d not in skipped_dirs]
                    for filename in filenames:
                        if filename in watched_files or filename.endswith(watched_suffixes):
                            try:
                                latest = max(latest, os.path.getmtime(join(dirpath, filename)))
                            except OSError:
                                pass
            top_cmake = join(root, 'CMakeLists.txt')
            if os.path.exists(top_cmake):
                try:
                    latest = max(latest, os.path.getmtime(top_cmake))
                except OSError:
                    pass

        return latest

    def _cmake_configure_state(self, builddir, cmake_configure_args):
        defines = {}
        for arg in cmake_configure_args:
            if not arg.startswith('-D'):
                continue
            key_value = arg[2:]
            if '=' not in key_value:
                continue
            key, value = key_value.split('=', 1)
            key = key.split(':', 1)[0]
            defines[key] = value

        configure_roots = [self.defold_root]
        build_home = defines.get('DEFOLD_BUILD_HOME')
        if build_home and normpath(build_home) != normpath(self.defold_root):
            configure_roots.append(build_home)
        platform = defines.get('TARGET_PLATFORM')
        private_root = get_platform_root(platform) if platform else ''
        if private_root and normpath(private_root) != normpath(self.defold_root):
            configure_roots.append(private_root)

        return {
            'args': cmake_configure_args,
            'builddir': normpath(builddir),
            'cmake_inputs_mtime': self._cmake_configure_inputs_mtime(configure_roots),
            'defold_root': normpath(self.defold_root),
            'dynamo_home': normpath(self.dynamo_home),
            'defines': defines
        }

    def _cmake_configure_state_matches(self, current, previous, allow_compatible_configure):
        """
        Returns True when the existing CMake build directory was configured with
        the same effective inputs.

        The compatibility mode exists for same-platform builds where the host
        tool pass runs before the full engine pass. A previous "all" configure
        is valid for a later "host" request because the full build graph already
        contains the host targets. The opposite direction is intentionally not
        accepted, since a host-only build graph may be missing engine targets.
        """
        if not previous:
            return False
        if current == previous:
            return True
        if not allow_compatible_configure:
            return False

        if current.get('builddir') != previous.get('builddir'):
            return False
        if current.get('defold_root') != previous.get('defold_root'):
            return False
        if current.get('dynamo_home') != previous.get('dynamo_home'):
            return False
        if current.get('cmake_inputs_mtime') != previous.get('cmake_inputs_mtime'):
            return False

        current_defines = dict(current.get('defines', {}))
        previous_defines = dict(previous.get('defines', {}))
        current_lib_set = current_defines.pop('DEFOLD_ENGINE_LIB_SET', None)
        previous_lib_set = previous_defines.pop('DEFOLD_ENGINE_LIB_SET', None)
        current_build_tests = current_defines.pop('BUILD_TESTS', None)
        previous_build_tests = previous_defines.pop('BUILD_TESTS', None)

        if current_defines != previous_defines:
            return False
        if current_lib_set != 'host' or previous_lib_set != 'all':
            return False
        if current_build_tests != 'OFF':
            return False

        # Keep test install rules consistent with the requested host pass.
        return previous_build_tests == current_build_tests

    def _cmake_cache_matches_configure_state(self, cmake_cache, configure_state):
        if not os.path.exists(cmake_cache):
            return False

        cache_values = {}
        with open(cmake_cache, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('//') or line.startswith('#') or '=' not in line:
                    continue
                key_type, value = line.split('=', 1)
                key = key_type.split(':', 1)[0]
                cache_values[key] = value

        defines = configure_state.get('defines', {})
        for key in ('CMAKE_BUILD_TYPE',
                    'CMAKE_INSTALL_PREFIX',
                    'CMAKE_GENERATOR_INSTANCE',
                    'CMAKE_SYSTEM_VERSION',
                    'TARGET_PLATFORM',
                    'BUILD_TESTS',
                    'DEFOLD_ENGINE_LIB_SET',
                    'DEFOLD_BUILD_HOME',
                    'DEFOLD_SDK_ROOT',
                    'DEFOLD_TEST_COLORS',
                    'DEFOLD_VISUAL_STUDIO_ROOT',
                    'DEFOLD_WINDOWS_SDK_VERSION',
                    'DEFOLD_SKIP_BOB_LIGHT'):
            expected = defines.get(key)
            if expected is None:
                continue

            actual = cache_values.get(key)
            if key in ('CMAKE_INSTALL_PREFIX', 'CMAKE_GENERATOR_INSTANCE', 'DEFOLD_BUILD_HOME', 'DEFOLD_SDK_ROOT', 'DEFOLD_VISUAL_STUDIO_ROOT'):
                actual = normpath(actual) if actual else actual
                expected = normpath(expected) if expected else expected
            if actual != expected:
                self._log(f'CMake cache mismatch for {key}: expected {expected}, got {actual}')
                return False

        return True

    def _cmake_generated_install_matches_configure_state(self, configure_state):
        args = configure_state.get('args', [])
        generator = ''
        for i, arg in enumerate(args):
            if arg == '-G' and i + 1 < len(args):
                generator = args[i + 1]
            elif arg.startswith('-G') and len(arg) > 2:
                generator = arg[2:]

        # The top-level engine build uses Ninja. If a per-library binary
        # directory was previously generated by a multi-config IDE solution,
        # its cmake_install.cmake can still point at Debug/RelWithDebInfo
        # subdirectories while Ninja produces libraries directly in the binary
        # directory. Detect that stale generated state before skipping configure.
        if generator != 'Ninja':
            return True

        defines = configure_state.get('defines', {})
        build_home = defines.get('DEFOLD_BUILD_HOME')
        platform = defines.get('TARGET_PLATFORM')
        build_type = defines.get('CMAKE_BUILD_TYPE')
        build_tests = defines.get('BUILD_TESTS')
        if not build_home or not platform:
            return True

        engine_root = join(build_home, 'engine')
        if not os.path.isdir(engine_root):
            return True

        stale_config_dirs = ['/build/%s/%s/' % (platform, config) for config in ('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
        for lib in os.listdir(engine_root):
            install_script = join(engine_root, lib, 'build', platform, 'cmake_install.cmake')
            if not os.path.exists(install_script):
                continue
            try:
                with open(install_script, 'r') as f:
                    content = f.read().replace('\\', '/')
            except OSError:
                continue

            stale_config_dir = next((path for path in stale_config_dirs if path in content), None)
            if stale_config_dir:
                self._log('CMake generated install mismatch for %s: stale multi-config output path found' % install_script)
                return False
            if build_tests == 'OFF' and '/src/test/cmake_install.cmake' in content:
                self._log('CMake generated install mismatch for %s: stale test install rules found' % install_script)
                return False

        return True

    def _cmake_generated_outputs_match_configure_state(self, configure_state):
        args = configure_state.get('args', [])
        generator = ''
        for i, arg in enumerate(args):
            if arg == '-G' and i + 1 < len(args):
                generator = args[i + 1]
            elif arg.startswith('-G') and len(arg) > 2:
                generator = arg[2:]

        if generator != 'Ninja':
            return True

        defines = configure_state.get('defines', {})
        build_home = defines.get('DEFOLD_BUILD_HOME')
        platform = defines.get('TARGET_PLATFORM')
        if not build_home or not platform:
            return True

        build_ninja = join(configure_state.get('builddir', ''), 'build.ninja')
        if not os.path.exists(build_ninja):
            return True

        try:
            with open(build_ninja, 'r') as f:
                content = f.read().replace('\\', '/')
        except OSError:
            return True

        expected_engine_root = normpath(join(build_home, 'engine')).replace('\\', '/').lower()
        generated_build_dir_re = re.compile(r'[A-Za-z]\$?:/[^ \t\r\n"<>|]*/engine/[^/\s]+/build/%s' % re.escape(platform))
        for match in generated_build_dir_re.finditer(content):
            path = match.group(0).replace('$:', ':')
            normalized_path = normpath(path).replace('\\', '/').lower()
            if not normalized_path.startswith(expected_engine_root + '/'):
                self._log('CMake generated output mismatch: stale generated build path found: %s' % path)
                return False

        return True

    def _remove_cmake_build_dirs_for_platform(self, build_home, platform, top_builddir):
        self._remove_tree(top_builddir)

        engine_root = join(build_home, 'engine')
        if os.path.isdir(engine_root):
            for lib in os.listdir(engine_root):
                self._remove_tree(join(engine_root, lib, 'build', platform))

        self._remove_tree(join(build_home, 'share', 'extender', 'build', platform))

    def _build_engine_libs_cmake(self, name, lib_set, platform, skip_tests = False, reuse_builddir = False, allow_compatible_configure = False, use_existing_bob_light = False):
        platform = self._cmake_target_platform(platform)
        build_home = self._platform_build_home(platform)
        builddir = self._cmake_top_build_dir(platform)

        build_type = self._find_cmake_build_type(self.waf_options)
        build_tests = (not skip_tests) and '--skip-build-tests' not in self.waf_options and self._can_run_tests()
        supports_tests = build_tests

        # Keep CMake build directories persistent so repeated builds can be
        # handled by Ninja's dependency graph instead of forcing a rebuild.

        if not os.path.exists(builddir):
            os.makedirs(builddir)

        is_verbose = self.verbose or ('-v' in self.waf_options) or ('--verbose' in self.waf_options)
        test = '' if (self.skip_tests or not supports_tests) else 'run_tests'
        build_test = 'build_tests' if build_tests else ''
        cmake_build_tests = 'ON' if build_tests else 'OFF'
        self._log(f"CMake lib set for {name}: {lib_set}")

        trace = '' #'--trace-expand'

        # ***************************************************************************************
        # generate the build script
        log_cmd_config = f'CMake configure {name}'
        self.build_tracker.start_command(log_cmd_config)

        run_tests = 'ON' if test else 'OFF'
        self._log(f'CMake settings for {name}: platform={platform}, build_type={build_type}, build_tests={cmake_build_tests}, run_tests={run_tests}')
        cmake_configure_args = ['cmake', '-S', self.defold_root, '-B', builddir, '-GNinja']
        if trace:
            cmake_configure_args.append(trace)
        cmake_configure_args += [
            f'-DCMAKE_BUILD_TYPE={build_type}',
            f'-DTARGET_PLATFORM={platform}',
            f'-DBUILD_TESTS={cmake_build_tests}',
            f'-DDEFOLD_ENGINE_LIB_SET={lib_set}',
            f'-DDEFOLD_BUILD_HOME:PATH={build_home}',
            f'-DDEFOLD_SDK_ROOT:PATH={self.dynamo_home}',
            f'-DCMAKE_INSTALL_PREFIX:PATH={self.dynamo_home}',
            f'-DDEFOLD_SKIP_BOB_LIGHT:BOOL={"ON" if (self.skip_bob_light or use_existing_bob_light) else "OFF"}',
            f'-DDEFOLD_TEST_COLORS:BOOL={"OFF" if self.no_colors else "ON"}',
            f'-DDEFOLD_CODESIGN:BOOL={"ON" if self.codesign else "OFF"}',
            f'-DDEFOLD_CODESIGNING_IDENTITY:STRING={self.codesigning_identity or ""}',
            f'-DDEFOLD_GCLOUD_PROJECTID:STRING={self.gcloud_projectid or ""}',
            f'-DDEFOLD_GCLOUD_LOCATION:STRING={self.gcloud_location or ""}',
            f'-DDEFOLD_GCLOUD_KEYRINGNAME:STRING={self.gcloud_keyringname or ""}',
            f'-DDEFOLD_GCLOUD_KEYNAME:STRING={self.gcloud_keyname or ""}',
            f'-DDEFOLD_GCLOUD_CERTFILE:STRING={self.gcloud_certfile or ""}',
            f'-DDEFOLD_GCLOUD_KEYFILE:STRING={self.gcloud_keyfile or ""}'
        ]
        cmake_configure_args += self._cmake_feature_defines()
        cmake_configure_state = self._cmake_configure_state(builddir, cmake_configure_args)
        cmake_configure_state_path = join(builddir, '.defold_cmake_configure.json')
        previous_cmake_configure_state = None
        if os.path.exists(cmake_configure_state_path):
            try:
                with open(cmake_configure_state_path, 'r') as f:
                    previous_cmake_configure_state = json.load(f)
            except Exception:
                pass

        cmake_cache = join(builddir, 'CMakeCache.txt')
        generated_outputs_match = self._cmake_generated_outputs_match_configure_state(cmake_configure_state)
        if not generated_outputs_match:
            self._log(f'Removing stale CMake build outputs for {platform}; generated files point outside {build_home}')
            self._remove_cmake_build_dirs_for_platform(build_home, platform, builddir)
            os.makedirs(builddir, exist_ok=True)
            previous_cmake_configure_state = None

        skip_configure = (
            os.path.exists(cmake_cache)
            and self._cmake_configure_state_matches(cmake_configure_state, previous_cmake_configure_state, allow_compatible_configure)
            and self._cmake_cache_matches_configure_state(cmake_cache, cmake_configure_state)
            and self._cmake_generated_install_matches_configure_state(cmake_configure_state)
            and generated_outputs_match)
        if skip_configure:
            self._log(f'Skipping CMake configure {name}; configure state is unchanged')
        else:
            run.env_command(self._form_env(), cmake_configure_args, cwd = self.defold_root)
            with open(cmake_configure_state_path, 'w') as f:
                json.dump(cmake_configure_state, f, indent = 2, sort_keys = True)

        self.build_tracker.end_command(log_cmd_config)

        # ***************************************************************************************
        # execute the build
        log_cmd_build = f'CMake build {name} {build_test}'
        self.build_tracker.start_command(log_cmd_build)

        cmake_build_args = ['cmake', '--build', builddir, '--target', 'all']
        if build_test:
            cmake_build_args.append(build_test)
        if is_verbose:
            cmake_build_args.append('--verbose')
        run.env_command(self._form_env(), cmake_build_args, cwd = self.defold_root)

        self.build_tracker.end_command(log_cmd_build)

        # Keep install as a separate phase. Use cmake --install instead of the
        # generated install target so the install phase does not re-enter 'all'.
        log_cmd_install = f'CMake install {name}'
        self.build_tracker.start_command(log_cmd_install)

        cmake_install_args = ['cmake', '--install', builddir, '--config', build_type]
        if is_verbose:
            cmake_install_args.append('--verbose')
        run.env_command(self._form_env(), cmake_install_args, cwd = self.defold_root)

        self.build_tracker.end_command(log_cmd_install)

        # ***************************************************************************************
        # run the build
        if test:
            log_cmd_tests = f'CMake run_tests {name}'
            self.build_tracker.start_command(log_cmd_tests)

            cmake_test_args = ['cmake', '--build', builddir, '--target', 'run_tests']
            if is_verbose:
                cmake_test_args.append('--verbose')
            run.env_command(self._form_env(), cmake_test_args, cwd = self.defold_root)

            self.build_tracker.end_command(log_cmd_tests)

    def _build_engine_lib(self, args, lib, platform, skip_tests = False, directory = 'engine'):
        self.build_tracker.start_component(lib, platform)

        if lib in CMAKE_SUPPORT:
            if platform == 'win32':
                platform = 'x86-win32'
            self._build_engine_lib_cmake(lib, platform, skip_tests, directory)
        else:
            self._build_engine_lib_waf(args, lib, platform, skip_tests, directory)

        self.build_tracker.end_component(lib, platform)

# For now gradle right in
# - 'com.dynamo.cr/com.dynamo.cr.bob'
# - 'com.dynamo.cr/com.dynamo.cr.test'
# Maybe in the future we consider to move it into install_ext
    def get_gradle_wrapper(self):
        if os.name == 'nt':  # Windows
            return join('.', 'gradlew.bat')
        else:  # Linux, macOS, or other Unix-like OS
            return join('.', 'gradlew')

    def build_bob_plugins(self):
        gradle = join('..', 'com.dynamo.cr.bob', os.name == 'nt' and 'gradlew.bat' or 'gradlew')
        gradle_args = []
        if self.verbose:
            gradle_args += ['--info']

        env = self._form_env()
        env['GRADLE_OPTS'] = f'-Dorg.gradle.parallel=true {JAVA_RUNTIME_FLAGS}'

        for plugin_name in ('xbox', 'switch', 'playstation'):
            plugin_dir = join(self.defold_root, 'com.dynamo.cr', 'com.dynamo.cr.%s' % plugin_name)
            if not os.path.isdir(plugin_dir):
                continue

            self.build_tracker.start_component('bob_plugin_%s' % plugin_name, self.host)

            s = run.command(" ".join([gradle, 'clean', 'install'] + gradle_args), cwd=plugin_dir, shell=True, env=env)
            if self.verbose:
                print(s)

            self.build_tracker.end_component('bob_plugin_%s' % plugin_name, self.host)

    def _run_bob_copy_script(self):
        """Run com.dynamo.cr.bob/scripts/copy.sh via POSIX sh.

        Use sh (not bash): on Windows, `bash` in PATH is often WSL's stub (no distro).
        Git for Windows provides sh.exe. Avoid shell=True so cmd.exe is not used."""
        bob_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        run.env_command(self._form_env(), ['sh', 'scripts/copy.sh'], cwd=bob_dir)

    def build_bob_light(self):
        self.build_tracker.start_component('bob_light', self.host)
        log_cmd_build = 'Gradle build bob_light'

        try:
            bob_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')

            env = self._form_env()

            gradle = self.get_gradle_wrapper()
            gradle_args = ['-Ptarget-platform=%s' % self.target_platform]
            if self.verbose:
                gradle_args += ['--info']

            env['GRADLE_OPTS'] = f'-Dorg.gradle.parallel=true {JAVA_RUNTIME_FLAGS}' #-Dorg.gradle.daemon=true

            self.build_tracker.start_command(log_cmd_build)
            try:
                s = run.command(" ".join([gradle, '-Pkeep-bob-uncompressed'] + gradle_args + ['installBobLight']), cwd = bob_dir, shell = True, env = env)
            finally:
                self.build_tracker.end_command(log_cmd_build)
            if self.verbose:
                print (s)
            self.build_bob_plugins()
        finally:
            self.build_tracker.end_component('bob_light', self.host)

    def build_engine(self):
        self.check_sdk()

        # We want random folder to thoroughly test bob-light
        # We dont' want it to unpack for _every_ single invocation during the build
        os.environ['DM_BOB_ROOTFOLDER'] = tempfile.mkdtemp(prefix='bob-light-')
        self._log("env DM_BOB_ROOTFOLDER=" + os.environ['DM_BOB_ROOTFOLDER'])

        host = self.host
        target_platform = self.target_platform
        with_waf = self._build_engine_with_waf()
        if with_waf:
            cmd = self._build_engine_cmd_waf(**self._get_build_flags(), incremental = self.incremental)
            args = cmd.split()
            self._log('Building engine libs with Waf fallback (--with-waf)')
        else:
            args = []
            self._log('Building engine libs with top-level CMake (incremental by default)')

        # Make sure we build these for the host platform for the toolchain (bob light)
        if with_waf:
            host_lib_skip_tests = host != target_platform
            for lib in HOST_LIBS:
                self._build_engine_lib(args, lib, host, skip_tests = host_lib_skip_tests)
        else:
            self.build_tracker.start_component('cmake_host_libs', host)
            self._build_engine_libs_cmake('host_libs', 'host', host, skip_tests = True, allow_compatible_configure = host == target_platform)
            self.build_tracker.end_component('cmake_host_libs', host)

        if not self.skip_bob_light:
            # We must build bob-light, which builds content during the engine build
            self.build_bob_light()

        if with_waf:
            for lib in ENGINE_LIBS:
                if host == target_platform and lib in HOST_LIBS:
                    continue
                if not build_private.is_library_supported(target_platform, lib):
                    continue
                self._build_engine_lib(args, lib, target_platform)
        else:
            reuse_builddir = host == target_platform
            target_lib_set = 'all' if reuse_builddir else 'target'
            self.build_tracker.start_component('cmake_engine_libs', target_platform)
            self._build_engine_libs_cmake('engine_libs', target_lib_set, target_platform, reuse_builddir = reuse_builddir, use_existing_bob_light = True)
            self.build_tracker.end_component('cmake_engine_libs', target_platform)

        if with_waf:
            self._build_engine_lib(args, 'extender', target_platform, directory = 'share')
        if not self.skip_docs:
            self.build_docs(incremental = True)
        if not self.skip_builtins:
            self.build_builtins()
        if self.generate_compile_commands:
            self.generate_global_compile_commands_json()
        if '--static-analyze' in self.waf_options:
            scan_output_dir = os.path.normpath(os.path.join(os.environ['DYNAMO_HOME'], '..', '..', 'static_analyze'))
            report_dir = os.path.normpath(os.path.join(os.environ['DYNAMO_HOME'], '..', '..', 'report'))
            run.command(self.get_python() + ['./scripts/scan_build_gather_report.py', '-o', report_dir, '-i', scan_output_dir])
            print("Wrote report to %s. Open with 'scan-view .' or 'python -m SimpleHTTPServer'" % report_dir)
            shutil.rmtree(scan_output_dir)

        self._log("Write platform.sdks.json")
        write_merged_platform_sdks(self.defold_root, self.target_platform, join(self.dynamo_home, "platform.sdks.json"))

        if os.path.exists(os.environ['DM_BOB_ROOTFOLDER']):
            print ("Removing", os.environ['DM_BOB_ROOTFOLDER'])
            shutil.rmtree(os.environ['DM_BOB_ROOTFOLDER'])

    def build_external(self):
        libs = EXTERNAL_LIBS
        if self.external_package:
            if self.external_package not in EXTERNAL_LIBS:
                self.fatal("Unknown external package '%s'. Expected one of: %s" % (self.external_package, ', '.join(EXTERNAL_LIBS)))
            libs = [self.external_package]

        waf_libs = [lib for lib in libs if lib in EXTERNAL_WAF_LIBS]
        if waf_libs:
            flags = self._get_build_flags()
            flags['prefix'] = join(self.defold_root, 'packages')
            cmd = self._build_engine_cmd_waf(**flags)
            args = cmd.split() + ['package']
            for lib in waf_libs:
                self._build_engine_lib(args, lib, platform=self.target_platform, directory='external')

        for lib in [lib for lib in libs if lib in EXTERNAL_CMAKE_LIBS]:
            if lib == 'vkquality' and self.target_platform not in ('armv7-android', 'arm64-android') and not self.external_package:
                self._log("Skipping vkquality for non-Android platform: %s" % self.target_platform)
                continue
            self._build_external_lib_cmake(lib, self.target_platform)

    def _build_external_lib_cmake(self, lib, platform):
        version = EXTERNAL_PACKAGE_VERSIONS[lib]
        package_name = '%s-%s' % (lib, version)
        source_dir = join(self.defold_root, 'external', lib)
        build_dir = join(source_dir, 'build', platform)
        install_dir = join(self.dynamo_home, package_name)
        package_dir = join(self.defold_root, 'packages')
        package_path = join(package_dir, '%s-%s.tar.gz' % (package_name, platform))

        if not os.path.exists(join(source_dir, 'CMakeLists.txt')):
            self.fatal("CMake external package '%s' is missing CMakeLists.txt" % lib)

        if os.path.exists(install_dir):
            shutil.rmtree(install_dir)
        os.makedirs(build_dir, exist_ok=True)
        os.makedirs(package_dir, exist_ok=True)

        build_type = self._find_cmake_build_type(self.waf_options)
        configure_args = [
            'cmake',
            '-S', source_dir,
            '-B', build_dir,
            '-GNinja',
            '-DCMAKE_BUILD_TYPE=%s' % build_type,
            '-DTARGET_PLATFORM=%s' % platform,
            '-DDEFOLD_SDK_ROOT=%s' % self.dynamo_home,
            '-DDEFOLD_EXTERNAL_INSTALL_PREFIX=%s' % install_dir,
        ]
        build_args = ['cmake', '--build', build_dir, '--target', 'install']
        if self.verbose or ('-v' in self.waf_options) or ('--verbose' in self.waf_options):
            build_args.append('--verbose')

        self.build_tracker.start_component(lib, platform)
        try:
            self.build_tracker.start_command('CMake configure external %s' % lib)
            try:
                run.env_command(self._form_env(), configure_args, cwd=source_dir)
            finally:
                self.build_tracker.end_command('CMake configure external %s' % lib)

            self.build_tracker.start_command('CMake build external %s' % lib)
            try:
                run.env_command(self._form_env(), build_args, cwd=source_dir)
            finally:
                self.build_tracker.end_command('CMake build external %s' % lib)

            package_command = ['tar', 'zcvf', os.path.normpath(package_path), 'include', 'lib', 'share']
            self.build_tracker.start_command('Package external %s' % lib)
            try:
                run.command(package_command, cwd=install_dir)
            finally:
                self.build_tracker.end_command('Package external %s' % lib)
            print("Installed to", package_path)
        finally:
            self.build_tracker.end_component(lib, platform)

    def archive_bob(self):
        sha1 = self._git_sha1()
        full_archive_path = join(sha1, 'bob').replace('\\', '/')
        for p in glob(join(self.dynamo_home, 'share', 'java', 'bob.jar')):
            self.upload_to_archive(p, '%s/%s' % (full_archive_path, basename(p)))
        for p in glob(join(self.dynamo_home, 'share', 'java', 'plugins', '*.jar')):
            self.upload_to_archive(p, '%s/plugins/%s' % (full_archive_path, basename(p)))

    def copy_local_bob_artefacts(self):
        texc_name = format_lib('texc_shared', self.host)
        modelc_name = format_lib('modelc_shared', self.host)
        fontc_name = format_lib('fontc_shared', self.host)
        shaderc_name = format_lib('shaderc_shared', self.host)
        luajit_dir = tempfile.mkdtemp()
        cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        missing = {}
        def add_missing(plf, txt):
            txts = []
            txts = missing.setdefault(plf, txts)
            txts = txts.append(txt)

        for plf in [['win32', 'x86_64-win32'],
                    ['x86_64-win32', 'x86_64-win32'],
                    ['x86_64-linux', 'x86_64-linux'],
                    ['arm64-linux', 'arm64-linux'],
                    ['x86_64-macos', 'x86_64-macos'],
                    ['arm64-macos', 'arm64-macos']]:
            luajit_package = [pkg for pkg in PLATFORM_PACKAGES[plf[0]] if "luajit" in pkg]
            luajit_path = join(cwd, '../../packages/%s-%s.tar.gz' % (luajit_package[0], plf[0]))
            if not os.path.exists(luajit_path):
                add_missing(plf[1], "package '%s' could not be found" % (luajit_path))
            else:
                self._extract(luajit_path, luajit_dir)
                for name in ('luajit-64'):
                    luajit_exe = format_exes(name, plf[0])[0]
                    src = join(luajit_dir, 'bin/%s/%s' % (plf[0], luajit_exe))
                    if not os.path.exists(src):
                        continue
                    tgt_dir = join(cwd, 'libexec/%s' % plf[1])
                    self._mkdirs(tgt_dir)
                    self._copy(src, join(tgt_dir, luajit_exe))

        # Any shared libraries that we depend on
        win32_files = dict([['ext/lib/%s/%s.dll' % (plf[0], lib), 'lib/%s/%s.dll' % (plf[1], lib)] for lib in [] for plf in [['win32', 'x86-win32'], ['x86_64-win32', 'x86_64-win32']]])
        macos_files = dict([['ext/lib/%s/lib%s.dylib' % (plf[0], lib), 'lib/%s/lib%s.dylib' % (plf[1], lib)] for lib in [] for plf in [['x86_64-macos', 'x86_64-macos'], ['arm64-macos', 'arm64-macos']]])
        linux_files = dict([['ext/lib/%s/lib%s.so' % (plf[0], lib), 'lib/%s/lib%s.so' % (plf[1], lib)] for lib in [] for plf in [['x86_64-linux', 'x86_64-linux'], ['arm64-linux', 'arm64-linux']]])
        js_files = {}
        android_files = {'share/java/classes.dex': 'lib/classes.dex',
                         'ext/share/java/android.jar': 'lib/android.jar', # this should be the stripped one
                         'ext/share/java/vkquality.jar': 'lib/vkquality.jar',
                         'ext/share/vkquality/assets/vkqualitydata.vkq': 'lib/vkquality/vkqualitydata.vkq',
                         'ext/lib/armv7-android/libvkquality.so': 'libexec/armv7-android/libvkquality.so',
                         'ext/lib/arm64-android/libvkquality.so': 'libexec/arm64-android/libvkquality.so'}

        switch_files = {}
        win32_engine_platform = self._engine_artifact_platform('win32')
        # This dict is being built up and will eventually be used for copying in the end
        # - "type" - what the files are needed for, for error reporting
        #   - pairs of src-file -> dst-file
        artefacts = {'generic': {'share/java/dlib.jar': 'lib/dlib.jar',
                                 'share/java/fontrenderer.jar': 'lib/fontrenderer.jar',
                                 'share/java/modelimporter.jar': 'lib/modelimporter.jar',
                                 'share/java/shaderc.jar': 'lib/shaderc.jar',
                                 'share/builtins.zip': 'lib/builtins.zip',
                                 'lib/%s/%s' % (self.host, texc_name): 'lib/%s/%s' % (self.host, texc_name),
                                 'lib/%s/%s' % (self.host, modelc_name): 'lib/%s/%s' % (self.host, modelc_name),
                                 'lib/%s/%s' % (self.host, fontc_name): 'lib/%s/%s' % (self.host, fontc_name),
                                 'lib/%s/%s' % (self.host, shaderc_name): 'lib/%s/%s' % (self.host, shaderc_name)},
                     'android-bundling': android_files,
                     'win32-bundling': win32_files,
                     'web-bundling': js_files,
                     'ios-bundling': {},
                     'osx-bundling': macos_files,
                     'linux-bundling': linux_files,
                     'switch-bundling': switch_files}
        # Add dmengine to 'artefacts' procedurally
        for type, plfs in {'android-bundling': [['armv7-android', 'armv7-android'], ['arm64-android', 'arm64-android']],
                           'win32-bundling': [[win32_engine_platform, 'x86-win32'], ['x86_64-win32', 'x86_64-win32']],
                           'web-bundling': [['wasm-web', 'wasm-web'], ['wasm_pthread-web', 'wasm_pthread-web']],
                           'ios-bundling': [['arm64-ios', 'arm64-ios'], ['x86_64-ios', 'x86_64-ios']],
                           'osx-bundling': [['x86_64-macos', 'x86_64-macos'], ['arm64-macos', 'arm64-macos']],
                           'linux-bundling': [['x86_64-linux', 'x86_64-linux'], ['arm64-linux', 'arm64-linux']],
                           'switch-bundling': [['arm64-nx64', 'arm64-nx64']]}.items():
            # plfs is pairs of src-platform -> dst-platform
            for plf in plfs:
                exes = format_exes('dmengine', plf[1]) + format_exes('dmengine_release', plf[1])
                artefacts[type].update(dict([['bin/%s/%s' % (plf[0], exe), 'libexec/%s/%s' % (plf[1], exe)] for exe in exes]))
        # Perform the actual copy, or list which files are missing
        for type, files in artefacts.items():
            m = []
            for src, dst in files.items():
                src_path = join(self.dynamo_home, src)
                if not os.path.exists(src_path):
                    m.append(src_path)
                else:
                    dst_path = join(cwd, dst)
                    self._mkdirs(os.path.dirname(dst_path))
                    self._copy(src_path, dst_path)
            if m:
                add_missing(type, m)
        if missing:
            print('*** NOTE! There are missing artefacts.')
            print(json.dumps(missing, indent=2))

    def build_bob(self):
        bob_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        test_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')

        sha1 = self._git_sha1()
        self.install_bob_tool_packages()
        self._run_bob_copy_script()
        if not os.path.exists(os.path.join(self.dynamo_home, 'archive', sha1)):
            self.copy_local_bob_artefacts()

        env = self._form_env()

        gradle = self.get_gradle_wrapper()
        gradle_args = ['-Ptarget-platform=%s' % self.target_platform]
        if self.verbose:
            gradle_args += ['--info']

        env['GRADLE_OPTS'] = f'-Dorg.gradle.parallel=true {JAVA_RUNTIME_FLAGS}' #-Dorg.gradle.daemon=true
        flags = []
        if self.keep_bob_uncompressed:
            flags = ['-Pkeep-bob-uncompressed']

        if self.skip_tests:
            # Clean and build the project
            run.command(" ".join([gradle] + flags + gradle_args + ['clean', 'install']), cwd=bob_dir, shell = True, env = env)
        else:
            # Build, install and test Bob in one Gradle graph so shared dependencies such as distBob run only once.
            run.command(" ".join([gradle] + flags + gradle_args + ['clean', 'install', 'testJar']), cwd = test_dir, shell = True, env = env, stdout = None)

        self.build_bob_plugins()

    def test_bob(self):
        bob_jar = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar')
        test_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')

        if not os.path.exists(bob_jar):
            self.fatal("bob.jar is missing. Build bob or download the bob-jar artifact first.")

        env = self._form_env()

        gradle = self.get_gradle_wrapper()
        gradle_args = ['-Ptarget-platform=%s' % self.target_platform]
        if self.verbose:
            gradle_args += ['--info']

        env['GRADLE_OPTS'] = f'-Dorg.gradle.parallel=true {JAVA_RUNTIME_FLAGS}' #-Dorg.gradle.daemon=true

        # compileTest only needs bob.jar on disk. Exclude distBob so this job tests the artifact
        # produced by build-bob instead of rebuilding it.
        run.command(" ".join([gradle] + gradle_args + ['testJar', '-x', 'distBob']), cwd = test_dir, shell = True, env = env, stdout = None)


    def build_sdk_headers(self):
        # Used to provide a small sized bundle with the headers for any C++ auto completion tools

        # Step 1: Generate the package
        filename = 'defoldsdk_headers.zip'
        headers_path = join(self.dynamo_home, filename)
        self._package_platform_sdk_headers(headers_path)

        # Step 2: Upload the package
        sha1 = self._git_sha1()

        sdkurl = join(sha1, 'engine').replace('\\', '/')
        self.upload_to_archive(headers_path, f'{sdkurl}/{filename}')

    def build_sdk(self):
        tempdir = tempfile.mkdtemp() # where the sdk ends up

        sha1 = self._git_sha1()
        u = urlparse(self.get_archive_path())

        root = urlparse(self.get_archive_path()).path[1:]
        base_prefix = os.path.join(root, sha1)

        # When a public checkout has a private platform added, only merge the
        # requested private platform SDK. Public releases still merge all SDKs.
        private_platforms = build_private.get_target_platforms()
        if build_private.is_repo_private():
            platforms = private_platforms
        elif self.target_platform in private_platforms:
            platforms = [self.target_platform]
        else:
            platforms = get_target_platforms()
        print("Building combined SDK from platform SDK archives:", platforms)

        zipmerge_path = shutil.which('zipmerge')
        if zipmerge_path:
            print("Using zipmerge to merge platform SDK archives:", zipmerge_path)
            treepath = os.path.join(tempdir, 'defoldsdk')
            sdkpath = treepath + '.zip'
            sdk_merge.build_combined_sdk_zip(
                netloc=u.netloc,
                base_prefix=base_prefix,
                platforms=platforms,
                output_zip_path=sdkpath,
                zipmerge_path=zipmerge_path,
                canonical_platform='x86_64-linux')
        else:
            print("zipmerge not found; using Python platform SDK archive merge")

            sdk_merge.build_combined_sdk_tree(
                netloc=u.netloc,
                base_prefix=base_prefix,
                platforms=platforms,
                extract_dir=tempdir,
                canonical_platform='x86_64-linux')

            # Due to an issue with how the attributes are preserved, let's go through the bin/ folders
            # and set the flags explicitly
            for root, _, files in os.walk(tempdir):
                for f in files:
                    p = os.path.join(root, f)
                    if '/bin/' in p:
                        os.chmod(p, 0o755)

            treepath = os.path.join(tempdir, 'defoldsdk')
            sdkpath = self._ziptree(treepath, directory=tempdir)

        print ("Packaged defold sdk from", tempdir, "to", sdkpath)

        sdkurl = join(sha1, 'engine').replace('\\', '/')
        self.upload_to_archive(sdkpath, '%s/defoldsdk.zip' % sdkurl)

        print("Create sdk signature")
        sig_filename = self._create_sha256_signature_file(sdkpath)
        self.upload_to_archive(join(dirname(sdkpath), sig_filename), '%s/defoldsdk.sha256' % sdkurl)

        print("Upload platform sdks mappings")
        platform_sdks_path = join(tempdir, 'platform.sdks.json')
        write_merged_platform_sdks(self.defold_root, self.target_platform, platform_sdks_path)
        self.upload_to_archive(platform_sdks_path, '%s/platform.sdks.json' % sdkurl)

        self.wait_uploads()
        shutil.rmtree(tempdir)
        print ("Removed", tempdir)

    def build_docs(self, incremental = None):
        self._log('Building API docs')
        docs_dir = join(self.defold_root, 'engine/docs')
        builddir = join(docs_dir, 'build')
        platform = self._cmake_target_platform(self.target_platform)
        build_type = self._find_cmake_build_type(self.waf_options)
        docs_run_tests = 'OFF' if self.skip_tests or self.target_platform != self.host else 'ON'
        is_verbose = self.verbose or ('-v' in self.waf_options) or ('--verbose' in self.waf_options)

        if incremental is None:
            incremental = True
        if not incremental:
            self._clean_cmake_builddir(builddir)

        self.build_tracker.start_component('build_docs', platform)
        try:
            log_cmd_config = 'CMake configure build_docs'
            self.build_tracker.start_command(log_cmd_config)

            cmake_configure_args = ['cmake', '-S', docs_dir, '-B', builddir, '-GNinja']
            cmake_configure_args += [
                f'-DCMAKE_BUILD_TYPE={build_type}',
                f'-DTARGET_PLATFORM={platform}',
                f'-DDEFOLD_DOCS_RUN_TESTS={docs_run_tests}'
            ]
            cmake_configure_state = self._cmake_configure_state(builddir, cmake_configure_args)
            cmake_configure_state_path = join(builddir, '.defold_cmake_configure.json')
            previous_cmake_configure_state = None
            if os.path.exists(cmake_configure_state_path):
                try:
                    with open(cmake_configure_state_path, 'r') as f:
                        previous_cmake_configure_state = json.load(f)
                except Exception:
                    pass

            cmake_cache = join(builddir, 'CMakeCache.txt')
            skip_configure = os.path.exists(cmake_cache) and self._cmake_configure_state_matches(cmake_configure_state, previous_cmake_configure_state, False)
            if skip_configure:
                self._log('Skipping CMake configure build_docs; configure state is unchanged')
            else:
                run.env_command(self._form_env(), cmake_configure_args, cwd = self.defold_root)
                with open(cmake_configure_state_path, 'w') as f:
                    json.dump(cmake_configure_state, f, indent = 2, sort_keys = True)

            self.build_tracker.end_command(log_cmd_config)

            log_cmd_build = 'CMake build build_docs'
            self.build_tracker.start_command(log_cmd_build)

            cmake_build_args = ['cmake', '--build', builddir, '--target', 'build_docs']
            if is_verbose:
                cmake_build_args.append('--verbose')
            run.env_command(self._form_env(), cmake_build_args, cwd = self.defold_root)

            self.build_tracker.end_command(log_cmd_build)
        finally:
            self.build_tracker.end_component('build_docs', platform)


# ------------------------------------------------------------
# BEGIN: EDITOR 2
#
    def download_editor2(self):
        if not self.channel:
            raise Exception('No channel provided when downloading the editor')

        editor_filename = "Defold-%s.zip" % self.target_platform
        editor_path = join(self.defold_root, 'editor', 'target', 'editor', editor_filename)

        s3_path = join(self._git_sha1(), self.channel, 'editor2', editor_filename)
        self.download_from_archive(s3_path, editor_path)

    def archive_editor2(self):
        if not self.channel:
            raise Exception('No channel provided when archiving the editor')

        sha1 = self._git_sha1()
        full_archive_path = join(sha1, self.channel, 'editor2')

        zip_file = "Defold-%s.zip" % self.target_platform
        gz_file = "Defold-%s.tar.gz" % self.target_platform
        dmg_file = "Defold-%s.dmg" % self.target_platform
        zip_path = join(self.defold_root, 'editor', 'target', 'editor', zip_file)
        gz_path = join(self.defold_root, 'editor', 'target', 'editor', gz_file)
        dmg_path = join(self.defold_root, 'editor', 'target', 'editor', dmg_file)
        if os.path.exists(zip_path): self.upload_to_archive(zip_path, '%s/%s' % (full_archive_path, zip_file))
        if os.path.exists(gz_path): self.upload_to_archive(gz_path, '%s/%s' % (full_archive_path, gz_file))
        if os.path.exists(dmg_path): self.upload_to_archive(dmg_path, '%s/%s' % (full_archive_path, dmg_file))
        self.wait_uploads()

    def run_editor_script(self, cmd):
        cwd = join(self.defold_root, 'editor')
        run.env_command(self._form_env(), cmd, cwd = cwd)

    def build_editor2(self):
        if not self.channel:
            raise Exception('No channel provided when bundling the editor')

        cmd = self.get_python() + ['./scripts/bundle.py',
               '--engine-artifacts=%s' % self.engine_artifacts,
               '--archive-domain=%s' % self.archive_domain,
               '--platform=%s' % self.target_platform,
               '--version=%s' % self.version,
               '--channel=%s' % self.channel,
               'build']

        if self.skip_tests:
            cmd.append("--skip-tests")

        if self.codesign:
            cmd.append('--codesign')
            if self.gcloud_keyname:
                cmd.append('--gcloud-keyname=%s' % self.gcloud_keyname)
            if self.gcloud_certfile:
                cmd.append("--gcloud-certfile=%s" % self.gcloud_certfile)
            if self.gcloud_keyfile:
                cmd.append("--gcloud-keyfile=%s" % self.gcloud_keyfile)
            if self.gcloud_location:
                cmd.append("--gcloud-location=%s" % self.gcloud_location)
            if self.gcloud_projectid:
                cmd.append("--gcloud-projectid=%s" % self.gcloud_projectid)
            if self.gcloud_keyringname:
                cmd.append("--gcloud-keyringname=%s" % self.gcloud_keyringname)

            if self.codesigning_identity:
                cmd.append('--codesigning-identity="%s"' % self.codesigning_identity)

            if self.notarization_username:
                cmd.append('--notarization-username=%s' % self.notarization_username)
            if self.notarization_password:
                cmd.append('--notarization-password=%s' % self.notarization_password)
            if self.notarization_itc_provider:
                cmd.append('--notarization-itc-provider=%s' % self.notarization_itc_provider)

        self.run_editor_script(cmd)

    def test_editor2(self):
        cmd = self.get_python() + ['./scripts/bundle.py',
               '--engine-artifacts=%s' % self.engine_artifacts,
               '--archive-domain=%s' % self.archive_domain,
               '--platform=%s' % self.target_platform]

        if self.channel:
            cmd.append('--channel=%s' % self.channel)

        cmd.append('test')

        self.run_editor_script(cmd)

#
# END: EDITOR 2
# ------------------------------------------------------------


    def bump(self):
        sha1 = self._git_sha1()

        with open('VERSION', 'r') as f:
            current = f.readlines()[0].strip()

        if self.set_version:
            new_version = self.set_version
        else:
            lst = [int(x) for x in current.split('.')]
            lst[-1] += 1
            new_version = '.'.join(map(str, lst))

        with open('VERSION', 'w') as f:
            f.write(new_version)

        print ('Bumping engine version from %s to %s' % (current, new_version))
        print ('Review changes and commit')

    def _add_private_repo_root(self, platforms, platform, private_repo):
        platform_config = platforms.setdefault(platform, {})
        if not isinstance(platform_config, dict):
            self.fatal("%s entry for %s must be an object" % (DEFOLD_PLATFORMS_FILE, platform))

        root = platform_config.get('root')
        if root == private_repo:
            return False

        platform_config['root'] = private_repo
        platform_config.pop('roots', None)
        return True

    def _print_configured_platforms(self):
        platforms = load_platforms_config()
        if not platforms:
            return

        print("Extra cross compile platforms from %s:" % DEFOLD_PLATFORMS_FILE)
        for platform in sorted(platforms.keys()):
            root = get_platform_root(platform)
            root_text = root if root else '<no root configured>'
            print("  %s: %s" % (platform, root_text))

    def add_private_repo(self):
        if not self.private_repo:
            self.fatal("No --private-repo path specified")

        private_repo = abspath(expanduser(self.private_repo))
        if not os.path.isdir(private_repo):
            self.fatal("Private repo path does not exist or is not a directory: %s" % private_repo)

        platform = self.private_platform
        if not platform:
            self.fatal("No private platform specified. Use --platform=<private-platform>.")

        if platform in get_default_target_platforms():
            self.fatal("add_private_repo requires a private platform name, but this is already a default supported platform: %s" % platform)

        platforms = load_platforms_config()
        changed = self._add_private_repo_root(platforms, platform, private_repo)

        save_platforms_config(platforms)

        print("Updated %s" % get_platforms_config_path())
        marker = "added" if changed else "already configured"
        print("  %s: %s (%s)" % (platform, get_platform_root(platform), marker))

    def save_env(self):
        if not self.save_env_path:
            self._log("No --save-env-path set when trying to save environment export")
            return

        env = self._form_env(inherit = False)
        res = ""
        for key in env:
            if bool(re.match('^[a-zA-Z0-9_]+$', key)):
                res = res + ("export %s='%s'\n" % (key, env[key]))
        with open(self.save_env_path, "w") as f:
            f.write(res)

    def find_and_set_java_home(self):
        cmd = ['java', '-XshowSettings:properties', '-version']
        process = subprocess.Popen(cmd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell = False)
        output = process.communicate()[1]
        lines = output.decode("utf-8").replace('\r', '').split('\n')

        for line in lines:
            line = line.strip()

            if 'java.home' in line:
                tokens = line.split(' = ')
                java_home = tokens[1].strip()
                os.environ['JAVA_HOME'] = java_home


    def shell(self):
        self.check_python()
        print ('Setting up shell with DEFOLD_HOME, DYNAMO_HOME, PATH, JAVA_HOME, CMAKE_GENERATOR, and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH (where applicable) set')
        self._print_configured_platforms()

        # Many login shells (e.g. zsh on macOS) reset PATH via path_helper
        # or user startup files (Homebrew shellenv), which can shadow our tools.
        # On non-Windows, re-export our PATH after login init and exec an
        # interactive shell to ensure our PATH takes precedence.
        # On Windows/msys environments, keep previous behavior to avoid
        # path translation issues and quoting of Windows paths.
        env = self._form_env()

        is_windows_host = (sys.platform == 'win32') or ('win32' in self.host)

        if not is_windows_host:
            env['DM_ENV_PATH'] = env['PATH']
            # Use $SHELL inside the shell for portability instead of injecting
            # a potentially platform-specific path from Python.
            shell_name = os.path.basename(SHELL)
            if shell_name == 'fish':
                # Fish shell isn't POSIX compatible
                reexport_cmd = 'set -gx PATH $DM_ENV_PATH; set -e DM_ENV_PATH; exec "$SHELL" -i'
            else:
                reexport_cmd = 'export PATH="$DM_ENV_PATH"; unset DM_ENV_PATH; exec "$SHELL" -i'
            args = [SHELL, '-l', '-c', reexport_cmd]
        else:
            # Keep legacy behavior on Windows/msys to preserve PATH rewriting
            # performed by the environment at shell startup.
            args = [SHELL, '-l']

        if os.path.exists("/nix"):
            args = ["nix-shell", os.path.join("scripts", "nix", "shell.nix"), "--run", " ".join(args)]

        process = subprocess.Popen(args, env=env)
        try:
            output = process.communicate()[0]
        except KeyboardInterrupt as e:
            sys.exit(0)

        if process.returncode != 0:
            if output is not None:
                self._log(str(output, encoding='utf-8'))
            sys.exit(process.returncode)

    def fatal(self, msg):
        self._log("****************************************************")
        self._log(msg)
        sys.exit(1)

# ------------------------------------------------------------
# BEGIN: RELEASE
#
    def compose_tag_name(self, version, channel):
        if channel and channel != 'stable':
            channel = '-' + channel
        else:
            channel = ''

        suffix = build_private.get_tag_suffix() # E.g. '' or 'switch'
        if suffix:
            suffix = '-' + suffix

        return '%s%s%s' % (version, channel, suffix)

    def create_tag(self):
        if self.channel is None:
            self._log("No channel specified!")
            sys.exit(1)

        is_stable = self.channel == 'stable'
        channel = '' if is_stable else self.channel
        msg = 'Release %s%s%s' % (self.version, '' if is_stable else ' - ', channel)

        tag_name = self.compose_tag_name(self.version, self.channel)

        cmd = 'git tag -f -a %s -m "%s"' % (tag_name, msg)

        # E.g.
        #   git tag -f -a 1.2.184 -m "Release 1.2.184" <- stable
        #   git tag -f -a 1.2.184-alpha -m "Release 1.2.184 - alpha"
        run.shell_command(cmd)
        return tag_name

    def push_tag(self, tag):
        cmd = 'git push -f origin %s' % tag
        run.shell_command(cmd)

    def _release_web_pages(self, releases):
        u = urlparse(self.get_archive_path())
        hostname = u.hostname
        bucket = s3.get_bucket(hostname)

        editor_archive_path = urlparse(self.get_archive_path(self.channel)).path

        release_sha1 = releases[0]['sha1']

        # The editor release notes (the update dialog's source) must land before
        # update-v4.json points users at the new sha1.
        releasenotes.upload(bucket, self.version, self.channel, required = self.channel in ('beta', 'stable'))

        html = None;
        with open(os.path.join("scripts", "resources", "downloads.html"), 'r') as file:
            html = file.read()
            file.close()

        # NOTE: We upload index.html to /CHANNEL/index.html
        # The root-index, /index.html, redirects to /stable/index.html
        self._log('Uploading %s/index.html' % self.channel)

        index_obj = bucket.Object('%s/index.html' % self.channel)
        index_obj.put(Body=html, ContentType='text/html')

        self._log('Uploading %s/info.json' % self.channel)
        new_obj = bucket.Object('%s/info.json' % self.channel)
        new_obj_content = json.dumps({'version': self.version,
                                                 'sha1' : release_sha1})
        new_obj.put(Body=new_obj_content, ContentType='application/json')

        # Editor update-v4.json
        v4_obj = bucket.Object('editor2/channels/%s/update-v4.json' % self.channel)
        self._log("Updating channel '%s' for update-v4.json: %s" % (self.channel, v4_obj.key))
        v4_content = json.dumps({'sha1': release_sha1})
        v4_obj.put(Body=v4_content, ContentType='application/json')

        # Set redirect urls so the editor can always be downloaded without knowing the latest sha1.
        # Used by www.defold.com/download
        # For example;
        #   redirect: /editor2/channels/stable/Defold-x86_64-macos.dmg -> /archive/<sha1>/stable/Defold-x86_64-macos.dmg
        for name in ['Defold-arm64-macos.dmg', 'Defold-x86_64-macos.dmg', 'Defold-x86_64-win32.zip', 'Defold-x86_64-linux.tar.gz', 'Defold-x86_64-linux.zip']:
            key_name = 'editor2/channels/%s/%s' % (self.channel, name)
            redirect = '%s/%s/%s/editor2/%s' % (editor_archive_path, release_sha1, self.channel, name)
            self._log('Creating link from %s -> %s' % (key_name, redirect))
            obj = bucket.Object(key_name)
            try:
                obj.copy_from(
                    CopySource={'Bucket': hostname, 'Key': key_name},
                    WebsiteRedirectLocation=redirect
                )
            except Exception:
                bucket.put_object(Key=key_name, Body='0', WebsiteRedirectLocation=redirect)

    def _get_tag_pattern_from_tag_name(self, channel, tag_name):
        # NOTE: Each of the main branches has a channel (stable, beta and alpha)
        #       and each of them have their separate tag patterns ("1.2.183" vs "1.2.183-beta"/"1.2.183-alpha")
        channel_pattern = ''
        if channel != 'stable':
            channel_pattern = '-' + channel
        platform_pattern = build_private.get_tag_suffix() # E.g. '' or 'switch'
        if platform_pattern:
            platform_pattern = '-' + platform_pattern

        # Example tags:
        #   1.2.184, 1.2.184-alpha, 1.2.184-beta
        #   1.2.184-switch, 1.2.184-alpha-switch, 1.2.184-beta-switch
        return r"(\d+\.\d+\.\d+%s)$" % (channel_pattern + platform_pattern)

    def _get_github_release_body(self):
        sha1 = self._git_sha1()
        body  = "Defold version %s\n" % self.version
        body += "Channel=%s sha1: %s\n" % (self.channel, sha1)
        body += "date = %s" % datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        return body

    def release(self):
        """ This step creates a tag using the channel name
        * It will update the webpage on d.defold.com (or DM_ARCHIVE_PATH)
        * It will update the releases in the target repository
        """
        if self.channel is None:
            self._log("No channel specified!")
            sys.exit(0)

        if run.shell_command('git config -l').find('remote.origin.url') != -1 and os.environ.get('GITHUB_WORKFLOW', None) is None:
            # NOTE: Only run fetch when we have a configured remote branch.
            # When running on buildbot we don't but fetching should not be required either
            # as we're already up-to-date
            self._log('Running git fetch to get latest tags and refs...')
            run.shell_command('git fetch')

        # Create or update the tag for engine releases
        prerelease = self.channel in ('alpha', 'beta')
        tag_name = None
        if self.channel in ('stable', 'beta', 'alpha'):
            tag_name = self.create_tag()
            self.push_tag(tag_name)

        if tag_name is not None:
            pattern = self._get_tag_pattern_from_tag_name(self.channel, tag_name)
            releases = s3.get_tagged_releases(self.get_archive_path(), pattern, num_releases=1)
        else:
            releases = [s3.get_single_release(self.get_archive_path(), self.version, self._git_sha1())]

        if not releases:
            self._log('Unable to find any releases')
            sys.exit(1)

        release_sha1 = releases[0]['sha1']

        if sys.stdin.isatty():
            sys.stdout.write('Release %s with SHA1 %s to channel %s? [y/n]: ' % (self.version, release_sha1, self.channel))
            sys.stdout.flush()
            response = sys.stdin.readline()
            if response[0] != 'y':
                return

        # Only release the web pages for the public repo
        if not build_private.is_repo_private():
            self._release_web_pages(releases);

        # Release to github as well
        if tag_name:
            # only allowed anyways with a github token
            body = self._get_github_release_body()
            release_name = 'v%s - %s' % (self.version, self.channel or self.channel)
            release_to_github.release(self, tag_name, release_sha1, releases[0], release_name=release_name, body=body, prerelease=prerelease)

        # Release to steam for stable only
        # if tag_name and (self.channel == 'stable'):
        #     self.release_to_steam()

    # E.g. use with ./scripts/build.py release_to_github --github-token=$CITOKEN --channel=stable
    # on a branch with the correct sha1 (e.g. beta or master)
    def release_to_github(self):
        engine_channel = None
        prerelease = True
        release_sha1 = self._git_sha1(self.version) # engine version
        if self.channel in ('stable', 'beta'):
            prerelease = False

        tag_name = self.compose_tag_name(self.version, engine_channel or self.channel)

        if tag_name is not None:
            pattern = self._get_tag_pattern_from_tag_name(self.channel, tag_name)
            releases = s3.get_tagged_releases(self.get_archive_path(), pattern, num_releases=1)
        else:
            # untagged releases
            releases = [s3.get_single_release(self.get_archive_path(), self.version, self._git_sha1())]

        body = self._get_github_release_body()
        release_name = 'v%s - %s' % (self.version, engine_channel or self.channel)

        release_to_github.release(self, tag_name, release_sha1, releases[0], release_name=release_name, body=body, prerelease=prerelease)

    def get_editor_urls_from_s3(self, archive_path, tag_name):
        release = s3.get_single_release(archive_path, tag_name)
        if not release.get("files"):
            log("No files found on S3")
            exit(1)

        # get a set of editor files only
        # for some reasons files are listed more than once in 'release'
        urls = set()
        base_url = "https://" + urlparse(archive_path).hostname
        for file in release.get("files", None):
            path = file.get("path")
            if os.path.basename(path) in ('Defold-x86_64-macos.dmg',
                                          'Defold-x86_64-linux.zip',
                                          'Defold-x86_64-win32.zip'):
                urls.add(base_url + path)

        return urls

    # Use with ./scripts/build.py release_to_steam --version=1.4.8
    def release_to_steam(self):
        channel = "stable"
        tag_name = self.compose_tag_name(self.version, channel)
        archive_path = self.get_archive_path(channel)
        urls = self.get_editor_urls_from_s3(archive_path, tag_name)
        release_to_steam.release(self, urls)


    # Use with ./scripts/build.py release_to_egs --version=1.4.8
    def release_to_egs(self):
        channel = "stable"
        tag_name = self.compose_tag_name(self.version, channel)
        archive_path = self.get_archive_path(channel)
        urls = self.get_editor_urls_from_s3(archive_path, tag_name)
        release_to_egs.release(self, urls, tag_name)

#
# END: RELEASE
# ------------------------------------------------------------

    def sync_archive(self):
        u = urlparse(self.get_archive_path())
        bucket_name = u.hostname
        bucket = s3.get_bucket(bucket_name)

        local_dir = os.path.join(self.dynamo_home, 'archive')
        self._mkdirs(local_dir)

        if not self.thread_pool:
            self.thread_pool = ThreadPool(8)

        def download(obj, path):
            self._log('s3://%s/%s -> %s' % (bucket_name, obj.key, path))
            obj.download_file(path)

        futures = []
        sha1 = self._git_sha1()
        # Only s3 is supported (scp is deprecated)
        # The pattern is used to filter out:
        # * Editor files
        # * Defold SDK files
        # * launcher files, used to launch editor2
        # * rarely used platforms: armv7-android and x86-win32
        # * headless builds
        pattern = re.compile(
            r'(^|/)editor(2)*/|/defoldsdk\.zip$|/launcher(\.exe)*$|/(armv7-android|x86-win32)(/|$)|headless'
        )
        prefix = s3.get_archive_prefix(self.get_archive_path(), self._git_sha1())
        for obj_summary in bucket.objects.filter(Prefix=prefix):
            rel = os.path.relpath(obj_summary.key, prefix)

            if not pattern.search(rel):
                p = os.path.join(local_dir, sha1, rel)
                self._mkdirs(os.path.dirname(p))
                f = Future(self.thread_pool, download, bucket.Object(obj_summary.key), p)
                futures.append(f)

        for f in futures:
            f()

# ------------------------------------------------------------
# BEGIN: SMOKE TEST
#
    def _download_editor2(self, channel, sha1):
        bundles = {
            'arm64-macos': 'Defold-arm64-macos.dmg',
            'x86_64-macos': 'Defold-x86_64-macos.dmg',
            'x86_64-linux' : 'Defold-x86_64-linux.zip',
            'x86_64-win32' : 'Defold-x86_64-win32.zip'
        }
        host = get_host_platform()
        bundle = bundles.get(host)
        if bundle:
            url = join(self.get_archive_path(), sha1, channel, 'editor2', bundle).replace("s3", "https").replace("\\", "/")
            path = self._download(url)
            return path
        else:
            print("No editor2 bundle found for %s" % host)
            return None

    def _install_editor2(self, path):
        host = get_host_platform()
        install_path = join('tmp', 'smoke_test')
        if 'macos' in host:
            out = run.command(['hdiutil', 'attach', path])
            print("cmd:" + out)
            last = [l2 for l2 in (l1.strip() for l1 in out.split('\n')) if l2][-1]
            words = last.split()
            fs = words[0]
            volume = words[-1]
            install_path = join(install_path, 'Defold.app')
            self._copy_tree(join(volume, 'Defold.app'), install_path)
            result = {'volume': volume,
                      'fs': fs,
                      'install_path': install_path,
                      'resources_path': join('Defold.app', 'Contents', 'Resources'),
                      'config': join(install_path, 'Contents', 'Resources', 'config')}
            return result
        else:
            if 'win32' in host or 'linux' in host:
                self._extract_zip(path, install_path)
            else:
                self._extract(path, install_path)
            install_path = join(install_path, 'Defold')
            result = {'install_path': install_path,
                      'resources_path': 'Defold',
                      'config': join(install_path, 'config')}
            return result

    def _uninstall_editor2(self, info):
        host = get_host_platform()
        shutil.rmtree(info['install_path'])
        if 'macos' in host:
            out = run.command(['hdiutil', 'detach', info['fs']])

    def _get_config(self, config, section, option, overrides):
        combined = '%s.%s' % (section, option)
        if combined in overrides:
            return overrides[combined]
        if section == 'bootstrap' and option == 'resourcespath':
            return '.'
        v = config.get(section, option)
        m = re.search(r"\${(\w+).(\w+)}", v)
        while m:
            s = m.group(1)
            o = m.group(2)
            v = re.sub(r"\${(\w+).(\w+)}", self._get_config(config, s, o, overrides), v, 1)
            m = re.search(r"\${(\w+).(\w+)}", v)
        return v

    def smoke_test(self):
        sha1 = self._git_sha1()
        cwd = join('tmp', 'smoke_test')
        if os.path.exists(cwd):
            shutil.rmtree(cwd)
        path = self._download_editor2(self.channel, sha1)
        info = self._install_editor2(path)
        config = ConfigParser()
        config.read(info['config'])
        overrides = {'bootstrap.resourcespath': info['resources_path']}
        jdk = 'jdk-%s' % sdk.VERSION_EDITOR_JDK
        host = get_host_platform()
        if 'win32' in host:
            java = join('Defold', 'packages', jdk, 'bin', 'java.exe')
        elif 'linux' in host:
            run.command(['chmod', '-R', '755', 'tmp/smoke_test/Defold'])
            java = join('Defold', 'packages', jdk, 'bin', 'java')
        else:
            java = join('Defold.app', 'Contents', 'Resources', 'packages', jdk, 'bin', 'java')
        jar = self._get_config(config, 'launcher', 'jar', overrides)
        vmargs = self._get_config(config, 'launcher', 'vmargs', overrides).split(',') + ['-Ddefold.log.dir=.', '-Ddefold.smoke.log=true']
        vmargs = filter(lambda x: not str.startswith(x, '-Ddefold.update.url='), vmargs)
        main = self._get_config(config, 'launcher', 'main', overrides)
        game_project = '../../editor/test/resources/geometry_wars/game.project'
        args = [java, '-cp', jar] + vmargs + [main, '--preferences=../../editor/test/resources/smoke_test.editor_settings', game_project]
        robot_jar = '%s/ext/share/java/defold-robot.jar' % self.dynamo_home
        robot_args = [java, '-jar', robot_jar, '-s', '../../share/smoke-test.edn', '-o', 'result']
        origdir = os.getcwd()
        origcwd = cwd
        if 'win32' in host:
            os.chdir(cwd)
            cwd = '.'
        print('Running robot: %s' % robot_args)
        robot_proc = subprocess.Popen(robot_args, cwd = cwd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell = False)
        time.sleep(2)
        self._log('Running editor: %s' % args)
        ed_proc = subprocess.Popen(args, cwd = cwd, shell = False)
        os.chdir(origdir)
        cwd = origcwd
        output = robot_proc.communicate()[0]
        if ed_proc.poll() == None:
            ed_proc.terminate()
            ed_proc.wait()
        self._uninstall_editor2(info)

        result_archive_path = '/'.join(['int.d.defold.com', 'archive', sha1, self.channel, 'editor2', 'smoke_test'])
        def _findwebfiles(libdir):
            paths = os.listdir(libdir)
            paths = [os.path.join(libdir, x) for x in paths if os.path.splitext(x)[1] in ('.html', '.css', '.png')]
            return paths
        for f in _findwebfiles(join(cwd, 'result')):
            self.upload_to_s3(f, 's3://%s/%s' % (result_archive_path, basename(f)))
        self.wait_uploads()
        self._log('Log: https://s3-eu-west-1.amazonaws.com/%s/index.html' % (result_archive_path))

        if robot_proc.returncode != 0:
            sys.exit(robot_proc.returncode)
        return True

    def local_smoke(self):
        host = get_host_platform()
        cwd = './editor'
        if os.path.exists('editor/log.txt'):
            os.remove('editor/log.txt')
        game_project = 'test/resources/geometry_wars/game.project'
        args = ['./scripts/lein', 'with-profile', '+smoke-test', 'run', game_project]
        robot_jar = '../defold-robot/target/defold-robot-0.7.0-standalone.jar'
        robot_args = ['java', '-jar', robot_jar, '-s', '../share/smoke-test.edn', '-o', 'local_smoke_result']
        origdir = os.getcwd()
        origcwd = cwd
        if 'win32' in host:
            os.chdir(cwd)
            args = ['sh'] + args
            cwd = '.'
        print('Running robot: %s' % robot_args)
        robot_proc = subprocess.Popen(robot_args, cwd = cwd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell = False)
        time.sleep(2)
        self._log('Running editor: %s' % args)
        ed_proc = subprocess.Popen(args, cwd = cwd, shell = False)
        os.chdir(origdir)
        cwd = origcwd
        output = robot_proc.communicate()[0]
        if ed_proc.poll() == None:
            ed_proc.terminate()
            ed_proc.wait()
        if robot_proc.returncode != 0:
            sys.exit(robot_proc.returncode)
        return True
#
# END: SMOKE TEST
# ------------------------------------------------------------
    def get_archive_path(self, channel=None):
        if channel is None:
            channel = self.channel
        assert(type(channel) == str)
        return join(self.archive_path, channel)

    def get_archive_redirect_key(self, url):
        old_url = url.replace(self.get_archive_path().replace("\\", "/"), self.archive_path)
        u = urlparse(old_url)
        p = u.path
        return p.lstrip('/')

    def download_from_archive(self, src_path, dst_file):
        url = join(self.get_archive_path(), src_path)
        self.download_from_s3(dst_file, url)


    def upload_to_archive(self, src_file, dst_path):
        url = join(self.get_archive_path(), dst_path).replace("\\", "/")
        self.upload_to_s3(src_file, url)


    def download_from_s3(self, path, url):
        url = url.replace('\\', '/')
        self._log('Downloading %s -> %s' % (url, path))
        u = urlparse(url)

        if u.scheme == 's3':
            self._mkdirs(os.path.dirname(path))

            bucket = s3.get_bucket(u.netloc)
            bucket.download_file(u.path.lstrip('/'), path)
            self._log('Downloaded %s -> %s' % (url, path))
        else:
            raise Exception('Unsupported url %s' % (url))


    def upload_to_s3(self, path, url):
        url = url.replace('\\', '/')
        self._log('Uploading %s -> %s' % (path, url))

        u = urlparse(url)

        if u.scheme == 's3':
            bucket = s3.get_bucket(u.netloc)
            # create redirect so that the old s3 paths still work
            # s3://d.defold.com/archive/channel/sha1/engine/* -> http://d.defold.com/archive/sha1/engine/*
            redirect_key = self.get_archive_redirect_key(url)
            redirect_url = url.replace("s3://", "http://")

            if not self.thread_pool:
                self.thread_pool = ThreadPool(8)

            p = u.path
            if p[-1] == '/':
                p += basename(path)

            # strip first / character to make key like dir1/dir2/filename.ext
            p = p.lstrip('/')
            def upload_singlefile():
                bucket.upload_file(path, p)
                self._log('Uploaded %s -> %s' % (path, url))
                self._log("Create redirection %s -> %s : %s" % (url, redirect_key, redirect_url))
                bucket.put_object(Key=redirect_key, Body='0', WebsiteRedirectLocation=redirect_url)

            def upload_multipart():
                contenttype, _ = mimetypes.guess_type(path)
                mp = None
                if contenttype is not None:
                    mp = bucket.Object(p).initiate_multipart_upload(ContentType=contenttype)
                else:
                    mp = bucket.Object(p).initiate_multipart_upload()

                source_size = os.stat(path).st_size
                chunksize = 64 * 1024 * 1024 # 64 MiB
                chunkcount = int(math.ceil(source_size / float(chunksize)))

                def upload_part(filepath, part, offset, size):
                    with open(filepath, 'rb') as fhandle:
                        fhandle.seek(offset)
                        part_content = fhandle.read(size)
                        part = mp.Part(part)
                        part.upload(Body=part_content, ContentLength=size)
                        fhandle.close()

                _threads = []
                for i in range(chunkcount):
                    part = i + 1
                    offset = i * chunksize
                    remaining = source_size - offset
                    size = min(chunksize, remaining)
                    args = {'filepath': path, 'part': part, 'offset': offset, 'size': size}

                    self._log('Uploading #%d %s -> %s' % (part, path, url))
                    _thread = Thread(target=upload_part, kwargs=args)
                    _threads.append(_thread)
                    _thread.start()

                for i in range(chunkcount):
                    _threads[i].join()
                    self._log('Uploaded #%d %s -> %s' % (i + 1, path, url))


                if len(list(mp.parts.all())) == chunkcount:
                    try:
                        parts = []
                        for part_summary in mp.parts.all():
                            parts.append({ 'ETag': part_summary.e_tag, 'PartNumber': part_summary.part_number })

                        mp.complete(MultipartUpload={ 'Parts': parts })
                        self._log('Uploaded %s -> %s' % (path, url))
                        self._log("Create redirection %s -> %s : %s" % (url, redirect_key, redirect_url))
                        bucket.put_object(Key=redirect_key, Body='0', WebsiteRedirectLocation=redirect_url)

                    except:
                        # If any exception ocurred during completion - we need to call abort()
                        # to free storage from uploaded parts. S3 doesn't do it automatically
                        mp.abort()
                        self._log('Failed to upload %s -> %s' % (path, url))
                        raise RuntimeError('Failed to upload %s -> %s' % (path, url))

                else:
                    mp.abort()
                    self._log('Failed to upload %s -> %s' % (path, url))
                    raise RuntimeError('Failed to upload %s -> %s' % (path, url))

            f = None
            #if sys.platform == 'win32':
            f = Future(self.thread_pool, upload_singlefile)
            # else:
            #     f = Future(self.thread_pool, upload_multipart)
            self.futures.append(f)

        else:
            raise Exception('Unsupported url %s' % (url))

    def wait_uploads(self):
        for f in self.futures:
            f()
        self.futures = []

    def _form_env(self, inherit = True):
        env = os.environ.copy() if inherit else {}

        host = self.host

        ld_library_path = 'DYLD_LIBRARY_PATH' if 'macos' in self.host else 'LD_LIBRARY_PATH'
        ld_library_paths = ['%s/lib/%s' % (self.dynamo_home, self.target_platform),
                            '%s/ext/lib/%s' % (self.dynamo_home, self.host)]

        env[ld_library_path] = os.path.pathsep.join(ld_library_paths)

        pythonpaths = ['%s/lib/python' % self.dynamo_home,
                      '%s/build_tools' % self.defold,
                      '%s/ext/lib/python' % self.dynamo_home]
        env['PYTHONPATH'] = os.path.pathsep.join(pythonpaths)
        env['PYTHONIOENCODING'] = 'UTF-8'

        if not 'JAVA_HOME' in os.environ:
            self.find_and_set_java_home()
        if not 'JAVA_HOME' in os.environ:
            self.fatal("Failed to find JAVA_HOME environment variable or valid java executable")
        env['JAVA_HOME'] = os.environ['JAVA_HOME']
        env['DM_JAVA_RUNTIME_FLAGS'] = JAVA_RUNTIME_FLAGS

        env['DEFOLD_HOME'] = self.defold_home
        env['DYNAMO_HOME'] = self.dynamo_home
        env.setdefault('CMAKE_GENERATOR', 'Ninja')

        android_host = self.host
        if 'win32' in android_host:
            android_host = 'windows'
        paths = os.path.pathsep.join(['%s/bin/%s' % (self.dynamo_home, self.target_platform),
                                      '%s/bin' % (self.dynamo_home),
                                      '%s/ext/bin' % self.dynamo_home,
                                      '%s/ext/bin/%s' % (self.dynamo_home, host)])

        env['PATH'] = paths + os.path.pathsep + os.environ['PATH']

        # This trickery is needed for the bash to properly inherit the PATH that we've set here
        # See /etc/profile for further details
        is_mingw = os.environ.get('MSYSTEM', '') in ('MINGW64',)
        if is_mingw:
            env['ORIGINAL_PATH'] = env['PATH']

        env['MAVEN_OPTS'] = '-Xms256m -Xmx700m -XX:MaxPermSize=1024m'

        # Force 32-bit python 2.7 on darwin.
        env['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
        env['VERSIONER_PYTHON_VERSION'] = '2.7'

        if self.no_colors:
            env['NOCOLOR'] = '1'

        if self.test_device and 'android' in self.target_platform:
            env['ANDROID_SERIAL'] = self.test_device
        elif self.test_device and self.target_platform == 'x86_64-xbone':
            env['XBOX_CONSOLE'] = self.test_device

        build_ios.apply_build_options_to_env(
            env,
            self.target_platform,
            test_device=self.test_device,
            identity=self.ios_identity,
            mobileprovision=self.ios_mobileprovision,
            team_id=self.ios_team_id,
            bundle_id_prefix=self.ios_bundle_id_prefix)

        # XMLHttpRequest Emulation for node.js
        xhr2_path = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR, 'xhr2', 'package', 'lib')
        if 'NODE_PATH' in os.environ:
            env['NODE_PATH'] = xhr2_path + os.path.pathsep + os.environ['NODE_PATH']
        else:
            env['NODE_PATH'] = xhr2_path

        return env

if __name__ == '__main__':
    s3.init_boto_data_path()
    usage = '''usage: %prog [options] command(s)

Commands:
distclean        - Removes the DYNAMO_HOME folder
clean            - Remove generated engine build outputs without removing DYNAMO_HOME
install_ext      - Install external packages
build_external   - Build external packages, optionally filtered with --package
install_release_dependencies - Install Python dependencies required by release
install_sdk      - Install sdk
install_waf      - Install waf
sync_archive     - Sync engine artifacts from S3
build_engine     - Build engine
archive_engine   - Archive engine (including builtins) to path specified with --archive-path
build_editor2    - Build editor
test_editor2     - Test editor
archive_editor2  - Archive editor to path specified with --archive-path
download_editor2 - Download editor bundle (zip)
build_bob        - Build bob with native libraries included for cross platform deployment
test_bob         - Test bob using an existing com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar
build_bob_light  - Build a lighter version of bob (mostly used for test content during builds)
archive_bob      - Archive bob to path specified with --archive-path
build_docs       - Build documentation
build_builtins   - Build builtin content archive
bump             - Bump version number
release          - Release editor
shell            - Start development shell
add_private_repo - Add a private repo to .defold-platforms
smoke_test       - Test editor and engine in combination
local_smoke      - Test run smoke test using local dev environment
gen_sdk_source   - Regenerate the dmSDK bindings from our C sdk

Multiple commands can be specified

CMake shorthand defaults from build.py shell: CMAKE_GENERATOR=Ninja, omitted --platform uses the host platform, CMAKE_BUILD_TYPE=RelWithDebInfo, and BUILD_TESTS=ON.
Use -- --opt-level=0 for Debug, -- --skip-build-tests to skip building tests, or --skip-tests to skip running tests.
Use --with-waf to build engine libs through the Waf fallback path and include Waf in install_ext during the CMake transition.

To pass on arbitrary options to waf/CMake: build.py OPTIONS COMMANDS -- BUILD_OPTIONS
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      help = 'Target platform. Defaults to the host platform. With add_private_repo, this may be a new private platform to write to .defold-platforms')

    parser.add_option('--skip-tests', dest='skip_tests',
                      action = 'store_true',
                      default = False,
                      help = 'Skip unit-tests. Default is false')

    parser.add_option('--test-device', dest='test_device',
                      default = None,
                      help = 'Device to target when running mobile tests. Android uses a device serial; iOS uses a physical device or simulator identifier, name, serial number or UDID')

    parser.add_option('--ios-identity', dest='ios_identity',
                      default = None,
                      help = 'iOS code signing identity name or SHA-1 to use for device tests')

    parser.add_option('--ios-mobileprovision', dest='ios_mobileprovision',
                      default = None,
                      help = 'Path to a .mobileprovision file to use for iOS device tests')

    parser.add_option('--ios-team-id', dest='ios_team_id',
                      default = None,
                      help = 'Apple development team id to use when selecting iOS device-test provisioning profiles')

    parser.add_option('--ios-bundle-id-prefix', dest='ios_bundle_id_prefix',
                      default = None,
                      help = 'Bundle id prefix for generated iOS device-test apps. Default is com.defold.tests')

    parser.add_option('--keep-bob-uncompressed', dest='keep_bob_uncompressed',
                    action = 'store_true',
                    default = False,
                    help = 'do not apply compression to bob.jar. Default is false')

    parser.add_option('--codesign', dest='codesign',
                      action = 'store_true',
                      default = False,
                      help = 'enable code signing (engine and editor). Default is false')

    parser.add_option('--skip-docs', dest='skip_docs',
                      action = 'store_true',
                      default = False,
                      help = 'skip building docs when building the engine. Default is false')

    parser.add_option('--incremental', dest='incremental',
                      action = 'store_true',
                      default = False,
                      help = 'skip reconfigure/distclean when building with Waf. Top-level CMake build_engine is incremental by default')

    parser.add_option('--skip-builtins', dest='skip_builtins',
                      action = 'store_true',
                      default = False,
                      help = 'skip building builtins when building the engine. Default is false')

    parser.add_option('--skip-bob-light', dest='skip_bob_light',
                      action = 'store_true',
                      default = False,
                      help = 'skip building bob-light when building the engine. Default is false')

    parser.add_option('--disable-ccache', dest='disable_ccache',
                      action = 'store_true',
                      default = False,
                      help = 'force disable of ccache. Default is false')

    parser.add_option('--generate-compile-commands', dest='generate_compile_commands',
                      action = 'store_true',
                      default = False,
                      help = 'generate compile_commands.json file. Default is false')

    parser.add_option('--no-colors', dest='no_colors',
                      action = 'store_true',
                      default = False,
                      help = 'No color output. Default is color output')

    default_archive_domain = DEFAULT_ARCHIVE_DOMAIN
    parser.add_option('--archive-domain', dest='archive_domain',
                      default = default_archive_domain,
                      help = 'Domain where builds will be archived. Default is %s' % default_archive_domain)

    default_package_path = CDN_PACKAGES_URL
    parser.add_option('--package-path', dest='package_path',
                      default = default_package_path,
                      help = 'Either an url to a file server where the sdk packages are located, or a path to a local folder. Reads $DM_PACKAGES_URL. Default is %s.' % default_package_path)

    parser.add_option('--package', dest='external_package',
                      default = None,
                      help = 'External package to build with build_external. Valid packages: %s' % ', '.join(EXTERNAL_LIBS))

    parser.add_option('--set-version', dest='set_version',
                      default = None,
                      help = 'Set version explicitily when bumping version')

    parser.add_option('--channel', dest='channel',
                      default = None,
                      help = 'Editor release channel (stable, beta, ...)')

    parser.add_option('--engine-artifacts', dest='engine_artifacts',
                      default = 'auto',
                      help = 'What engine version to bundle the Editor with (auto, dynamo-home, archived, archived-stable or a SHA1)')

    parser.add_option('--save-env-path', dest='save_env_path',
                      default = None,
                      help = 'Save environment variables to a file')

    parser.add_option('--private-repo', dest='private_repo',
                      default = None,
                      help = 'Path to a private repo to add to .defold-platforms')

    parser.add_option('--notarization-username', dest='notarization_username',
                      default = None,
                      help = 'Username to use when sending the editor for notarization')

    parser.add_option('--notarization-password', dest='notarization_password',
                      default = None,
                      help = 'Password to use when sending the editor for notarization')

    parser.add_option('--notarization-itc-provider', dest='notarization_itc_provider',
                      default = None,
                      help = 'Optional iTunes Connect provider to use when sending the editor for notarization')

    parser.add_option('--github-token', dest='github_token',
                      default = None,
                      help = 'GitHub authentication token when releasing to GitHub')

    github_release_target_repo = DEFAULT_RELEASE_REPOSITORY
    parser.add_option('--github-target-repo', dest='github_target_repo',
                      default = github_release_target_repo,
                      help = 'GitHub target repo when releasing artefacts. Default is %s' % github_release_target_repo)

    parser.add_option('--github-sha1', dest='github_sha1',
                      default = None,
                      help = 'A specific sha1 to use in github operations')

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version to use instead of from VERSION file')

    parser.add_option('--codesigning-identity', dest='codesigning_identity',
                      default = 'Developer ID Application: Stiftelsen Defold Foundation (26PW6SVA7H)',
                      help = 'Codesigning identity for macOS version of the editor')

    parser.add_option('--gcloud-projectid', dest='gcloud_projectid',
                      default = None,
                      help = 'Google Cloud project id where key ring is stored')

    parser.add_option('--gcloud-location', dest='gcloud_location',
                      default = None,
                      help = 'Google cloud region where key ring is located')

    parser.add_option('--gcloud-keyringname', dest='gcloud_keyringname',
                      default = None,
                      help = 'Google Cloud key ring name')

    parser.add_option('--gcloud-keyname', dest='gcloud_keyname',
                      default = None,
                      help = 'Google Cloud key name')

    parser.add_option('--gcloud-certfile', dest='gcloud_certfile',
                      default = None,
                      help = 'Google Cloud certificate chain file')

    parser.add_option('--gcloud-keyfile', dest='gcloud_keyfile',
                      default = None,
                      help = 'Google Cloud service account key file')

    parser.add_option('--verbose', dest='verbose',
                      action = 'store_true',
                      default = False,
                      help = 'If used, outputs verbose logging')

    parser.add_option('--size-analyze', dest='size_analyze',
                      action = 'store_true',
                      default = False,
                      help = 'Emit extra wasm-web analysis artifacts such as source maps and separate DWARF')

    parser.add_option('--with-waf', dest='with_waf',
                      action = 'store_true',
                      default = False,
                      help = 'Build engine libs with the Waf fallback path instead of the top-level CMake path')

    options, all_args = parser.parse_args()

    args = list(filter(lambda x: x[:2] != '--', all_args))
    waf_options = list(filter(lambda x: x[:2] == '--', all_args))
    if options.size_analyze:
        waf_options.append('--size-analyze')
    if options.with_waf:
        waf_options.append('--with-waf')

    if len(args) == 0:
        parser.error('No command specified')

    known_platforms = get_target_platforms()
    is_add_private_repo = 'add_private_repo' in args
    if options.target_platform and options.target_platform not in known_platforms and not is_add_private_repo:
        parser.error("option --platform: invalid choice: %r (choose from %s)" % (options.target_platform, ', '.join(known_platforms)))

    private_platform = None
    if is_add_private_repo:
        private_platform = options.target_platform or get_host_platform()
        if private_platform in get_default_target_platforms():
            parser.error("add_private_repo requires a private platform name, but this is already a default supported platform: %s" % private_platform)
        if private_platform not in known_platforms and len(args) > 1:
            parser.error("add_private_repo for a new private platform must be run before other commands: %s" % private_platform)

    if not options.target_platform or (is_add_private_repo and options.target_platform not in known_platforms):
        target_platform = get_host_platform()
    else:
        target_platform = options.target_platform

    build_private.set_target_platform(target_platform)

    c = Configuration(dynamo_home = os.environ.get('DYNAMO_HOME', None),
                      target_platform = target_platform,
                      skip_tests = options.skip_tests,
                      test_device = options.test_device,
                      ios_identity = options.ios_identity,
                      ios_mobileprovision = options.ios_mobileprovision,
                      ios_team_id = options.ios_team_id,
                      ios_bundle_id_prefix = options.ios_bundle_id_prefix,
                      keep_bob_uncompressed = options.keep_bob_uncompressed,
                      codesign = options.codesign,
                      skip_docs = options.skip_docs,
                      incremental = options.incremental,
                      skip_builtins = options.skip_builtins,
                      skip_bob_light = options.skip_bob_light,
                      disable_ccache = options.disable_ccache,
                      generate_compile_commands = options.generate_compile_commands,
                      no_colors = options.no_colors,
                      archive_domain = options.archive_domain,
                      package_path = options.package_path,
                      external_package = options.external_package,
                      set_version = options.set_version,
                      channel = options.channel,
                      engine_artifacts = options.engine_artifacts,
                      waf_options = waf_options,
                      save_env_path = options.save_env_path,
                      private_repo = options.private_repo,
                      private_platform = private_platform,
                      notarization_username = options.notarization_username,
                      notarization_password = options.notarization_password,
                      notarization_itc_provider = options.notarization_itc_provider,
                      github_token = options.github_token,
                      github_target_repo = options.github_target_repo,
                      github_sha1 = options.github_sha1,
                      version = options.version,
                      codesigning_identity = options.codesigning_identity,
                      gcloud_projectid = options.gcloud_projectid,
                      gcloud_location = options.gcloud_location,
                      gcloud_keyringname = options.gcloud_keyringname,
                      gcloud_keyname = options.gcloud_keyname,
                      gcloud_certfile = options.gcloud_certfile,
                      gcloud_keyfile = options.gcloud_keyfile,
                      verbose = options.verbose)

    commands_without_dynamo_home = ['shell', 'save_env', 'add_private_repo']
    needs_dynamo_home = any(cmd not in commands_without_dynamo_home for cmd in args)
    if needs_dynamo_home:
        for env_var in ['DEFOLD_HOME', 'DYNAMO_HOME', 'PYTHONPATH', 'JAVA_HOME']:
            if not env_var in os.environ:
                c._log("CMD: " + ' '.join(sys.argv))
                msg = f"{env_var} was not found in environment.\nDid you use './scripts/build.py shell'?"
                c.fatal(msg)

    for cmd in args:
        f = getattr(c, cmd, None)
        if not f:
            parser.error('Unknown command %s' % cmd)
        else:
            c.build_tracker.start_command(cmd)
            f()
            c.wait_uploads()
            c.build_tracker.end_command(cmd)

    # Print and save build time summary
    c.build_tracker.print_summary()
    c.build_tracker.save_times()

    print('Done')
