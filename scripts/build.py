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

# add build_tools folder to the import search path
import sys, os, platform
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath, splitext
sys.path.append(os.path.join(normpath(join(dirname(abspath(__file__)), '..')), "build_tools"))

import shutil, zipfile, re, itertools, json, platform, math, mimetypes, hashlib
import optparse, pprint, subprocess, urllib, urllib.parse, tempfile, time
import github
import run
import s3
import sdk
import release_to_github
import release_to_steam
import release_to_egs
import BuildUtility
import http_cache
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
                    'js-web', 'wasm-web', 'wasm_pthread-web']

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
    '--with-opus': 'WITH_OPUS',
    '--with-webgpu': 'WITH_WEBGPU'
}

sys.dont_write_bytecode = True
try:
    import build_vendor
    sys.modules['build_private'] = build_vendor
    print("Imported %s from %s" % ('build_private', build_vendor.__file__))
except ModuleNotFoundError as e:
    if "No module named 'build_vendor'" in str(e):
        print("Couldn't find build_vendor.py. Skipping.")
        pass
    else:
        raise e
except Exception as e:
    print("Failed to import build_vendor.py:")
    raise e

sys.dont_write_bytecode = False
try:
    import build_private
except Exception:
    pass

if 'build_private' not in sys.modules:
    class build_private(object):
        @classmethod
        def get_target_platforms(cls):
            return []
        @classmethod
        def get_install_host_packages(cls, platform): # Returns the packages that should be installed for the host
            return []
        @classmethod
        def get_install_target_packages(cls, platform): # Returns the packages that should be installed for the target
            return []
        @classmethod
        def install_sdk(cls, configuration, platform): # Installs the sdk for the private platform
            pass
        @classmethod
        def is_library_supported(cls, platform, library):
            return True
        @classmethod
        def is_repo_private(self):
            return False
        @classmethod
        def get_tag_suffix(self):
            return ''

assert(hasattr(build_private, 'get_target_platforms'))
assert(hasattr(build_private, 'get_install_host_packages'))
assert(hasattr(build_private, 'get_install_target_packages'))
assert(hasattr(build_private, 'install_sdk'))
assert(hasattr(build_private, 'is_library_supported'))
assert(hasattr(build_private, 'is_repo_private'))
assert(hasattr(build_private, 'get_tag_suffix'))

def get_target_platforms():
    return BASE_PLATFORMS + build_private.get_target_platforms()

PACKAGES_ALL=[
    "protobuf-3.20.1",
    "junit-4.6",
    "jsign-4.2",
    "protobuf-java-3.20.1",
    "openal-1.1",
    "maven-3.0.1",
    "vecmath",
    "vpx-1.7.0",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "defold-robot-0.7.0",
    "bullet-2.77",
    "libunwind-395b27b68c5453222378bc5fe4dab4c6db89816a",
    "jctest-0.12",
    "vulkan-v1.4.307",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-11.3.2",
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
    "harfbuzz-11.3.2",
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
    "harfbuzz-11.3.2",
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
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

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
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

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
    "harfbuzz-11.3.2",
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
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

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
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

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
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]

PACKAGES_ANDROID=[
"protobuf-3.20.1",
    "android-support-multidex",
    "androidx-multidex",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "glfw-2.7.1",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]
PACKAGES_ANDROID.append(sdk.ANDROID_PACKAGE)

PACKAGES_ANDROID_64=[
"protobuf-3.20.1",
    "android-support-multidex",
    "androidx-multidex",
    "luajit-2.1.0-3e223cb",
    "tremolo-b0cb4d1",
    "bullet-2.77",
    "glfw-2.7.1",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-11.3.2",
    "SheenBidi-2.9.0",
    "libunibreak-6.1",
    "SkriBidi-1e8038"]
PACKAGES_ANDROID_64.append(sdk.ANDROID_PACKAGE)

PACKAGES_EMSCRIPTEN=[
    "protobuf-3.20.1",
    "bullet-2.77",
    "glfw-2.7.1",
    "wagyu-39",
    "box2d-3.1.0",
    "box2d_defold-2.2.1",
    "opus-1.5.2",
    "harfbuzz-11.3.2",
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
    'js-web':           PACKAGES_EMSCRIPTEN,
    'wasm-web':         PACKAGES_EMSCRIPTEN,
    'wasm_pthread-web': PACKAGES_EMSCRIPTEN
}

DMSDK_PACKAGES_ALL="vectormathlibrary-r1649".split()

CDN_PACKAGES_URL=os.environ.get("DM_PACKAGES_URL", None)
DEFAULT_ARCHIVE_DOMAIN=os.environ.get("DM_ARCHIVE_DOMAIN", "d.defold.com")
DEFAULT_RELEASE_REPOSITORY=os.environ.get("DM_RELEASE_REPOSITORY") if os.environ.get("DM_RELEASE_REPOSITORY") else release_to_github.get_current_repo()

PACKAGES_TAPI_VERSION="tapi1.6"
PACKAGES_NODE_MODULE_XHR2="xhr2-v0.1.0"
PACKAGES_ANDROID_NDK="android-ndk-r{0}".format(sdk.ANDROID_NDK_VERSION)
PACKAGES_ANDROID_SDK="android-sdk"
PACKAGES_CCTOOLS_PORT="cctools-port-darwin19-6c438753d2252274678d3e0839270045698c159b-linux"

NODE_MODULE_LIB_DIR = os.path.join("ext", "lib", "node_modules")

SHELL = os.environ.get('SHELL', 'bash')
# Don't use WSL from the msys/cygwin terminal
if os.environ.get('TERM','') in ('cygwin',):
    if 'WD' in os.environ:
        SHELL= '%s\\bash.exe' % os.environ['WD'] # the binary directory

ENGINE_LIBS = "testmain dlib jni texc modelc shaderc ddf platform font graphics particle lua hid input physics resource extension script render rig gameobject gui sound liveupdate crash gamesys tools record profiler engine sdk".split()
HOST_LIBS = "testmain dlib jni texc modelc shaderc".split()

CMAKE_SUPPORT = ['platform', 'hid', 'input']

EXTERNAL_LIBS = "box2d box2d_v2 glfw bullet3d opus".split()

