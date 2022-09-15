#!/usr/bin/env python
# Copyright 2020-2022 The Defold Foundation
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
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
sys.path.append(os.path.join(normpath(join(dirname(abspath(__file__)), '..')), "build_tools"))

import shutil, zipfile, re, itertools, json, platform, math, mimetypes
import optparse, subprocess, urllib, urllib.parse, tempfile, time
import github
import run
import s3
import sdk
import release_to_github
import BuildUtility
import http_cache
from urllib.parse import urlparse
from tarfile import TarFile
from glob import glob
from threading import Thread, Event
from queue import Queue
from configparser import ConfigParser

BASE_PLATFORMS = [  'x86_64-linux',
                    'x86_64-macos', #'arm64-macos',
                    'win32', 'x86_64-win32',
                    'x86_64-ios', 'arm64-ios',
                    'armv7-android', 'arm64-android',
                    'js-web', 'wasm-web']

sys.dont_write_bytecode = True
try:
    import build_nx64
    sys.modules['build_private'] = build_nx64
except Exception:
    pass

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

def get_target_platforms():
    return BASE_PLATFORMS + build_private.get_target_platforms()


PACKAGES_ALL="protobuf-3.20.1 waf-2.0.3 junit-4.6 protobuf-java-3.20.1 openal-1.1 maven-3.0.1 ant-1.9.3 vecmath vpx-1.7.0 luajit-2.1.0-633f265 tremolo-0.0.8 defold-robot-0.7.0 bullet-2.77 libunwind-395b27b68c5453222378bc5fe4dab4c6db89816a jctest-0.8 vulkan-1.1.108".split()
PACKAGES_HOST="cg-3.1 vpx-1.7.0 luajit-2.1.0-633f265 tremolo-0.0.8".split()
PACKAGES_IOS_X86_64="protobuf-3.20.1 luajit-2.1.0-633f265 tremolo-0.0.8 bullet-2.77".split()
PACKAGES_IOS_64="protobuf-3.20.1 luajit-2.1.0-633f265 tremolo-0.0.8 bullet-2.77 MoltenVK-1.0.41".split()
PACKAGES_MACOS_X86_64="protobuf-3.20.1 luajit-2.1.0-633f265 vpx-1.7.0 tremolo-0.0.8 sassc-5472db213ec223a67482df2226622be372921847 bullet-2.77 spirv-cross-2018-08-07 glslc-v2018.0 MoltenVK-1.0.41 luac-32-5.1.5".split()
PACKAGES_MACOS_ARM64="protobuf-3.20.1 luajit-2.1.0-633f265 vpx-1.7.0 tremolo-0.0.8 sassc-5472db213ec223a67482df2226622be372921847 bullet-2.77 spirv-cross-2018-08-07 glslc-v2018.0 MoltenVK-1.0.41 luac-32-5.1.5".split()
PACKAGES_WIN32="protobuf-3.20.1 luajit-2.1.0-633f265 openal-1.1 glut-3.7.6 bullet-2.77 vulkan-1.1.108".split()
PACKAGES_WIN32_64="protobuf-3.20.1 luajit-2.1.0-633f265 openal-1.1 glut-3.7.6 sassc-5472db213ec223a67482df2226622be372921847 bullet-2.77 spirv-cross-2018-08-07 glslc-v2018.0 vulkan-1.1.108 luac-32-5.1.5".split()
PACKAGES_LINUX_64="protobuf-3.20.1 luajit-2.1.0-633f265 sassc-5472db213ec223a67482df2226622be372921847 bullet-2.77 spirv-cross-2018-08-07 glslc-v2018.0 vulkan-1.1.108 luac-32-5.1.5".split()
PACKAGES_ANDROID="protobuf-3.20.1 android-support-multidex androidx-multidex android-31 luajit-2.1.0-633f265 tremolo-0.0.8 bullet-2.77 libunwind-8ba86320a71bcdc7b411070c0c0f101cf2131cf2".split()
PACKAGES_ANDROID_64="protobuf-3.20.1 android-support-multidex androidx-multidex android-31 luajit-2.1.0-633f265 tremolo-0.0.8 bullet-2.77 libunwind-8ba86320a71bcdc7b411070c0c0f101cf2131cf2".split()
PACKAGES_EMSCRIPTEN="protobuf-3.20.1 bullet-2.77".split()
PACKAGES_NODE_MODULES="xhr2-0.1.0".split()

DMSDK_PACKAGES_ALL="vectormathlibrary-r1649".split()

CDN_PACKAGES_URL=os.environ.get("DM_PACKAGES_URL", None)
DEFAULT_ARCHIVE_DOMAIN=os.environ.get("DM_ARCHIVE_DOMAIN", "d.defold.com")
DEFAULT_RELEASE_REPOSITORY=os.environ.get("DM_RELEASE_REPOSITORY") if os.environ.get("DM_RELEASE_REPOSITORY") else release_to_github.get_current_repo()

PACKAGES_TAPI_VERSION="tapi1.6"
PACKAGES_NODE_MODULE_XHR2="xhr2-v0.1.0"
PACKAGES_ANDROID_NDK="android-ndk-r20"
PACKAGES_ANDROID_SDK="android-sdk"
ANDROID_TARGET_API_LEVEL=31
ANDROID_BUILD_TOOLS_VERSION="32.0.0"
PACKAGES_CCTOOLS_PORT="cctools-port-darwin19-6c438753d2252274678d3e0839270045698c159b-linux"

NODE_MODULE_LIB_DIR = os.path.join("ext", "lib", "node_modules")
EMSCRIPTEN_VERSION_STR = "2.0.11"
EMSCRIPTEN_SDK = "sdk-{0}-64bit".format(EMSCRIPTEN_VERSION_STR)
PACKAGES_EMSCRIPTEN_SDK="emsdk-{0}".format(EMSCRIPTEN_VERSION_STR)
SHELL = os.environ.get('SHELL', 'bash')
# Don't use WSL from the msys/cygwin terminal
if os.environ.get('TERM','') in ('cygwin',):
    if 'WD' in os.environ:
        SHELL= '%s\\bash.exe' % os.environ['WD'] # the binary directory

ENGINE_LIBS = "testmain ddf particle glfw graphics lua hid input physics resource extension script render rig gameobject gui sound liveupdate crash gamesys tools record iap push iac webview profiler facebook engine sdk".split()