def get_host_platform():
    return sdk.get_host_platform()

def format_exes(name, platform):
    prefix = ''
    suffix = ['']
    if 'win32' in platform:
        suffix = ['.exe']
    elif 'android' in platform:
        prefix = 'lib'
        suffix = ['.so']
    elif platform in ['js-web']:
        prefix = ''
        suffix = ['.js']
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
                 keep_bob_uncompressed = False,
                 skip_codesign = False,
                 skip_docs = False,
                 incremental = False,
                 skip_builtins = False,
                 skip_bob_light = False,
                 disable_ccache = False,
                 generate_compile_commands = False,
                 no_colors = False,
                 archive_domain = None,
                 package_path = None,
                 set_version = None,
                 channel = None,
                 engine_artifacts = None,
                 waf_options = [],
                 save_env_path = None,
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

        self.build_utility = BuildUtility.BuildUtility(self.target_platform, self.host, self.dynamo_home)

        self.skip_tests = skip_tests
        self.keep_bob_uncompressed = keep_bob_uncompressed
        self.skip_codesign = skip_codesign
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
        self.set_version = set_version
        self.channel = channel
        self.engine_artifacts = engine_artifacts
        self.waf_options = waf_options
        self.save_env_path = save_env_path
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
        - Windows: Visual Studio 2022
        - Android: Prints guidance to open CMakeLists.txt in Android Studio
        - Other platforms: falls back to Ninja/Unix Makefiles by default
        """
        tp = target_platform or self.target_platform
        if not tp:
            raise RuntimeError('make_solution: target_platform must be specified')

        # Android guidance
        if 'android' in tp:
            self._log('Android: Open the top-level CMakeLists.txt directly in Android Studio to create a project.')
            return

        # Choose generator
        generator = None
        arch_args = []

        if tp.endswith('-macos') or tp.endswith('-ios'):
            generator = 'Xcode'
        elif 'win32' in tp:
            generator = 'Visual Studio 17 2022'
            # Map architecture
            if tp == 'x86_64-win32':
                arch_args = ['-A', 'x64']
            elif tp == 'win32':
                arch_args = ['-A', 'Win32']

        # Build directory per target platform inside solutions/<platform>
        if not build_dir:
            build_dir = os.path.join(self.defold_root, 'solutions', tp)
        self._mkdirs(build_dir)

        cmake_cmd = ['cmake', '-S', self.defold_root, '-B', build_dir, f'-DTARGET_PLATFORM={tp}']
        if generator:
            cmake_cmd += ['-G', generator]
        if arch_args:
            cmake_cmd += arch_args

        # Generate solution
        self._log('Generating solution with command: %s' % ' '.join(cmake_cmd))
        run.env_command(self._form_env(), cmake_cmd)

        # Report absolute path to the generated solution files
        old_project_name = 'defold_libraries'
        solution_build_dir = os.path.abspath(build_dir)

        target_name = f"Defold-{tp}"
        final_path = solution_build_dir

        old_path = None
        new_path = None

        if generator == 'Xcode':
            # Default name from CMake project()
            old_path = os.path.join(solution_build_dir, f"{old_project_name}.xcodeproj")
            new_path = os.path.join(solution_build_dir, f"{target_name}.xcodeproj")
        elif generator and generator.startswith('Visual Studio'):
            old_path = os.path.join(solution_build_dir, f"{old_project_name}.sln")
            new_path = os.path.join(solution_build_dir, f"{target_name}.sln")

        if new_path != old_path and old_path is not None:
            if os.path.exists(old_path):
                try:
                    if os.path.exists(new_path):
                        shutil.rmtree(new_path)
                    shutil.move(old_path, new_path)
                    final_path = new_path
                except Exception as e:
                    self._log(f"Warning: Failed to rename project: {e}. Keeping default name: {old_path}")
                    final_path = old_path
            else:
                final_path = new_path

        self._log(f'Solution generated: {final_path}')

    def __del__(self):
        if len(getattr(self, "futures", [])) > 0:
            print('ERROR: Pending futures (%d)' % len(self.futures))
            os._exit(5)

    def get_python(self):
        self.check_python()
        return [sys.executable]

    def _create_common_dirs(self):
        for p in ['ext/lib/python', 'share', 'lib/js-web/js', 'lib/wasm-web/js', 'lib/wasm_pthread-web/js']:
            self._mkdirs(join(self.dynamo_home, p))

    def _mkdirs(self, path):
        if not os.path.exists(path):
            os.makedirs(path)

    def _log(self, msg):
        print(str(msg))
        sys.stdout.flush()
        sys.stderr.flush()

    def _remove_tree(self, path):
        if os.path.exists(path):
            self._log('Removing %s' % path)
            shutil.rmtree(path)

    def distclean(self):
        self._remove_tree(self.dynamo_home)

        for lib in ENGINE_LIBS:
            builddir = join(self.defold_root, 'engine/%s/build' % lib)
            self._remove_tree(builddir)

        # remove engine test dir specifically
        self._remove_tree(join(self.defold_root, 'engine/engine/src/test/build'))

        # Recreate dirs
        self._create_common_dirs()
        self._log('distclean done.')

    def _extract_tgz(self, file, path):
        self._log('Extracting %s to %s' % (file, path))
        self._mkdirs(path)
        suffix = os.path.splitext(file)[1]
        fmts = {'.gz': 'z', '.xz': 'J', '.bzip2': 'j'}
        run.env_command(self._form_env(), ['tar', 'xf%s' % fmts.get(suffix, 'z'), file], cwd = path)

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
        waf_package = "waf-2.0.3"
        waf_path = make_package_path(self.defold_root, 'common', waf_package)
        self._extract_tgz(waf_path, self.ext)

    def install_ext(self):
        def make_package_path(root, platform, package):
            return join(root, 'packages', package) + '-%s.tar.gz' % platform

        def make_package_paths(root, platform, packages):
            return [make_package_path(root, platform, package) for package in packages]

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

        if target_platform in ['js-web', 'wasm-web', 'wasm_pthread-web']:
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

        for base_platform in self.get_base_platforms():
            packages = list(PACKAGES_HOST) + build_private.get_install_host_packages(base_platform)
            packages.extend(PLATFORM_PACKAGES.get(base_platform, []))
            package_paths = make_package_paths(self.defold_root, base_platform, packages)
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

        target_packages = PLATFORM_PACKAGES.get(self.target_platform, []) + build_private.get_install_target_packages(self.target_platform)
        target_package_paths = make_package_paths(self.defold_root, self.target_platform, target_packages)
        target_package_paths = [path for path in target_package_paths if path not in installed_packages]

        if len(target_package_paths) != 0:
            print("Installing %s packages" % self.target_platform)
            for path in target_package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(target_package_paths)

        print("Installing python wheels")
        run.env_command(self._form_env(), self.get_python() + ['-m', 'pip', '-q', '-q', 'install', '-t', join(self.ext, 'lib', 'python'), 'requests', 'pyaml', 'rangehttpserver', 'pystache'])
        for whl in glob(join(self.defold_root, 'packages', '*.whl')):
            self._log('Installing %s' % basename(whl))
            run.env_command(self._form_env(), self.get_python() + ['-m', 'pip', '-q', '-q', 'install', '--upgrade', '-t', join(self.ext, 'lib', 'python'), whl])

        print("Installing javascripts")
        for n in 'js-web-pre.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        for n in 'js-web-pre-engine.js'.split():
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

    def check_python(self):
        if sys.version_info.major != 3:
            self.fatal("The build scripts requires Python 3!")

    def has_sdk(self, sdkfolder, target_platform):
        return None != sdk.get_sdk_info(sdkfolder, target_platform, False)

    def _find_program(self, platform, name, paths):
        name = format_exes(name, platform)[0]
        for path in paths:
            fullpath = os.path.join(path, name)
            if os.path.isfile(fullpath):
                return fullpath
        return None

    def check_sdk(self):
        sdkfolder = join(self.ext, 'SDKs')

        self.sdk_info = sdk.get_sdk_info(sdkfolder, target_platform, True)

        # TODO: Make sure this check works for all platforms
        if not self.sdk_info:
            if not self.verbose:
                # Do it again, with verbose on, so that we can get more info straight away:
                sdk.get_sdk_info(sdkfolder, target_platform, True)

            url = "https://github.com/defold/defold/blob/dev/README_BUILD.md#important-prerequisite---platform-sdks"
            self._log(f"Failed to get sdk info for platform {target_platform}.")
            self._log(f" * Is the local sdk setup correctly?")
            self._log(f" * Or have you called `install_sdk`?")
            self._log(f"We recommend you follow the setup guide found here: {url}")
            sys.exit(1)

        if self.verbose:
            print("SDK info:")
            pprint.pprint(self.sdk_info)


        result = sdk.test_sdk(target_platform, self.sdk_info, verbose = self.verbose)
        if not result:
            self.fatal("Failed sdk check")

        cmake = shutil.which('cmake')
        if not cmake:
            self.fatal("CMake not found in PATH")
        self._log(f"Found CMake: {cmake}")

        ninja = shutil.which('ninja')
        if not ninja:
            self.fatal("Ninja not found in PATH")
        self._log(f"Found Ninja: {ninja}")

        args = ["cmake", f"-DTARGET_PLATFORM={target_platform}", "-P", join(self.defold_root, "scripts/cmake/check_install.cmake")]
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
        run.env_command(self._form_env(), args + plf_args + self.waf_options, cwd = cwd)

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
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_WIN32_SDK_10), join(win32_sdk_folder, 'WindowsKits', '10') )
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_WIN32_TOOLCHAIN), join(win32_sdk_folder, 'MicrosoftVisualStudio14.0'), strip_components=0 )

        if target_platform in ('js-web', 'wasm-web', 'wasm_pthread-web'):
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

        if 'linux' in self.host:
            package = sdk.PACKAGES_LINUX_X86_64_TOOLCHAIN
            if self.host == 'arm64-linux':
                package = sdk.PACKAGES_LINUX_ARM64_TOOLCHAIN

            download_sdk(self, '%s/%s.tar.xz' % (self.package_path, package), join(sdkfolder, self.host, sdk.PACKAGES_LINUX_CLANG), format='J')

        if target_platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios') and 'linux' in self.host:
            if not os.path.exists(join(sdkfolder, self.host, sdk.PACKAGES_LINUX_CLANG, 'cctools')):
                download_sdk(self, '%s/%s.tar.gz' % (self.package_path, PACKAGES_CCTOOLS_PORT), join(sdkfolder, self.host, sdk.PACKAGES_LINUX_CLANG), force_extract=True)

        build_private.install_sdk(self, target_platform)

    def _activate_ems(self, emsdk, bin_dir, version):
        run.env_command(self._form_env(), [join(emsdk, 'emsdk'), 'activate', version, '--embedded'])

        # prewarm the cache
        # Although this method might be more "correct", it also takes 10 minutes more than we'd like on CI
        #run.env_command(self._form_env(), ['%s/embuilder.py' % self._form_ems_path(), 'build', 'SYSTEM', 'MINIMAL'])
        # .. so we stick with the old version of prewarming

        # Compile a file warm up the emscripten caches (libc etc)
        c_file = tempfile.mktemp(suffix='.c')
        exe_file = tempfile.mktemp(suffix='.js')
        with open(c_file, 'w') as f:
            f.write('int main() { return 0; }')
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

    def _add_files_to_zip(self, zip, paths, basedir=None, topfolder=None):
        for p in paths:
            if not os.path.isfile(p):
                continue
            an = p
            if basedir:
                an = os.path.relpath(p, basedir)
            if topfolder:
                an = os.path.join(topfolder, an)
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
    def _package_platform_sdk(self, platform):
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

            def _findlibs(libdir):
                paths = os.listdir(libdir)
                paths = [os.path.join(libdir, x) for x in paths if os.path.splitext(x)[1] in ('.a', '.dylib', '.so', '.lib', '.dll')]
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

            # Dynamo libs
            libdir = os.path.join(self.dynamo_home, 'lib/%s' % platform)
            paths = _findlibs(libdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)
            # External libs
            libdir = os.path.join(self.dynamo_home, 'ext/lib/%s' % platform)
            paths = _findlibs(libdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            if platform in ['armv7-android', 'arm64-android']:
                # Android Jars (Dynamo)
                jardir = os.path.join(self.dynamo_home, 'share/java')
                paths = _findjars(jardir, ('android.jar', 'dlib.jar', 'r.jar'))
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

                # Android Jars (external)
                external_jars = ("android-support-multidex.jar",
                                 "androidx-multidex.jar",
                                 "glfw_android.jar")
                jardir = os.path.join(self.dynamo_home, 'ext/share/java')
                paths = _findjars(jardir, external_jars)
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # Win32 resource files
            if platform in ['win32', 'x86_64-win32']:
                engine_rc = os.path.join(self.dynamo_home, 'lib/%s/defold.ico' % platform)
                defold_ico = os.path.join(self.dynamo_home, 'lib/%s/engine.rc' % platform)
                self._add_files_to_zip(zip, [engine_rc, defold_ico], self.dynamo_home, topfolder)

            # new cmake support. it outputs to x86-win32 folder
            if platform == 'win32':
                libdir = os.path.join(self.dynamo_home, 'lib/x86-win32')
                paths = _findlibs(libdir)
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

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

            if platform in ['js-web']:
                # JavaScript files
                # js-web-pre-x files
                for subdir in ['share', 'lib/js-web/js/', 'ext/lib/js-web/js/']:
                    jsdir = os.path.join(self.dynamo_home, subdir)
                    paths = _findjslibs(jsdir)
                    self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

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
            if platform in ('x86_64-macos','arm64-macos','x86_64-linux','arm64-linux','x86_64-win32'): # needed for the linux build server
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

                # bob pipeline classes
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

        strip = "strip"
        if 'android' in self.target_platform:
            ANDROID_NDK_ROOT = os.path.join(sdkfolder,'android-ndk-r%s' % sdk.ANDROID_NDK_VERSION)

            ANDROID_HOST = 'linux' if sys.platform == 'linux' else 'darwin'
            strip = "%s/toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-strip" % (ANDROID_NDK_ROOT, ANDROID_HOST)

        if self.target_platform in ('x86_64-macos','arm64-macos','arm64-ios','x86_64-ios') and 'linux' == sys.platform:
            strip = os.path.join(sdkfolder, 'linux', sdk.PACKAGES_LINUX_CLANG, 'bin', 'x86_64-apple-darwin19-strip')

        run.shell_command("%s %s" % (strip, path))
        return True

    def archive_engine(self):
        sha1 = self._git_sha1()
        full_archive_path = join(sha1, 'engine', self.target_platform).replace('\\', '/')
        share_archive_path = join(sha1, 'engine', 'share').replace('\\', '/')
        java_archive_path = join(sha1, 'engine', 'share', 'java').replace('\\', '/')
        dynamo_home = self.dynamo_home
        self.full_archive_path = full_archive_path

        bin_dir = self.build_utility.get_binary_path()
        lib_dir = self.target_platform

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

        for n in ['dmengine', 'dmengine_release', 'dmengine_headless']:
            for engine_name in format_exes(n, self.target_platform):
                engine = join(bin_dir, engine_name)
                self.upload_to_archive(engine, '%s/%s' % (full_archive_path, engine_name))
                engine_stripped = join(bin_dir, engine_name + "_stripped")
                shutil.copy2(engine, engine_stripped)
                if self._strip_engine(engine_stripped):
                    self.upload_to_archive(engine_stripped, '%s/stripped/%s' % (full_archive_path, engine_name))
                if 'win32' in self.target_platform:
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
                    self.upload_to_archive(engine_symbols, '%s/%s.debug.wasm' % (full_archive_path, engine_name))
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
                lib_path = join(dynamo_home, 'lib', lib_dir, lib_name)
                self.upload_to_archive(lib_path, '%s/%s' % (full_archive_path, lib_name))

        sdkpath, sdk_sig_path = self._package_platform_sdk(self.target_platform)
        self.upload_to_archive(sdkpath, '%s/defoldsdk.zip' % full_archive_path)
        self.upload_to_archive(sdk_sig_path, '%s/defoldsdk.sha256' % full_archive_path)

    def _can_run_tests(self):
        supported_tests = {}
        # E.g. on win64, we can test multiple platforms
        supported_tests['x86_64-win32'] = ['win32', 'x86_64-win32', 'arm64-nx64', 'x86_64-ps4', 'x86_64-ps5']
        supported_tests['arm64-macos'] = ['x86_64-macos', 'arm64-macos', 'wasm-web', 'wasm_pthread-web', 'js-web']
        supported_tests['x86_64-macos'] = ['x86_64-macos', 'wasm-web', 'wasm_pthread-web', 'js-web']

        return self.target_platform in supported_tests.get(self.host, []) or self.host == self.target_platform

    def _get_build_flags(self):
        supports_tests = self._can_run_tests()
        skip_tests = '--skip-tests' if self.skip_tests or not supports_tests else ''
        skip_codesign = '--skip-codesign' if self.skip_codesign else ''
        disable_ccache = '--disable-ccache' if self.disable_ccache else ''
        generate_compile_commands = '--generate-compile-commands' if self.generate_compile_commands else ''
        return {'skip_tests':skip_tests, 'skip_codesign':skip_codesign, 'disable_ccache':disable_ccache, 'generate_compile_commands':generate_compile_commands, 'prefix':None}

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
        for lib in ENGINE_LIBS:
            cwd = 'engine/%s' % lib
            info = join(self.defold_root, 'engine/%s/sdk_gen.json' % lib)
            if os.path.exists(info):
                self._gen_sdk_source_lib(lib, cmd, join(self.defold_root, cwd), info)

# <- Gen source files
# ------------------------------------------------------------

    def _build_engine_cmd_waf(self, skip_tests, skip_codesign, disable_ccache, generate_compile_commands, prefix):
        prefix = prefix and prefix or self.dynamo_home
        commands = "build install"
        if not self.incremental:
            commands = "distclean configure " + commands
        return '%s %s/ext/bin/waf --prefix=%s %s %s %s %s %s' % (' '.join(self.get_python()), self.dynamo_home, prefix, skip_tests, skip_codesign, disable_ccache, generate_compile_commands, commands)

    def _build_engine_lib_waf(self, args, lib, platform, skip_tests, directory):
        skip_build_tests = []
        if skip_tests and '--skip-build-tests' not in self.waf_options:
            skip_build_tests.append('--skip-tests')
            skip_build_tests.append('--skip-build-tests')
        cwd = join(self.defold_root, '%s/%s' % (directory, lib))
        plf_args = ['--platform=%s' % platform]
        run.env_command(self._form_env(), args + plf_args + self.waf_options + skip_build_tests, cwd = cwd)

    def _find_cmake_build_type(self, options):
        opt_level = 2
        for x in options:
            if '--opt-level=' in x:
                opt_level = x.replace('--opt-level=', '')
                opt_level = int(opt_level)
                break
        if opt_level < 2:
            return 'Debug'
        return 'RelWithDebInfo'

    def _cmake_feature_defines(self):
        defines = []
        handled = set()
        for option in self.waf_options:
            if not option.startswith('--with-'):
                continue
            if option in handled:
                continue
            handled.add(option)
            feature = _CMAKE_FEATURE_FLAG_MAP.get(option)
            if feature:
                defines.append(f"-D{feature}=ON")
            else:
                self._log(f"Warning: CMake build currently ignores '{option}'")
        return defines

    def _build_engine_lib_cmake(self, lib, platform, directory):
        libdir = join(directory, lib)
        builddir = join(libdir, 'build')

        build_type = self._find_cmake_build_type(self.waf_options)
        build_tests = '--skip-build-tests' not in self.waf_options
        supports_tests = build_tests and self._can_run_tests()

        if not self.incremental:
            self._remove_tree(builddir) # distclean

        if not os.path.exists(builddir):
            os.makedirs(builddir)

        env = self._form_env()

        is_verbose = ('-v' in self.waf_options) or ('--verbose' in self.waf_options)
        verbose = '-v' if is_verbose else ''
        test = '' if (self.skip_tests or not supports_tests) else 'run_tests'
        build_test = 'build_tests' if build_tests else ''
        cmake_build_tests = 'ON' if build_tests else 'OFF'
        install = 'install'

        trace = '' #'--trace-expand'

        # ***************************************************************************************
        # generate the build script
        log_cmd_config = f'CMake configure {lib}'
        self.build_tracker.start_command(log_cmd_config)

        cmake_configure_args = f"cmake -S . -B build -GNinja {trace} -DCMAKE_BUILD_TYPE={build_type} -DTARGET_PLATFORM={platform} -DBUILD_TESTS={cmake_build_tests}".split()
        cmake_configure_args += self._cmake_feature_defines()
        run.env_command(self._form_env(), cmake_configure_args, cwd = libdir)

        self.build_tracker.end_command(log_cmd_config)

        # ***************************************************************************************
        # execute the build
        log_cmd_build = f'Ninja build {lib} {build_test}'
        self.build_tracker.start_command(log_cmd_build)

        ninja_build_args = f"ninja all {build_test} {install} {verbose}".split()
        run.env_command(self._form_env(), ninja_build_args, cwd = builddir)

        self.build_tracker.end_command(log_cmd_build)

        # ***************************************************************************************
        # run the build
        if test:
            log_cmd_tests = f'Ninja run_tests {lib}'
            self.build_tracker.start_command(log_cmd_tests)

            ninja_build_args = f"ninja run_tests {verbose}".split()
            run.env_command(self._form_env(), ninja_build_args, cwd = builddir)

            self.build_tracker.end_command(log_cmd_tests)


    def _build_engine_lib(self, args, lib, platform, skip_tests = False, directory = 'engine'):
        self.build_tracker.start_component(lib, platform)
        if lib in CMAKE_SUPPORT:
            if platform == 'win32':
                platform = 'x86-win32'
            self._build_engine_lib_cmake(lib, platform, directory)
        else:
            self._build_engine_lib_waf(args, lib, platform, skip_tests, directory)

        self.build_tracker.end_component(lib, platform)

# For now gradle right in
# - 'com.dynamo.cr/com.dynamo.cr.bob'
# - 'com.dynamo.cr/com.dynamo.cr.test'
# - 'com.dynamo.cr/com.dynamo.cr.common'
# Maybe in the future we consider to move it into install_ext
    def get_gradle_wrapper(self):
        if os.name == 'nt':  # Windows
            return join('.', 'gradlew.bat')
        else:  # Linux, macOS, or other Unix-like OS
            return join('.', 'gradlew')

    def build_bob_light(self):
        self.build_tracker.start_component('bob_light', self.host)

        bob_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        # common_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.common')

        sha1 = self._git_sha1()
        if os.path.exists(os.path.join(self.dynamo_home, 'archive', sha1)):
            run.env_shell_command(self._form_env(), "./scripts/copy.sh", cwd=bob_dir)
        else:
            self.copy_local_bob_artefacts()

        env = self._form_env()

        gradle = self.get_gradle_wrapper()
        gradle_args = []
        if self.verbose:
            gradle_args += ['--info']

        env['GRADLE_OPTS'] = '-Dorg.gradle.parallel=true' #-Dorg.gradle.daemon=true

        # Clean and build the project
        s = run.command(" ".join([gradle, '-Pkeep-bob-uncompressed', 'clean', 'installBobLight'] + gradle_args), cwd = bob_dir, shell = True, env = env)
        if self.verbose:
        	print (s)
        self.build_tracker.end_component('bob_light', self.host)

    def build_engine(self):
        self.check_sdk()

        # We want random folder to thoroughly test bob-light
        # We dont' want it to unpack for _every_ single invocation during the build
        os.environ['DM_BOB_ROOTFOLDER'] = tempfile.mkdtemp(prefix='bob-light-')
        self._log("env DM_BOB_ROOTFOLDER=" + os.environ['DM_BOB_ROOTFOLDER'])

        cmd = self._build_engine_cmd_waf(**self._get_build_flags())
        args = cmd.split()
        host = self.host

        # Make sure we build these for the host platform for the toolchain (bob light)
        for lib in HOST_LIBS:
            self._build_engine_lib(args, lib, host)
        if not self.skip_bob_light:
            # We must build bob-light, which builds content during the engine build
            self.build_bob_light()
        # Target libs to build

        engine_libs = list(ENGINE_LIBS)
        for lib in engine_libs:

            # No need to rebuild a library if it has already been built
            if host == self.target_platform:
                if lib in HOST_LIBS:
                    continue

            if not build_private.is_library_supported(target_platform, lib):
                continue
            self._build_engine_lib(args, lib, target_platform)

        self._build_engine_lib(args, 'extender', target_platform, directory = 'share')
        if not self.skip_docs:
            self.build_docs()
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

        self._log("Copy platform.sdks.json")
        shutil.copyfile(join(self.defold_root, "share", "platform.sdks.json"), join(self.dynamo_home, "platform.sdks.json"))

        if os.path.exists(os.environ['DM_BOB_ROOTFOLDER']):
            print ("Removing", os.environ['DM_BOB_ROOTFOLDER'])
            shutil.rmtree(os.environ['DM_BOB_ROOTFOLDER'])

    def build_external(self):
        flags = self._get_build_flags()
        flags['prefix'] = join(self.defold_root, 'packages')
        cmd = self._build_engine_cmd_waf(**flags)
        args = cmd.split() + ['package']
        for lib in EXTERNAL_LIBS:
            self._build_engine_lib(args, lib, platform=self.target_platform, directory='external')

    def archive_bob(self):
        sha1 = self._git_sha1()
        full_archive_path = join(sha1, 'bob').replace('\\', '/')
        for p in glob(join(self.dynamo_home, 'share', 'java', 'bob.jar')):
            self.upload_to_archive(p, '%s/%s' % (full_archive_path, basename(p)))

    def copy_local_bob_artefacts(self):
        texc_name = format_lib('texc_shared', self.host)
        modelc_name = format_lib('modelc_shared', self.host)
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
                         'ext/share/java/android.jar': 'lib/android.jar'} # this should be the stripped one

        switch_files = {}
        # This dict is being built up and will eventually be used for copying in the end
        # - "type" - what the files are needed for, for error reporting
        #   - pairs of src-file -> dst-file
        artefacts = {'generic': {'share/java/dlib.jar': 'lib/dlib.jar',
                                 'share/java/modelimporter.jar': 'lib/modelimporter.jar',
                                 'share/java/shaderc.jar': 'lib/shaderc.jar',
                                 'share/builtins.zip': 'lib/builtins.zip',
                                 'lib/%s/%s' % (self.host, texc_name): 'lib/%s/%s' % (self.host, texc_name),
                                 'lib/%s/%s' % (self.host, modelc_name): 'lib/%s/%s' % (self.host, modelc_name),
                                 'lib/%s/%s' % (self.host, shaderc_name): 'lib/%s/%s' % (self.host, shaderc_name)},
                     'android-bundling': android_files,
                     'win32-bundling': win32_files,
                     'js-bundling': js_files,
                     'ios-bundling': {},
                     'osx-bundling': macos_files,
                     'linux-bundling': linux_files,
                     'switch-bundling': switch_files}
        # Add dmengine to 'artefacts' procedurally
        for type, plfs in {'android-bundling': [['armv7-android', 'armv7-android'], ['arm64-android', 'arm64-android']],
                           'win32-bundling': [['win32', 'x86-win32'], ['x86_64-win32', 'x86_64-win32']],
                           'js-bundling': [['js-web', 'js-web'], ['wasm-web', 'wasm-web'], ['wasm_pthread-web', 'wasm_pthread-web']],
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
        # common_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.common')
        test_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')

        sha1 = self._git_sha1()
        if os.path.exists(os.path.join(self.dynamo_home, 'archive', sha1)):
            run.env_shell_command(self._form_env(), "./scripts/copy.sh", cwd=bob_dir)
        else:
            self.copy_local_bob_artefacts()

        env = self._form_env()

        gradle = self.get_gradle_wrapper()
        gradle_args = []
        if self.verbose:
            gradle_args += ['--info']

        env['GRADLE_OPTS'] = '-Dorg.gradle.parallel=true' #-Dorg.gradle.daemon=true
        flags = ''
        if self.keep_bob_uncompressed:
            flags = '-Pkeep-bob-uncompressed'

        # Clean and build the project
        run.command(" ".join([gradle, flags, 'clean', 'install'] + gradle_args), cwd=bob_dir, shell = True, env = env)

        # Run tests if not skipped
        if not self.skip_tests:
            run.command(" ".join([gradle, flags, 'testJar'] + gradle_args), cwd = test_dir, shell = True, env = env, stdout = None)


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
        bucket = s3.get_bucket(u.netloc)

        root = urlparse(self.get_archive_path()).path[1:]
        base_prefix = os.path.join(root, sha1)

        platforms = None
        # when we build the sdk in a private repo we only include the private platforms
        if build_private.is_repo_private():
            platforms = build_private.get_target_platforms()
        else:
            platforms = get_target_platforms()

        # For the linux build tools (protoc, dlib_shared etc)
        if 'x86_64-linux' not in platforms:
            platforms.append('x86_64-linux')

        # Since we usually want to use the scripts in this package on a linux machine, we'll unpack
        # it last, in order to preserve unix line endings in the files
        if 'x86_64-linux' in platforms:
            platforms.remove('x86_64-linux')
            platforms.append('x86_64-linux')

        for platform in platforms:
            prefix = os.path.join(base_prefix, 'engine', platform, 'defoldsdk.zip')
            entry = bucket.Object(prefix)

            platform_sdk_zip = tempfile.NamedTemporaryFile(delete = False)
            print ("Downloading", entry.key)
            entry.download_file(platform_sdk_zip.name)
            print ("Downloaded", entry.key, "to", platform_sdk_zip.name)

            self._extract_zip(platform_sdk_zip.name, tempdir)
            print ("Extracted", platform_sdk_zip.name, "to", tempdir)

            os.unlink(platform_sdk_zip.name)
            print ("")

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
        self.upload_to_archive(join(self.defold_root, "share", "platform.sdks.json"), '%s/platform.sdks.json' % sdkurl)

        shutil.rmtree(tempdir)
        print ("Removed", tempdir)

    def build_docs(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        self._log('Building API docs')
        cwd = join(self.defold_root, 'engine/docs')
        python_cmd = ' '.join(self.get_python())
        commands = 'build install'
        if not self.incremental:
            commands = "distclean configure " + commands
        cmd = '%s %s/ext/bin/waf configure --prefix=%s %s %s' % (python_cmd, self.dynamo_home, self.dynamo_home, skip_tests, commands)
        run.env_command(self._form_env(), [python_cmd, './scripts/bundle.py', 'docs', '--docs-dir', cwd], cwd = join(self.defold_root, 'editor'))
        run.env_command(self._form_env(), cmd.split() + self.waf_options, cwd = cwd)
        with open(join(self.dynamo_home, 'share', 'ref-doc.zip'), 'wb') as f:
            self._ziptree(join(self.dynamo_home, 'share', 'doc'), outfile = f, directory = join(self.dynamo_home, 'share'))


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

        if self.skip_codesign:
            cmd.append('--skip-codesign')
        else:
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

    def save_env(self):
        if not self.save_env_path:
            self._log("No --save-env-path set when trying to save environment export")
            return

        env = self._form_env()
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
        print ('Setting up shell with DEFOLD_HOME, DYNAMO_HOME, PATH, JAVA_HOME, and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH (where applicable) set')

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
        # We handle the stable channel seperately, since we want it to point
        # to the editor-dev release (which uses the latest stable engine).
        editor_channel = None
        if self.channel == "stable":
            editor_channel = "editor-alpha"
        else:
            editor_channel = self.channel or "stable"

        u = urlparse(self.get_archive_path())
        hostname = u.hostname
        bucket = s3.get_bucket(hostname)

        editor_archive_path = urlparse(self.get_archive_path(editor_channel)).path

        release_sha1 = releases[0]['sha1']

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
        #   redirect: /editor2/channels/editor-alpha/Defold-x86_64-macos.dmg -> /archive/<sha1>/editor-alpha/Defold-x86_64-macos.dmg
        for name in ['Defold-arm64-macos.dmg', 'Defold-x86_64-macos.dmg', 'Defold-x86_64-win32.zip', 'Defold-x86_64-linux.tar.gz', 'Defold-x86_64-linux.zip']:
            key_name = 'editor2/channels/%s/%s' % (editor_channel, name)
            redirect = '%s/%s/%s/editor2/%s' % (editor_archive_path, release_sha1, editor_channel, name)
            self._log('Creating link from %s -> %s' % (key_name, redirect))
            obj = bucket.Object(key_name)
            obj.copy_from(
                CopySource={'Bucket': hostname, 'Key': key_name},
                WebsiteRedirectLocation=redirect
            )

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
        engine_channel = None
        editor_channel = None
        engine_sha1 = None
        editor_sha1 = None
        if self.channel in ('stable','beta'):
            engine_sha1 = self._git_sha1()

        elif self.channel in ('editor-alpha',):
            engine_channel = 'stable'
            editor_channel = self.channel
            editor_sha1 = self._git_sha1()
            engine_sha1 = self._git_sha1(self.version) # engine version

        else:
            engine_sha1 = self._git_sha1()
            engine_channel = self.channel
            editor_channel = self.channel

        if not editor_sha1:
            editor_sha1 = engine_sha1

        body  = "Defold version %s\n" % self.version
        body += "Engine channel=%s sha1: %s\n" % (engine_channel, engine_sha1)
        body += "Editor channel=%s sha1: %s\n" % (editor_channel, editor_sha1)
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
        tag_name = None
        is_editor_branch = False
        engine_channel = None
        editor_channel = None
        prerelease = True
        if self.channel in ('stable', 'beta', 'alpha'):
            engine_channel = self.channel
            editor_channel = self.channel
            prerelease = self.channel in ('alpha', 'beta')
            tag_name = self.create_tag()
            self.push_tag(tag_name)

        elif self.channel in ('editor-alpha',):
            # We update the stable release with new editor builds
            engine_channel = 'stable'
            editor_channel = self.channel
            prerelease = False
            tag_name = self.compose_tag_name(self.version, engine_channel)
            is_editor_branch = True

        if tag_name is not None and not is_editor_branch:
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
            release_name = 'v%s - %s' % (self.version, engine_channel or self.channel)
            release_to_github.release(self, tag_name, release_sha1, releases[0], release_name=release_name, body=body, prerelease=prerelease, editor_only=is_editor_branch)

        # Release to steam for stable only
        # if tag_name and (self.channel == 'editor-alpha'):
        #     self.release_to_steam()

    # E.g. use with ./scripts/build.py release_to_github --github-token=$CITOKEN --channel=editor-alpha
    # on a branch with the correct sha1 (e.g. beta or editor-dev)
    def release_to_github(self):
        engine_channel = None
        release_sha1 = None
        is_editor_branch = False
        prerelease = True
        if self.channel in ('editor-alpha',):
            engine_channel = 'stable'
            is_editor_branch = True
            prerelease = False
            release_sha1 = self._git_sha1()
        else:
            release_sha1 = self._git_sha1(self.version) # engine version
            if self.channel in ('stable', 'beta'):
                prerelease = False

        tag_name = self.compose_tag_name(self.version, engine_channel or self.channel)

        if tag_name is not None and not is_editor_branch:
            pattern = self._get_tag_pattern_from_tag_name(self.channel, tag_name)
            releases = s3.get_tagged_releases(self.get_archive_path(), pattern, num_releases=1)
        else:
            # e.g. editor-dev releases
            releases = [s3.get_single_release(self.get_archive_path(), self.version, self._git_sha1())]

        body = self._get_github_release_body()
        release_name = 'v%s - %s' % (self.version, engine_channel or self.channel)

        release_to_github.release(self, tag_name, release_sha1, releases[0], release_name=release_name, body=body, prerelease=prerelease, editor_only=is_editor_branch)

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
        editor_channel = "editor-alpha"
        engine_channel = "stable"
        tag_name = self.compose_tag_name(self.version, engine_channel)
        archive_path = self.get_archive_path(editor_channel)
        urls = self.get_editor_urls_from_s3(archive_path, tag_name)
        release_to_steam.release(self, urls)


    # Use with ./scripts/build.py release_to_egs --version=1.4.8
    def release_to_egs(self):
        editor_channel = "editor-alpha"
        engine_channel = "stable"
        tag_name = self.compose_tag_name(self.version, engine_channel)
        archive_path = self.get_archive_path(editor_channel)
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
        # * rarely used platforms: armv7-android , js-web  and x86-win32
        # * headless builds
        pattern = re.compile(
            r'(^|/)editor(2)*/|/defoldsdk\.zip$|/launcher(\.exe)*$|/(armv7-android|js-web|x86-win32)(/|$)|headless'
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
        jdk = 'jdk-25+36'
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

    def _form_env(self):
        env = os.environ.copy()

        host = self.host

        ld_library_path = 'DYLD_LIBRARY_PATH' if 'macos' in self.host else 'LD_LIBRARY_PATH'
        ld_library_paths = ['%s/lib/%s' % (self.dynamo_home, self.target_platform),
                            '%s/ext/lib/%s' % (self.dynamo_home, self.host)]
        if self.host in ['x86_64-linux', 'arm64-linux']:
            ld_library_paths.append('%s/ext/SDKs/%s/%s/%s/lib' % (self.dynamo_home, self.host, sdk.PACKAGES_LINUX_CLANG, PACKAGES_TAPI_VERSION))

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

        env['DEFOLD_HOME'] = self.defold_home
        env['DYNAMO_HOME'] = self.dynamo_home

        android_host = self.host
        if 'win32' in android_host:
            android_host = 'windows'
        paths = os.path.pathsep.join(['%s/bin/%s' % (self.dynamo_home, self.target_platform),
                                      '%s/bin' % (self.dynamo_home),
                                      '%s/ext/bin' % self.dynamo_home,
                                      '%s/ext/bin/%s' % (self.dynamo_home, host)])

        env['PATH'] = paths + os.path.pathsep + env['PATH']

        # This trickery is needed for the bash to properly inherit the PATH that we've set here
        # See /etc/profile for further details
        is_mingw = env.get('MSYSTEM', '') in ('MINGW64',)
        if is_mingw:
            env['ORIGINAL_PATH'] = env['PATH']

        env['MAVEN_OPTS'] = '-Xms256m -Xmx700m -XX:MaxPermSize=1024m'

        # Force 32-bit python 2.7 on darwin.
        env['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
        env['VERSIONER_PYTHON_VERSION'] = '2.7'

        if self.no_colors:
            env['NOCOLOR'] = '1'

        # XMLHttpRequest Emulation for node.js
        xhr2_path = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR, 'xhr2', 'package', 'lib')
        if 'NODE_PATH' in env:
            env['NODE_PATH'] = xhr2_path + os.path.pathsep + env['NODE_PATH']
        else:
            env['NODE_PATH'] = xhr2_path

        return env

if __name__ == '__main__':
    s3.init_boto_data_path()
    usage = '''usage: %prog [options] command(s)

Commands:
distclean        - Removes the DYNAMO_HOME folder
install_ext      - Install external packages
install_sdk      - Install sdk
install_waf      - Install waf
sync_archive     - Sync engine artifacts from S3
build_engine     - Build engine
archive_engine   - Archive engine (including builtins) to path specified with --archive-path
build_editor2    - Build editor
archive_editor2  - Archive editor to path specified with --archive-path
download_editor2 - Download editor bundle (zip)
build_bob        - Build bob with native libraries included for cross platform deployment
build_bob_light  - Build a lighter version of bob (mostly used for test content during builds)
archive_bob      - Archive bob to path specified with --archive-path
build_docs       - Build documentation
build_builtins   - Build builtin content archive
bump             - Bump version number
release          - Release editor
shell            - Start development shell
smoke_test       - Test editor and engine in combination
local_smoke      - Test run smoke test using local dev environment
gen_sdk_source   - Regenerate the dmSDK bindings from our C sdk

Multiple commands can be specified

To pass on arbitrary options to waf: build.py OPTIONS COMMANDS -- WAF_OPTIONS
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      choices = get_target_platforms(),
                      help = 'Target platform')

    parser.add_option('--skip-tests', dest='skip_tests',
                      action = 'store_true',
                      default = False,
                      help = 'Skip unit-tests. Default is false')

    parser.add_option('--keep-bob-uncompressed', dest='keep_bob_uncompressed',
                    action = 'store_true',
                    default = False,
                    help = 'do not apply compression to bob.jar. Default is false')

    parser.add_option('--skip-codesign', dest='skip_codesign',
                      action = 'store_true',
                      default = False,
                      help = 'skip code signing (engine and editor). Default is false')

    parser.add_option('--skip-docs', dest='skip_docs',
                      action = 'store_true',
                      default = False,
                      help = 'skip building docs when building the engine. Default is false')

    parser.add_option('--incremental', dest='incremental',
                      action = 'store_true',
                      default = False,
                      help = 'skip reconfigure/distclean when building the engine. Default is false')

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
                      default = None,
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

    options, all_args = parser.parse_args()

    args = list(filter(lambda x: x[:2] != '--', all_args))
    waf_options = list(filter(lambda x: x[:2] == '--', all_args))

    if len(args) == 0:
        parser.error('No command specified')

    target_platform = options.target_platform
    if not options.target_platform:
        target_platform = get_host_platform()

    c = Configuration(dynamo_home = os.environ.get('DYNAMO_HOME', None),
                      target_platform = target_platform,
                      skip_tests = options.skip_tests,
                      keep_bob_uncompressed = options.keep_bob_uncompressed,
                      skip_codesign = options.skip_codesign,
                      skip_docs = options.skip_docs,
                      incremental = options.incremental,
                      skip_builtins = options.skip_builtins,
                      skip_bob_light = options.skip_bob_light,
                      disable_ccache = options.disable_ccache,
                      generate_compile_commands = options.generate_compile_commands,
                      no_colors = options.no_colors,
                      archive_domain = options.archive_domain,
                      package_path = options.package_path,
                      set_version = options.set_version,
                      channel = options.channel,
                      engine_artifacts = options.engine_artifacts,
                      waf_options = waf_options,
                      save_env_path = options.save_env_path,
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

    needs_dynamo_home = True
    for cmd in args:
        if cmd in ['shell', 'save_env']:
            needs_dynamo_home = False
            break
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