EXTERNAL_LIBS = "bullet3d".split()

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
    elif 'js-web' in platform:
        prefix = ''
        suffix = ['.js']
    elif 'wasm-web' in platform:
        prefix = ''
        suffix = ['.js', '.wasm']
    elif platform in ['arm64-nx64']:
        prefix = ''
        suffix = ['.nss', '.nso']
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

        for i in range(worker_count):
            w = Thread(target = self.worker)
            w.setDaemon(True)
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
    def __init__(self, dynamo_home = None,
                 target_platform = None,
                 skip_tests = False,
                 skip_codesign = False,
                 skip_docs = False,
                 skip_builtins = False,
                 skip_bob_light = False,
                 disable_ccache = False,
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
                 windows_cert = None,
                 windows_cert_pass = None,
                 verbose = False):

        if sys.platform == 'win32':
            home = os.environ['USERPROFILE']
        else:
            home = os.environ['HOME']

        self.dynamo_home = dynamo_home if dynamo_home else join(os.getcwd(), 'tmp', 'dynamo_home')
        self.ext = join(self.dynamo_home, 'ext')
        self.dmsdk = join(self.dynamo_home, 'sdk')
        self.defold = normpath(join(dirname(abspath(__file__)), '..'))
        self.defold_root = os.getcwd()
        self.host = get_host_platform()
        self.target_platform = target_platform

        self.build_utility = BuildUtility.BuildUtility(self.target_platform, self.host, self.dynamo_home)

        self.skip_tests = skip_tests
        self.skip_codesign = skip_codesign
        self.skip_docs = skip_docs
        self.skip_builtins = skip_builtins
        self.skip_bob_light = skip_bob_light
        self.disable_ccache = disable_ccache
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
        self.windows_cert = windows_cert
        self.windows_cert_pass = windows_cert_pass
        self.verbose = verbose

        if self.github_token is None:
            self.github_token = os.environ.get("GITHUB_TOKEN")

        self.thread_pool = None
        self.futures = []

        if version is None:
            with open('VERSION', 'r') as f:
                self.version = f.readlines()[0].strip()

        self._create_common_dirs()

    def __del__(self):
        if len(self.futures) > 0:
            print('ERROR: Pending futures (%d)' % len(self.futures))
            os._exit(5)

    def get_python(self):
        if 'macos' in self.host and 'arm64' == platform.machine():
            if 'x86_64-macos' == self.target_platform:
                return 'arch -x86_64 python'
        return 'python'

    def _create_common_dirs(self):
        for p in ['ext/lib/python', 'share', 'lib/js-web/js', 'lib/wasm-web/js']:
            self._mkdirs(join(self.dynamo_home, p))

    def _mkdirs(self, path):
        if not os.path.exists(path):
            os.makedirs(path)

    def _log(self, msg):
        print(str(msg))
        sys.stdout.flush()
        sys.stderr.flush()

    def distclean(self):
        if os.path.exists(self.dynamo_home):
            self._log('Removing %s' % self.dynamo_home)
            shutil.rmtree(self.dynamo_home)

        for lib in ['dlib','texc']+ENGINE_LIBS:
            builddir = join(self.defold_root, 'engine/%s/build' % lib)
            if os.path.exists(builddir):
                self._log('Removing %s' % builddir)
                shutil.rmtree(builddir)

        # Recreate dirs
        self._create_common_dirs()
        self._log('distclean done.')

    def _extract_tgz(self, file, path):
        self._log('Extracting %s to %s' % (file, path))
        version = sys.version_info
        suffix = os.path.splitext(file)[1]
        # Avoid a bug in python 2.7 (fixed in 2.7.2) related to not being able to remove symlinks: http://bugs.python.org/issue10761
        if self.host == 'x86_64-linux' and version[0] == 2 and version[1] == 7 and version[2] < 2:
            fmts = {'.gz': 'z', '.xz': 'J', '.bzip2': 'j'}
            run.env_command(self._form_env(), ['tar', 'xf%s' % fmts.get(suffix, 'z'), file], cwd = path)
        else:
            fmts = {'.gz': 'gz', '.xz': 'xz', '.bzip2': 'bz2'}
            tf = TarFile.open(file, 'r:%s' % fmts.get(suffix, 'gz'))
            tf.extractall(path)
            tf.close()

    def _extract_tgz_rename_folder(self, src, target_folder, strip_components=1, format=None):
        src = src.replace('\\', '/')

        force_local = ''
        if os.environ.get('GITHUB_SHA', None) is not None and os.environ.get('TERM', '') == 'cygwin':
            force_local = '--force-local' # to make tar not try to "connect" because it found a colon in the source file

        self._log('Extracting %s to %s/' % (src, target_folder))
        parentdir, dirname = os.path.split(target_folder)
        old_dir = os.getcwd()
        os.chdir(parentdir)
        if not os.path.exists(dirname):
            os.makedirs(dirname)

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


    def install_ext(self):
        def make_package_path(root, platform, package):
            return join(root, 'packages', package) + '-%s.tar.gz' % platform

        def make_package_paths(root, platform, packages):
            return [make_package_path(root, platform, package) for package in packages]

        self._check_package_path()

        print("Installing common packages")
        for p in PACKAGES_ALL:
            self._extract_tgz(make_package_path(self.defold_root, 'common', p), self.ext)

        for p in DMSDK_PACKAGES_ALL:
            self._extract_tgz(make_package_path(self.defold_root, 'common', p), self.dmsdk)

        # TODO: Make sure the order of install does not affect the outcome!

        platform_packages = {
            'win32':          PACKAGES_WIN32,
            'x86_64-win32':   PACKAGES_WIN32_64,
            'x86_64-linux':   PACKAGES_LINUX_64,
            'x86_64-macos':   PACKAGES_MACOS_X86_64,
            #'arm64-macos':    PACKAGES_MACOS_ARM64,
            'arm64-ios':      PACKAGES_IOS_64,
            'x86_64-ios':     PACKAGES_IOS_X86_64,
            'armv7-android':  PACKAGES_ANDROID,
            'arm64-android':  PACKAGES_ANDROID_64,
            'js-web':         PACKAGES_EMSCRIPTEN,
            'wasm-web':       PACKAGES_EMSCRIPTEN
        }

        base_platforms = self.get_base_platforms()
        target_platform = self.target_platform
        other_platforms = set(platform_packages.keys()).difference(set(base_platforms), set([target_platform, self.host]))

        if target_platform in ['js-web', 'wasm-web']:
            node_modules_dir = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR)
            for package in PACKAGES_NODE_MODULES:
                path = join(self.defold_root, 'packages', package + '.tar.gz')
                name = package.split('-')[0]
                self._extract_tgz(path, join(node_modules_dir, name))

        installed_packages = set()

        for platform in other_platforms:
            packages = platform_packages.get(platform, [])
            package_paths = make_package_paths(self.defold_root, platform, packages)
            print("Installing %s packages " % platform)
            for path in package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(package_paths)

        for base_platform in self.get_base_platforms():
            packages = list(PACKAGES_HOST) + build_private.get_install_host_packages(base_platform)
            packages.extend(platform_packages.get(base_platform, []))
            package_paths = make_package_paths(self.defold_root, base_platform, packages)
            package_paths = [path for path in package_paths if path not in installed_packages]
            if len(package_paths) != 0:
                print("Installing %s packages" % base_platform)
                for path in package_paths:
                    self._extract_tgz(path, self.ext)
                installed_packages.update(package_paths)

        # For easier usage with the extender server, we want the linux protoc tool available
        if target_platform in ('x86_64-macos', 'x86_64-win32', 'x86_64-linux'):
            protobuf_packages = filter(lambda x: "protobuf" in x, PACKAGES_HOST)
            package_paths = make_package_paths(self.defold_root, 'x86_64-linux', protobuf_packages)
            print("Installing %s packages " % 'x86_64-linux')
            for path in package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(package_paths)

        target_packages = platform_packages.get(self.target_platform, []) + build_private.get_install_target_packages(self.target_platform)
        target_package_paths = make_package_paths(self.defold_root, self.target_platform, target_packages)
        target_package_paths = [path for path in target_package_paths if path not in installed_packages]

        if len(target_package_paths) != 0:
            print("Installing %s packages" % self.target_platform)
            for path in target_package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(target_package_paths)

        print("Installing python wheels")
        run.env_command(self._form_env(), [self.get_python(), '-m', 'pip', '-q', '-q', 'install', '-t', join(self.ext, 'lib', 'python'), 'requests', 'pyaml'])
        for whl in glob(join(self.defold_root, 'packages', '*.whl')):
            self._log('Installing %s' % basename(whl))
            run.env_command(self._form_env(), [self.get_python(), '-m', 'pip', '-q', '-q', 'install', '--upgrade', '-t', join(self.ext, 'lib', 'python'), whl])

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

    def check_sdk(self):
        sdkfolder = join(self.ext, 'SDKs')

        self.sdk_info = sdk.get_sdk_info(sdkfolder, target_platform)

        # We currently only support a subset of platforms using this mechanic
        if platform in ('x86_64-macos', 'arm64-macos','x86_64-ios','arm64-ios'):
            if not self.sdk_info:
                print("Couldn't find any sdks")
                print("We recommend you follow the packaging steps found here: %s" % "https://github.com/defold/defold/blob/dev/scripts/package/README.md#packaging-the-sdks")
                print("Then run './scripts/build.py --package-path=<local_folder_or_url> install_ext --platform=<platform>=%s'" % self.target_platform)
                sys.exit(1)

            print("Using SDKS:", self.sdk_info)

    def install_sdk(self):
        sdkfolder = join(self.ext, 'SDKs')

        target_platform = self.target_platform
        if target_platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
            # macOS SDK
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_MACOS_SDK), join(sdkfolder, sdk.PACKAGES_MACOS_SDK))
            download_sdk(self,'%s/%s.darwin.tar.gz' % (self.package_path, sdk.PACKAGES_XCODE_TOOLCHAIN), join(sdkfolder, sdk.PACKAGES_XCODE_TOOLCHAIN))

        if target_platform in ('arm64-ios', 'x86_64-ios'):
            # iOS SDK
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_IOS_SDK), join(sdkfolder, sdk.PACKAGES_IOS_SDK))
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_IOS_SIMULATOR_SDK), join(sdkfolder, sdk.PACKAGES_IOS_SIMULATOR_SDK))

        if 'win32' in target_platform or ('win32' in self.host):
            win32_sdk_folder = join(self.ext, 'SDKs', 'Win32')
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_WIN32_SDK_10), join(win32_sdk_folder, 'WindowsKits', '10') )
            download_sdk(self,'%s/%s.tar.gz' % (self.package_path, sdk.PACKAGES_WIN32_TOOLCHAIN), join(win32_sdk_folder, 'MicrosoftVisualStudio14.0'), strip_components=0 )

            # On OSX, the file system is already case insensitive, so no need to duplicate the files as we do on the extender server

        if target_platform in ('armv7-android', 'arm64-android'):
            host = self.host
            if 'win32' in host:
                host = 'windows'
            elif 'linux' in host:
                host = 'linux'
            elif 'macos' in host:
                host = 'darwin' # our packages are still called darwin

            # Android NDK
            download_sdk(self, '%s/%s-%s-x86_64.tar.gz' % (self.package_path, PACKAGES_ANDROID_NDK, host), join(sdkfolder, PACKAGES_ANDROID_NDK))
            # Android SDK
            download_sdk(self, '%s/%s-%s-android-%s-%s.tar.gz' % (self.package_path, PACKAGES_ANDROID_SDK, host, ANDROID_TARGET_API_LEVEL, ANDROID_BUILD_TOOLS_VERSION), join(sdkfolder, PACKAGES_ANDROID_SDK))

        if 'linux' in self.host:
            download_sdk(self, '%s/%s.tar.xz' % (self.package_path, sdk.PACKAGES_LINUX_TOOLCHAIN), join(sdkfolder, 'linux', sdk.PACKAGES_LINUX_CLANG), format='J')

        if target_platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios') and 'linux' in self.host:
            if not os.path.exists(join(sdkfolder, 'linux', sdk.PACKAGES_LINUX_CLANG, 'cctools')):
                download_sdk(self, '%s/%s.tar.gz' % (self.package_path, PACKAGES_CCTOOLS_PORT), join(sdkfolder, 'linux', sdk.PACKAGES_LINUX_CLANG), force_extract=True)

        build_private.install_sdk(self, target_platform)

    def get_ems_dir(self):
        return join(self.ext, 'SDKs', 'emsdk-' + EMSCRIPTEN_VERSION_STR)

    def _form_ems_path(self):
        upstream = join(self.get_ems_dir(), 'upstream', 'emscripten')
        if os.path.exists(upstream):
            return upstream
        return join(self.get_ems_dir(), 'fastcomp', 'emscripten')

    def install_ems(self):
        # TODO: should eventually be moved to install_sdk
        emsDir = self.get_ems_dir()

        os.environ['EMSCRIPTEN'] = self._form_ems_path()
        os.environ['EM_CONFIG'] = join(self.get_ems_dir(), '.emscripten')
        os.environ['EM_CACHE'] = join(self.get_ems_dir(), 'emscripten_cache')

        if os.path.isdir(emsDir):
            print("Emscripten is already installed:", emsDir)
        else:
            self._check_package_path()
            platform_map = {'x86_64-linux':'linux',
                            'x86_64-macos':'darwin',
                            'arm64-macos':'darwin',
                            'x86_64-win32':'win32'}
            path = join(self.package_path, '%s-%s.tar.gz' % (PACKAGES_EMSCRIPTEN_SDK, platform_map.get(self.host, self.host)))
            path = self.get_local_or_remote_file(path)
            self._extract(path, join(self.ext, 'SDKs'))

        config = os.environ['EM_CONFIG']
        if not os.path.isfile(config):
            self.activate_ems()

    def activate_ems(self):
        version = EMSCRIPTEN_VERSION_STR
        if 'fastcomp' in self._form_ems_path():
            version += "-fastcomp"
        run.env_command(self._form_env(), [join(self.get_ems_dir(), 'emsdk'), 'activate', version, '--embedded'])

        # prewarm the cache
        # Although this method might be more "correct", it also takes 10 minutes more than we'd like on CI
        #run.env_command(self._form_env(), ['%s/embuilder.py' % self._form_ems_path(), 'build', 'SYSTEM', 'MINIMAL'])
        # .. so we stick with the old version of prewarming

        # Compile a file warm up the emscripten caches (libc etc)
        c_file = tempfile.mktemp(suffix='.c')
        exe_file = tempfile.mktemp(suffix='.js')
        with open(c_file, 'w') as f:
            f.write('int main() { return 0; }')
        run.env_command(self._form_env(), ['%s/emcc' % self._form_ems_path(), c_file, '-o', '%s' % exe_file])

    def check_ems(self):
        config = join(self.get_ems_dir(), '.emscripten')
        err = False
        if not os.path.isfile(config):
            print('No .emscripten file.')
            err = True
        emsDir = self.get_ems_dir()
        if not os.path.isdir(emsDir):
            print('Emscripten tools not installed.')
            err = True
        if err:
            print('Consider running install_ems')

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
        return self.target_platform in ['x86_64-linux', 'x86_64-macos', 'arm64-macos', 'x86_64-win32']

    # package the native SDK, return the path to the zip file
    def _package_platform_sdk(self, platform):
        with open(join(self.dynamo_home, 'defoldsdk.zip'), 'wb') as outfile:
            zip = zipfile.ZipFile(outfile, 'w', zipfile.ZIP_DEFLATED)

            topfolder = 'defoldsdk'
            defold_home = os.path.normpath(os.path.join(self.dynamo_home, '..', '..'))

            # Includes
            includes = []
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "sdk/include")):
                for file in files:
                    if file.endswith('.h'):
                        includes.append(os.path.join(root, file))

            # proto _ddf.h + "res_*.h"
            for root, dirs, files in os.walk(os.path.join(self.dynamo_home, "include")):
                for file in files:
                    if file.endswith('.h') and ('ddf' in file or file.startswith('res_')):
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

            # Android Jars (Dynamo)
            jardir = os.path.join(self.dynamo_home, 'share/java')
            paths = _findjars(jardir, ('android.jar', 'dlib.jar', 'r.jar'))
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # Android Jars (external)
            external_jars = ("android-support-multidex.jar",
                             "androidx-multidex.jar",
                             "android.jar")
            jardir = os.path.join(self.dynamo_home, 'ext/share/java')
            paths = _findjars(jardir, external_jars)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # Win32 resource files
            engine_rc = os.path.join(self.dynamo_home, 'lib/%s/defold.ico' % platform)
            defold_ico = os.path.join(self.dynamo_home, 'lib/%s/engine.rc' % platform)
            self._add_files_to_zip(zip, [engine_rc, defold_ico], self.dynamo_home, topfolder)

            # JavaScript files
            # js-web-pre-x files
            jsdir = os.path.join(self.dynamo_home, 'share')
            paths = _findjslibs(jsdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)
            # libraries for js-web
            jsdir = os.path.join(self.dynamo_home, 'lib/js-web/js/')
            paths = _findjslibs(jsdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)
            # libraries for wasm-web
            jsdir = os.path.join(self.dynamo_home, 'lib/wasm-web/js/')
            paths = _findjslibs(jsdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # .proto files
            for d in ['share/proto/', 'ext/include/google/protobuf']:
                protodir = os.path.join(self.dynamo_home, d)
                paths = _findfiles(protodir, ('.proto',))
                self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # pipeline tools
            if platform in ('x86_64-macos','arm64-macos','x86_64-linux','x86_64-win32'): # needed for the linux build server
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
            return outfile.name
        return None

    def build_platform_sdk(self):
        # Helper function to make it easier to build a platform sdk locally
        try:
            path = self._package_platform_sdk(self.target_platform)
        except Exception as e:
            print ("Failed to package sdk for platform %s: %s" % (self.target_platform, e))
        else:
            print ("Wrote %s" % path)

    def build_builtins(self):
        with open(join(self.dynamo_home, 'share', 'builtins.zip'), 'wb') as f:
            self._ziptree(join(self.dynamo_home, 'content', 'builtins'), outfile = f, directory = join(self.dynamo_home, 'content'))

    def _strip_engine(self, path):
        """ Strips the debug symbols from an executable """
        if self.target_platform not in ['x86_64-linux','x86_64-macos','arm64-macos','arm64-ios','x86_64-ios','armv7-android','arm64-android']:
            return False

        sdkfolder = join(self.ext, 'SDKs')

        strip = "strip"
        if 'android' in self.target_platform:
            ANDROID_NDK_VERSION = '20'
            ANDROID_NDK_ROOT = os.path.join(sdkfolder,'android-ndk-r%s' % ANDROID_NDK_VERSION)
            ANDROID_GCC_VERSION = '4.9'
            if target_platform == 'armv7-android':
                ANDROID_PLATFORM = 'arm-linux-androideabi'
            elif target_platform == 'arm64-android':
                ANDROID_PLATFORM = 'aarch64-linux-android'

            ANDROID_HOST = 'linux' if sys.platform == 'linux' else 'darwin'
            strip = "%s/toolchains/%s-%s/prebuilt/%s-x86_64/bin/%s-strip" % (ANDROID_NDK_ROOT, ANDROID_PLATFORM, ANDROID_GCC_VERSION, ANDROID_HOST, ANDROID_PLATFORM)

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
        if self.target_platform in ['x86_64-linux', 'x86_64-macos', 'arm64-macos', 'x86_64-win32']:
            launcher_name = format_exes("launcher", self.target_platform)[0]
            launcherbin = join(bin_dir, launcher_name)
            self.upload_to_archive(launcherbin, '%s/%s' % (full_archive_path, launcher_name))

        # upload gdc tool on desktop platforms
        if self.is_desktop_target():
            gdc_name = format_exes("gdc", self.target_platform)[0]
            gdc_bin = join(bin_dir, gdc_name)
            self.upload_to_archive(gdc_bin, '%s/%s' % (full_archive_path, gdc_name))

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

        if self.target_platform == 'x86_64-linux':
            # NOTE: It's arbitrary for which platform we archive dlib.jar. Currently set to linux 64-bit
            self.upload_to_archive(join(dynamo_home, 'share', 'java', 'dlib.jar'), '%s/dlib.jar' % (java_archive_path))

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
            libs = ['dlib', 'texc', 'particle']
            for lib in libs:
                lib_name = format_lib('%s_shared' % (lib), self.target_platform)
                lib_path = join(dynamo_home, 'lib', lib_dir, lib_name)
                self.upload_to_archive(lib_path, '%s/%s' % (full_archive_path, lib_name))

        sdkpath = self._package_platform_sdk(self.target_platform)
        self.upload_to_archive(sdkpath, '%s/defoldsdk.zip' % full_archive_path)

    def _get_build_flags(self):
        supported_tests = {}
        # E.g. on win64, we can test multiple platforms
        supported_tests['x86_64-win32'] = ['win32', 'x86_64-win32', 'arm64-nx64']

        supports_tests = self.target_platform in supported_tests.get(self.host, []) or self.host == self.target_platform
        skip_tests = '--skip-tests' if self.skip_tests or not supports_tests else ''
        skip_codesign = '--skip-codesign' if self.skip_codesign else ''
        disable_ccache = '--disable-ccache' if self.disable_ccache else ''
        return {'skip_tests':skip_tests, 'skip_codesign':skip_codesign, 'disable_ccache':disable_ccache, 'prefix':None}

    def get_base_platforms(self):
        # Base platforms is the platforms to build the base libs for.
        # The base libs are the libs needed to build bob, i.e. contains compiler code.

        platform_dependencies = {'x86_64-macos': ['x86_64-macos'],
                                 #'arm64-macos': ['arm64-macos'],
                                 'x86_64-linux': [],
                                 'x86_64-win32': ['win32']}

        platforms = list(platform_dependencies.get(self.host, [self.host]))

        if not self.host in platforms:
            platforms.append(self.host)

        return platforms

    def _build_engine_cmd(self, skip_tests, skip_codesign, disable_ccache, prefix):
        prefix = prefix and prefix or self.dynamo_home
        return '%s %s/ext/bin/waf --prefix=%s %s %s %s distclean configure build install' % (self.get_python(), self.dynamo_home, prefix, skip_tests, skip_codesign, disable_ccache)

    def _build_engine_lib(self, args, lib, platform, skip_tests = False, dir = 'engine'):
        self._log('Building %s for %s' % (lib, platform))
        skip_build_tests = []
        if skip_tests and '--skip-build-tests' not in self.waf_options:
            skip_build_tests.append('--skip-tests')
            skip_build_tests.append('--skip-build-tests')
        cwd = join(self.defold_root, '%s/%s' % (dir, lib))
        plf_args = ['--platform=%s' % platform]
        run.env_command(self._form_env(), args + plf_args + self.waf_options + skip_build_tests, cwd = cwd)

    def build_bob_light(self):
        self._log('Building bob light')

        bob_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        common_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.common')

        sha1 = self._git_sha1()
        if os.path.exists(os.path.join(self.dynamo_home, 'archive', sha1)):
            run.env_shell_command(self._form_env(), "./scripts/copy.sh", cwd = bob_dir)
        else:
            self.copy_local_bob_artefacts()

        ant = join(self.dynamo_home, 'ext/share/ant/bin/ant')
        ant_args = ['-logger', 'org.apache.tools.ant.listener.AnsiColorLogger']
        if self.verbose:
            ant_args += ['-v']

        env = self._form_env()
        env['ANT_OPTS'] = '-Dant.logger.defaults=%s/ant-logger-colors.txt' % join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')
        env['DM_BOB_EXT_LIB_DIR'] = os.path.join(common_dir, 'ext')
        env['DM_BOB_CLASS_DIR'] = os.path.join(bob_dir, 'build')

        s = run.command(" ".join([ant, 'clean', 'compile-bob-light'] + ant_args),
                                    cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob'), shell = True, env = env)
        if self.verbose:
            print (s)

        s = run.command(" ".join([ant, 'install-bob-light'] + ant_args), cwd = bob_dir, shell = True, env = env)
        if self.verbose:
            print (s)

    def build_engine(self):
        self.check_sdk()

        # We want random folder to thoroughly test bob-light
        # We dont' want it to unpack for _every_ single invocation during the build
        os.environ['DM_BOB_ROOTFOLDER'] = tempfile.mkdtemp(prefix='bob-light-')
        self._log("env DM_BOB_ROOTFOLDER=" + os.environ['DM_BOB_ROOTFOLDER'])

        cmd = self._build_engine_cmd(**self._get_build_flags())
        args = cmd.split()
        host = self.host
        # Make sure we build these for the host platform for the toolchain (bob light)
        for lib in ['dlib', 'texc']:
            skip_tests = host != self.target_platform
            self._build_engine_lib(args, lib, host, skip_tests = skip_tests)
        if not self.skip_bob_light:
            # We must build bob-light, which builds content during the engine build
            self.build_bob_light()
        # Target libs to build

        engine_libs = list(ENGINE_LIBS)
        if host != self.target_platform:
            engine_libs.insert(0, 'dlib')
            if self.is_desktop_target():
                engine_libs.insert(1, 'texc')
        for lib in engine_libs:
            if not build_private.is_library_supported(target_platform, lib):
                continue
            self._build_engine_lib(args, lib, target_platform)
        self._build_engine_lib(args, 'extender', target_platform, dir = 'share')
        if not self.skip_docs:
            self.build_docs()
        if not self.skip_builtins:
            self.build_builtins()
        if '--static-analyze' in self.waf_options:
            scan_output_dir = os.path.normpath(os.path.join(os.environ['DYNAMO_HOME'], '..', '..', 'static_analyze'))
            report_dir = os.path.normpath(os.path.join(os.environ['DYNAMO_HOME'], '..', '..', 'report'))
            run.command([self.get_python(), './scripts/scan_build_gather_report.py', '-o', report_dir, '-i', scan_output_dir])
            print("Wrote report to %s. Open with 'scan-view .' or 'python -m SimpleHTTPServer'" % report_dir)
            shutil.rmtree(scan_output_dir)

        if os.path.exists(os.environ['DM_BOB_ROOTFOLDER']):
            print ("Removing", os.environ['DM_BOB_ROOTFOLDER'])
            shutil.rmtree(os.environ['DM_BOB_ROOTFOLDER'])

    def build_external(self):
        flags = self._get_build_flags()
        flags['prefix'] = join(self.defold_root, 'packages')
        cmd = self._build_engine_cmd(**flags)
        args = cmd.split() + ['package']
        for lib in EXTERNAL_LIBS:
            self._build_engine_lib(args, lib, platform=self.target_platform, dir='external')

    def archive_bob(self):
        sha1 = self._git_sha1()
        full_archive_path = join(sha1, 'bob').replace('\\', '/')
        for p in glob(join(self.dynamo_home, 'share', 'java', 'bob.jar')):
            self.upload_to_archive(p, '%s/%s' % (full_archive_path, basename(p)))

    def copy_local_bob_artefacts(self):
        texc_name = format_lib('texc_shared', self.host)
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
                    ['x86_64-macos', 'x86_64-macos'],
                    ['arm64-macos', 'arm64-macos']]:
            luajit_path = join(cwd, '../../packages/luajit-2.1.0-633f265-%s.tar.gz' % (plf[0]))
            if not os.path.exists(luajit_path):
                add_missing(plf[1], "package '%s' could not be found" % (luajit_path))
            else:
                self._extract(luajit_path, luajit_dir)
                for name in ('luajit-32', 'luajit-64'):
                    luajit_exe = format_exes(name, plf[0])[0]
                    src = join(luajit_dir, 'bin/%s/%s' % (plf[0], luajit_exe))
                    if not os.path.exists(src):
                        continue
                    self._copy(src, join(cwd, 'libexec/%s/%s' % (plf[1], luajit_exe)))

        win32_files = dict([['ext/lib/%s/%s.dll' % (plf[0], lib), 'lib/%s/%s.dll' % (plf[1], lib)] for lib in ['OpenAL32', 'wrap_oal'] for plf in [['win32', 'x86-win32'], ['x86_64-win32', 'x86_64-win32']]])
        macos_files = dict([['ext/lib/%s/lib%s.dylib' % (plf[0], lib), 'lib/%s/lib%s.dylib' % (plf[1], lib)] for lib in [] for plf in [['x86_64-macos', 'x86_64-macos'], ['arm64-macos', 'arm64-macos']]])
        linux_files = dict([['ext/lib/%s/lib%s.so' % (plf[0], lib), 'lib/%s/lib%s.so' % (plf[1], lib)] for lib in [] for plf in [['x86_64-linux', 'x86_64-linux']]])
        js_files = {}
        android_files = {'share/java/classes.dex': 'lib/classes.dex',
                         'ext/share/java/android.jar': 'lib/android.jar'}
        switch_files = {}
        # This dict is being built up and will eventually be used for copying in the end
        # - "type" - what the files are needed for, for error reporting
        #   - pairs of src-file -> dst-file
        artefacts = {'generic': {'share/java/dlib.jar': 'lib/dlib.jar',
                                 'share/builtins.zip': 'lib/builtins.zip',
                                 'lib/%s/%s' % (self.host, texc_name): 'lib/%s/%s' % (self.host, texc_name)},
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
                           'js-bundling': [['js-web', 'js-web'], ['wasm-web', 'wasm-web']],
                           'ios-bundling': [['arm64-ios', 'arm64-ios'], ['x86_64-ios', 'x86_64-ios']],
                           'osx-bundling': [['x86_64-macos', 'x86_64-macos'], ['arm64-macos', 'arm64-macos']],
                           'linux-bundling': [['x86_64-linux', 'x86_64-linux']],
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
        cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')

        bob_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        common_dir = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.common')

        sha1 = self._git_sha1()
        if os.path.exists(os.path.join(self.dynamo_home, 'archive', sha1)):
            run.env_shell_command(self._form_env(), "./scripts/copy.sh", cwd = bob_dir)
        else:
            self.copy_local_bob_artefacts()

        env = self._form_env()

        ant = join(self.dynamo_home, 'ext/share/ant/bin/ant')
        ant_args = ['-logger', 'org.apache.tools.ant.listener.AnsiColorLogger']
        env['ANT_OPTS'] = '-Dant.logger.defaults=%s/ant-logger-colors.txt' % join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')

        env = self._form_env()
        env['ANT_OPTS'] = '-Dant.logger.defaults=%s/ant-logger-colors.txt' % join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')
        env['DM_BOB_EXT_LIB_DIR'] = os.path.join(common_dir, 'ext')
        env['DM_BOB_CLASS_DIR'] = os.path.join(bob_dir, 'build')

        run.command(" ".join([ant, 'clean', 'compile'] + ant_args), cwd = bob_dir, shell = True, env = env)

        run.command(" ".join([ant, 'install'] + ant_args), cwd = bob_dir, shell = True, env = env)

        if not self.skip_tests:
            cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob.test')
            args = [ant, 'test-clean', 'test'] + ant_args
            run.command(" ".join(args), cwd = cwd, shell = True, env = env, stdout = None)


    def build_sdk(self):
        tempdir = tempfile.mkdtemp() # where the sdk ends up

        sha1 = self._git_sha1()
        u = urlparse(self.get_archive_path())
        bucket = s3.get_bucket(u.netloc)

        root = urlparse(self.get_archive_path()).path[1:]
        base_prefix = os.path.join(root, sha1)

        platforms = get_target_platforms()

        # Since we usually want to use the scripts in this package on a linux machine, we'll unpack
        # it last, in order to preserve unix line endings in the files
        if 'x86_64-linux' in platforms:
            platforms.remove('x86_64-linux')
            platforms.append('x86_64-linux')

        for platform in platforms:
            prefix = os.path.join(base_prefix, 'engine', platform, 'defoldsdk.zip')
            entry = bucket.get_key(prefix)

            if entry is None:
                raise Exception("Could not find sdk: %s" % prefix)

            platform_sdk_zip = tempfile.NamedTemporaryFile(delete = False)
            print ("Downloading", entry.key)
            entry.get_contents_to_filename(platform_sdk_zip.name)
            print ("Downloaded", entry.key, "to", platform_sdk_zip.name)

            self._extract_zip(platform_sdk_zip.name, tempdir)
            print ("Extracted", platform_sdk_zip.name, "to", tempdir)

            os.unlink(platform_sdk_zip.name)
            print ("")

        # Due to an issue with how the attributes are preserved, let's go through the bin/ folders
        # and set the flags explicitly
        for root, dirs, files in os.walk(tempdir):
            for f in files:
                p = os.path.join(root, f)
                if '/bin/' in p:
                    os.chmod(p, 0o755)
                    st = os.stat(p)

        treepath = os.path.join(tempdir, 'defoldsdk')
        sdkpath = self._ziptree(treepath, directory=tempdir)
        print ("Packaged defold sdk from", tempdir, "to", sdkpath)

        sdkurl = join(sha1, 'engine').replace('\\', '/')
        self.upload_to_archive(sdkpath, '%s/defoldsdk.zip' % sdkurl)

        shutil.rmtree(tempdir)
        print ("Removed", tempdir)

    def build_docs(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        self._log('Building API docs')
        cwd = join(self.defold_root, 'engine/docs')
        cmd = '%s %s/ext/bin/waf configure --prefix=%s %s distclean configure build install' % (self.get_python(), self.dynamo_home, self.dynamo_home, skip_tests)
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
        dmg_file = "Defold-%s.dmg" % self.target_platform
        zip_path = join(self.defold_root, 'editor', 'target', 'editor', zip_file)
        dmg_path = join(self.defold_root, 'editor', 'target', 'editor', dmg_file)
        if os.path.exists(zip_path): self.upload_to_archive(zip_path, '%s/%s' % (full_archive_path, zip_file))
        if os.path.exists(dmg_path): self.upload_to_archive(dmg_path, '%s/%s' % (full_archive_path, dmg_file))
        self.wait_uploads()

    def run_editor_script(self, cmd):
        cwd = join(self.defold_root, 'editor')
        run.env_command(self._form_env(), cmd, cwd = cwd)

    def build_editor2(self):
        cmd = [self.get_python(), './scripts/bundle.py',
               '--engine-artifacts=%s' % self.engine_artifacts,
               '--archive-domain=%s' % self.archive_domain,
               'build']

        if self.skip_tests:
            cmd.append("--skip-tests")

        self.run_editor_script(cmd)

    def bundle_editor2(self):
        if not self.channel:
            raise Exception('No channel provided when bundling the editor')

        cmd = [self.get_python(), './scripts/bundle.py',
               '--platform=%s' % self.target_platform,
               '--version=%s' % self.version,
               '--channel=%s' % self.channel,
               '--engine-artifacts=%s' % self.engine_artifacts,
               '--archive-domain=%s' % self.archive_domain,
               'bundle']
        self.run_editor_script(cmd)

    def sign_editor2(self):
        editor_bundle_dir = join(self.defold_root, 'editor', 'target', 'editor')
        cmd = [self.get_python(), './scripts/bundle.py',
               '--platform=%s' % self.target_platform,
               '--bundle-dir=%s' % editor_bundle_dir,
               '--archive-domain=%s' % self.archive_domain,
               'sign']
        if self.skip_codesign:
            cmd.append('--skip-codesign')
        else:
            if self.windows_cert:
                cmd.append('--windows-cert=%s' % self.windows_cert)
            if self.windows_cert_pass:
                cmd.append("--windows-cert-pass=%s" % self.windows_cert_pass)
            if self.codesigning_identity:
                cmd.append('--codesigning-identity="%s"' % self.codesigning_identity)
        self.run_editor_script(cmd)

    def notarize_editor2(self):
        if self.target_platform not in ('x86_64-macos', 'arm64-macos'):
            return

        editor_bundle_dir = join(self.defold_root, 'editor', 'target', 'editor')
        # create dmg installer
        cmd = ['./scripts/bundle.py',
               '--platform=%s' % self.target_platform,
               '--bundle-dir=%s' % editor_bundle_dir,
               '--archive-domain=%s' % self.archive_domain,
               'installer']
        if self.skip_codesign:
            cmd.append('--skip-codesign')
        else:
            if self.codesigning_identity:
                cmd.append('--codesigning-identity="%s"' % self.codesigning_identity)
        self.run_editor_script(cmd)

        # notarize dmg
        editor_dmg = join(editor_bundle_dir, 'Defold-%s.dmg' % self.target_platform)
        cmd = ['./scripts/notarize.py',
               editor_dmg,
               self.notarization_username,
               self.notarization_password,
               self.notarization_itc_provider]
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

    def shell(self):
        print ('Setting up shell with DYNAMO_HOME, PATH, ANDROID_HOME and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH (where applicable) set')
        if "win32" in self.host:
            preexec_fn = None
        else:
            preexec_fn = self.check_ems

        if "macos" in self.host and "arm" in platform.processor():
            print ('Detected Apple M1 CPU - running shell with x86 architecture')
            # Submit as string because on POSIX subsequent tokens are passed to the shell - not the arch command.
            # See: https://docs.python.org/3.10/library/subprocess.html#popen-constructor
            args = 'arch -arch x86_64 %s -l' % SHELL
        else:
            args = [SHELL, '-l']

        process = subprocess.Popen(args, env=self._form_env(), shell=True)
        output = process.communicate()[0]

        if process.returncode != 0:
            self._log(str(output, encoding='utf-8'))
            sys.exit(process.returncode)

# ------------------------------------------------------------
# BEGIN: RELEASE
#
    def _get_tag_name(self, version, channel):
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

        tag_name = self._get_tag_name(self.version, self.channel)

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
        model = {'releases': releases,
                 'has_releases': True}

        model['release'] = { 'channel': "Unknown", 'version': self.version }
        if self.channel:
            model['release']['channel'] = self.channel.capitalize()

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

        editor_download_url = "https://%s%s/%s/%s/editor2/" % (hostname, editor_archive_path, release_sha1, editor_channel)
        model['release'] = {'editor': [ dict(name='macOS 10.12', url=editor_download_url + 'Defold-x86_64-macos.dmg'),
                                        dict(name='Windows', url=editor_download_url + 'Defold-x86_64-win32.zip'),
                                        dict(name='Ubuntu 18.04+', url=editor_download_url + 'Defold-x86_64-linux.zip')] }

        page = None;
        with open(os.path.join("scripts", "resources", "downloads.html"), 'r') as file:
            page = file.read()

        # NOTE: We upload index.html to /CHANNEL/index.html
        # The root-index, /index.html, redirects to /stable/index.html
        self._log('Uploading %s/index.html' % self.channel)
        html = page % {'model': json.dumps(model)}

        key = bucket.new_key('%s/index.html' % self.channel)
        key.content_type = 'text/html'
        key.set_contents_from_string(html)

        self._log('Uploading %s/info.json' % self.channel)
        key = bucket.new_key('%s/info.json' % self.channel)
        key.content_type = 'application/json'
        key.set_contents_from_string(json.dumps({'version': self.version,
                                                 'sha1' : release_sha1}))

        # Editor update-v3.json
        key_v3 = bucket.new_key('editor2/channels/%s/update-v3.json' % self.channel)
        key_v3.content_type = 'application/json'
        self._log("Updating channel '%s' for update-v3.json: %s" % (self.channel, key_v3))
        key_v3.set_contents_from_string(json.dumps({'sha1': release_sha1}))

        # Set redirect urls so the editor can always be downloaded without knowing the latest sha1.
        # Used by www.defold.com/download
        # For example;
        #   redirect: /editor2/channels/editor-alpha/Defold-x86_64-macos.dmg -> /archive/<sha1>/editor-alpha/Defold-x86_64-macos.dmg
        for name in ['Defold-x86_64-macos.dmg', 'Defold-x86_64-win32.zip', 'Defold-x86_64-linux.zip']:
            key_name = 'editor2/channels/%s/%s' % (editor_channel, name)
            redirect = '%s/%s/%s/editor2/%s' % (editor_archive_path, release_sha1, editor_channel, name)
            self._log('Creating link from %s -> %s' % (key_name, redirect))
            key = bucket.new_key(key_name)
            key.set_redirect(redirect)

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
        if self.channel in ('stable', 'beta', 'alpha'):
            tag_name = self.create_tag()
            self.push_tag(tag_name)

        if tag_name is not None:
            # NOTE: Each of the main branches has a channel (stable, beta and alpha)
            #       and each of them have their separate tag patterns ("1.2.183" vs "1.2.183-beta"/"1.2.183-alpha")
            channel_pattern = ''
            if self.channel != 'stable':
                channel_pattern = '-' + self.channel
            platform_pattern = build_private.get_tag_suffix() # E.g. '' or 'switch'
            if platform_pattern:
                platform_pattern = '-' + platform_pattern

            # Example tags:
            #   1.2.184, 1.2.184-alpha, 1.2.184-beta
            #   1.2.184-switch, 1.2.184-alpha-switch, 1.2.184-beta-switch
            pattern = r"(\d+\.\d+\.\d+%s)$" % (channel_pattern + platform_pattern)

            releases = s3.get_tagged_releases(self.get_archive_path(), pattern)
        else:
            # e.g. editor-dev releases
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
            release_to_github.release(self, tag_name, release_sha1, releases[0])

#
# END: RELEASE
# ------------------------------------------------------------

    def release_to_github(self):
        tag_name = self._get_tag_name(self.version, self.channel)
        release_sha1 = self._git_sha1(self.version)
        releases = [s3.get_single_release(self.get_archive_path(''), self.version, release_sha1)]
        release_to_github.release(self, tag_name, release_sha1, releases[0])

    def sync_archive(self):
        u = urlparse(self.get_archive_path())
        bucket_name = u.hostname
        bucket = s3.get_bucket(bucket_name)

        local_dir = os.path.join(self.dynamo_home, 'archive')
        self._mkdirs(local_dir)

        if not self.thread_pool:
            self.thread_pool = ThreadPool(8)

        def download(key, path):
            self._log('s3://%s/%s -> %s' % (bucket_name, key.name, path))
            key.get_contents_to_filename(path)

        futures = []
        sha1 = self._git_sha1()
        # Only s3 is supported (scp is deprecated)
        # The pattern is used to filter out:
        # * Editor files
        # * Defold SDK files
        # * launcher files, used to launch editor2
        pattern = re.compile(r'(^|/)editor(2)*/|/defoldsdk\.zip$|/launcher(\.exe)*$')
        prefix = s3.get_archive_prefix(self.get_archive_path(), self._git_sha1())
        for key in bucket.list(prefix = prefix):
            rel = os.path.relpath(key.name, prefix)

            if not pattern.search(rel):
                p = os.path.join(local_dir, sha1, rel)
                self._mkdirs(os.path.dirname(p))
                f = Future(self.thread_pool, download, key, p)
                futures.append(f)

        for f in futures:
            f()

# ------------------------------------------------------------
# BEGIN: SMOKE TEST
#
    def _download_editor2(self, channel, sha1):
        bundles = {
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
        jdk = 'jdk-11.0.15+10'
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
        args = [java, '-cp', jar] + vmargs + [main, '--preferences=../../editor/test/resources/smoke_test_prefs.json', game_project]
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
        return u.path

    def download_from_archive(self, src_path, dst_file):
        url = join(self.get_archive_path(), src_path)
        self.download_from_s3(dst_file, url)


    def upload_to_archive(self, src_file, dst_path):
        url = join(self.get_archive_path(), dst_path).replace("\\", "/")
        self._log("Uploading %s -> %s" % (src_file, url))
        self.upload_to_s3(src_file, url)

        # create redirect so that the old s3 paths still work
        # s3://d.defold.com/archive/channel/sha1/engine/* -> http://d.defold.com/archive/sha1/engine/*
        bucket = s3.get_bucket(urlparse(url).netloc)
        redirect_key = self.get_archive_redirect_key(url)
        redirect_url = url.replace("s3://", "http://")
        key = bucket.new_key(redirect_key)
        key.set_redirect(redirect_url)
        self._log("Redirecting %s -> %s : %s" % (url, redirect_key, redirect_url))

    def download_from_s3(self, path, url):
        url = url.replace('\\', '/')
        self._log('Downloading %s -> %s' % (url, path))
        u = urlparse(url)

        if u.scheme == 's3':
            self._mkdirs(os.path.dirname(path))
            from boto.s3.key import Key

            bucket = s3.get_bucket(u.netloc)
            k = Key(bucket)
            k.key = u.path
            k.get_contents_to_filename(path)
            self._log('Downloaded %s -> %s' % (url, path))
        else:
            raise Exception('Unsupported url %s' % (url))


    def upload_to_s3(self, path, url):
        url = url.replace('\\', '/')
        self._log('Uploading %s -> %s' % (path, url))

        u = urlparse(url)

        if u.scheme == 's3':
            bucket = s3.get_bucket(u.netloc)

            if not self.thread_pool:
                self.thread_pool = ThreadPool(8)

            p = u.path
            if p[-1] == '/':
                p += basename(path)

            def upload_singlefile():
                key = bucket.new_key(p)
                key.set_contents_from_filename(path)
                self._log('Uploaded %s -> %s' % (path, url))

            def upload_multipart():
                headers = {}
                contenttype, _ = mimetypes.guess_type(path)
                if contenttype is not None:
                    headers['Content-Type'] = contenttype

                mp = bucket.initiate_multipart_upload(p, headers=headers)

                source_size = os.stat(path).st_size
                chunksize = 64 * 1024 * 1024 # 64 MiB
                chunkcount = int(math.ceil(source_size / float(chunksize)))

                def upload_part(filepath, part, offset, size):
                    with open(filepath, 'rb') as fhandle:
                        fhandle.seek(offset)
                        mp.upload_part_from_file(fp=fhandle, part_num=part, size=size)

                _threads = []
                for i in range(chunkcount):
                    part = i + 1
                    offset = i * chunksize
                    remaining = source_size - offset
                    size = min(chunksize, remaining)
                    args = {'filepath': path, 'part': part, 'offset': offset, 'size': size}

                    self._log('Uploading #%d %s -> %s' % (i + 1, path, url))
                    _thread = Thread(target=upload_part, kwargs=args)
                    _threads.append(_thread)
                    _thread.start()

                for i in range(chunkcount):
                    _threads[i].join()
                    self._log('Uploaded #%d %s -> %s' % (i + 1, path, url))

                if len(mp.get_all_parts()) == chunkcount:
                    mp.complete_upload()
                    self._log('Uploaded %s -> %s' % (path, url))
                else:
                    mp.cancel_upload()
                    self._log('Failed to upload %s -> %s' % (path, url))

            f = None
            if sys.platform == 'win32':
                f = Future(self.thread_pool, upload_singlefile)
            else:
                f = Future(self.thread_pool, upload_multipart)
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
        if self.host == 'x86_64-linux':
            ld_library_paths.append('%s/ext/SDKs/linux/%s/%s/lib' % (self.dynamo_home, sdk.PACKAGES_LINUX_CLANG, PACKAGES_TAPI_VERSION))

        env[ld_library_path] = os.path.pathsep.join(ld_library_paths)

        pythonpaths = ['%s/lib/python' % self.dynamo_home,
                      '%s/build_tools' % self.defold,
                      '%s/ext/lib/python' % self.dynamo_home]
        env['PYTHONPATH'] = os.path.pathsep.join(pythonpaths)

        env['DYNAMO_HOME'] = self.dynamo_home

        env['ANDROID_HOME'] = os.path.join(self.dynamo_home, 'ext', 'SDKs', 'android-sdk')

        go_root = '%s/ext/go/%s/go' % (self.dynamo_home, self.target_platform)

        android_host = self.host
        if 'win32' in android_host:
            android_host = 'windows'
        paths = os.path.pathsep.join(['%s/bin/%s' % (self.dynamo_home, self.target_platform),
                                      '%s/bin' % (self.dynamo_home),
                                      '%s/ext/bin' % self.dynamo_home,
                                      '%s/ext/bin/%s' % (self.dynamo_home, host),
                                      '%s/bin' % go_root,
                                      '%s/platform-tools' % env['ANDROID_HOME'],
                                      '%s/ext/SDKs/%s/toolchains/llvm/prebuilt/%s-x86_64/bin' % (self.dynamo_home,PACKAGES_ANDROID_NDK,android_host)])

        env['PATH'] = paths + os.path.pathsep + env['PATH']

        # This trickery is needed for the bash to properly inherit the PATH that we've set here
        # See /etc/profile for further details
        is_mingw = env.get('MSYSTEM', '') in ('MINGW64',)
        if is_mingw:
            env['ORIGINAL_PATH'] = env['PATH']

        go_paths = os.path.pathsep.join(['%s/go' % self.dynamo_home,
                                         join(self.defold, 'go')])
        env['GOPATH'] = go_paths
        env['GOROOT'] = go_root

        env['MAVEN_OPTS'] = '-Xms256m -Xmx700m -XX:MaxPermSize=1024m'

        # Force 32-bit python 2.7 on darwin.
        env['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
        env['VERSIONER_PYTHON_VERSION'] = '2.7'

        if self.no_colors:
            env['NOCOLOR'] = '1'

        env['EMSCRIPTEN'] = self._form_ems_path()
        env['EM_CACHE'] = join(self.get_ems_dir(), 'emscripten_cache')
        env['EM_CONFIG'] = join(self.get_ems_dir(), '.emscripten')

        xhr2_path = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR, 'xhr2', 'package', 'lib')
        if 'NODE_PATH' in env:
            env['NODE_PATH'] = xhr2_path + os.path.pathsep + env['NODE_PATH']
        else:
            env['NODE_PATH'] = xhr2_path

        return env

if __name__ == '__main__':
    boto_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../packages/boto-2.28.0-py2.7.whl'))
    sys.path.insert(0, boto_path)
    usage = '''usage: %prog [options] command(s)

Commands:
distclean        - Removes the DYNAMO_HOME folder
install_ext      - Install external packages
install_ems      - Install emscripten sdk
install_sdk      - Install sdk
sync_archive     - Sync engine artifacts from S3
activate_ems     - Used when changing to a branch that uses a different version of emscripten SDK (resets ~/.emscripten)
build_engine     - Build engine
archive_engine   - Archive engine (including builtins) to path specified with --archive-path
build_editor2    - Build editor
sign_editor2     - Sign editor
bundle_editor2   - Bundle editor (zip)
archive_editor2  - Archive editor to path specified with --archive-path
download_editor2 - Download editor bundle (zip)
notarize_editor2 - Notarize the macOS version of the editor
build_bob        - Build bob with native libraries included for cross platform deployment
archive_bob      - Archive bob to path specified with --archive-path
build_docs       - Build documentation
build_builtins   - Build builtin content archive
bump             - Bump version number
release          - Release editor
shell            - Start development shell
smoke_test       - Test editor and engine in combination
local_smoke      - Test run smoke test using local dev environment

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

    parser.add_option('--skip-codesign', dest='skip_codesign',
                      action = 'store_true',
                      default = False,
                      help = 'skip code signing (engine and editor). Default is false')

    parser.add_option('--skip-docs', dest='skip_docs',
                      action = 'store_true',
                      default = False,
                      help = 'skip building docs when building the engine. Default is false')

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

    parser.add_option('--windows-cert', dest='windows_cert',
                      default = None,
                      help = 'Path to codesigning certificate for Windows version of the editor')

    parser.add_option('--windows-cert-pass', dest='windows_cert_pass',
                      default = None,
                      help = 'Path to file containing password to codesigning certificate for Windows version of the editor')

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
                      skip_codesign = options.skip_codesign,
                      skip_docs = options.skip_docs,
                      skip_builtins = options.skip_builtins,
                      skip_bob_light = options.skip_bob_light,
                      disable_ccache = options.disable_ccache,
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
                      windows_cert = options.windows_cert,
                      windows_cert_pass = options.windows_cert_pass,
                      verbose = options.verbose)

    for cmd in args:
        f = getattr(c, cmd, None)
        if not f:
            parser.error('Unknown command %s' % cmd)
        else:
            start = time.time()
            print("Running '%s'" % cmd)
            f()
            c.wait_uploads()
            duration = (time.time() - start)
            print("'%s' completed in %.2f s" % (cmd, duration))

    print('Done')
