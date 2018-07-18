#!/usr/bin/env python

import os, sys, shutil, zipfile, re, itertools, json, platform, math, mimetypes
import optparse, subprocess, urllib, urlparse, tempfile, time
import imp
from datetime import datetime
from tarfile import TarFile
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
from glob import glob
from threading import Thread, Event
from Queue import Queue
from ConfigParser import ConfigParser

"""
Build utility for installing external packages, building engine, editor and cr
Run build.py --help for help
"""

PACKAGES_ALL="protobuf-2.3.0 waf-1.5.9 gtest-1.8.0 vectormathlibrary-r1649 junit-4.6 protobuf-java-2.3.0 openal-1.1 maven-3.0.1 ant-1.9.3 vecmath vpx-v0.9.7-p1 facebook-4.4.0 facebook-gameroom-2017-08-14 luajit-2.0.5 tremolo-0.0.8 PVRTexLib-4.18.0 webp-0.5.0 defold-robot-0.1.0".split()
PACKAGES_HOST="protobuf-2.3.0 gtest-1.8.0 cg-3.1 vpx-v0.9.7-p1 PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 tremolo-0.0.8".split()
PACKAGES_EGGS="protobuf-2.3.0-py2.5.egg pyglet-1.1.3-py2.5.egg gdata-2.0.6-py2.6.egg Jinja2-2.6-py2.6.egg Markdown-2.6.7-py2.7.egg".split()
PACKAGES_IOS="protobuf-2.3.0 gtest-1.8.0 facebook-4.4.0 luajit-2.0.5 tremolo-0.0.8".split()
PACKAGES_IOS_64="protobuf-2.3.0 gtest-1.8.0 facebook-4.4.0 tremolo-0.0.8".split()
PACKAGES_DARWIN="protobuf-2.3.0 gtest-1.8.0 PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 vpx-v0.9.7-p1 tremolo-0.0.8".split()
PACKAGES_DARWIN_64="protobuf-2.3.0 gtest-1.8.0 PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 vpx-v0.9.7-p1 tremolo-0.0.8 sassc-5472db213ec223a67482df2226622be372921847 apkc-0.1.0".split()
PACKAGES_WIN32="facebook-gameroom-2017-08-14 PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 openal-1.1 glut-3.7.6 apkc-0.1.0".split()
PACKAGES_WIN32_64="facebook-gameroom-2017-08-14 PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 openal-1.1 glut-3.7.6 sassc-5472db213ec223a67482df2226622be372921847 apkc-0.1.0".split()
PACKAGES_LINUX="PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 openal-1.1 apkc-0.1.0".split()
PACKAGES_LINUX_64="PVRTexLib-4.18.0 webp-0.5.0 luajit-2.0.5 sassc-5472db213ec223a67482df2226622be372921847 apkc-0.1.0".split()
PACKAGES_ANDROID="protobuf-2.3.0 gtest-1.8.0 facebook-4.4.1 android-support-v4 android-support-multidex android-23 google-play-services-4.0.30 luajit-2.0.5 tremolo-0.0.8 amazon-iap-2.0.16".split()
PACKAGES_EMSCRIPTEN="gtest-1.8.0 protobuf-2.3.0".split()
PACKAGES_EMSCRIPTEN_SDK="emsdk-1.35.23"
PACKAGES_IOS_SDK="iPhoneOS11.2.sdk"
PACKAGES_MACOS_SDK="MacOSX10.13.sdk"
PACKAGES_XCODE_TOOLCHAIN="XcodeToolchain9.2"
PACKAGES_WIN32_TOOLCHAIN="Microsoft-Visual-Studio-14-0"
PACKAGES_WIN32_SDK_8="WindowsKits-8.1"
PACKAGES_WIN32_SDK_10="WindowsKits-10.0"
DEFOLD_PACKAGES_URL = "https://s3-eu-west-1.amazonaws.com/defold-packages"
NODE_MODULE_XHR2_URL = "%s/xhr2-0.1.0-common.tar.gz" % (DEFOLD_PACKAGES_URL)
NODE_MODULE_LIB_DIR = os.path.join("ext", "lib", "node_modules")
EMSCRIPTEN_VERSION_STR = "1.35.0"
# The linux tool does not yet support git tags, so we have to treat it as a special case for the moment.
EMSCRIPTEN_VERSION_STR_LINUX = "master"
EMSCRIPTEN_SDK_OSX = "sdk-{0}-64bit".format(EMSCRIPTEN_VERSION_STR)
EMSCRIPTEN_SDK_LINUX = "sdk-{0}-64bit".format(EMSCRIPTEN_VERSION_STR_LINUX)
EMSCRIPTEN_DIR = join('bin', 'emsdk_portable', 'emscripten', EMSCRIPTEN_VERSION_STR)
EMSCRIPTEN_DIR_LINUX = join('bin', 'emsdk_portable', 'emscripten', EMSCRIPTEN_VERSION_STR_LINUX)
PACKAGES_FLASH=[]
SHELL = os.environ.get('SHELL', 'bash')

ENGINE_LIBS = "ddf particle glfw graphics lua hid input physics resource extension script tracking render rig gameobject gui sound liveupdate gamesys tools record gameroom iap push iac adtruth webview profiler facebook crash engine sdk".split()

class ExecException(Exception):
    def __init__(self, retcode, output):
        self.retcode = retcode
        self.output = output


def is_64bit_machine():
    return platform.machine().endswith('64')

# Legacy format, should be removed eventually
# Returns: [linux|x86_64-linux|win32|x86_64-win32|darwin]
def get_host_platform():
    if sys.platform == 'linux2':
        arch = platform.architecture()[0]
        if arch == '64bit':
            return 'x86_64-linux'
        else:
            return 'linux'
    elif sys.platform == 'win32' and is_64bit_machine():
        return 'x86_64-win32'
    else:
        return sys.platform

# The difference from get_host_platform is that it returns the correct platform
# Returns: [x86|x86_64][win32|linux|darwin]
def get_host_platform2():
    if sys.platform == 'linux2':
        arch = platform.architecture()[0]
        if arch == '64bit':
            return 'x86_64-linux'
        else:
            return 'x86-linux'
    elif sys.platform == 'win32':
        if is_64bit_machine():
            return 'x86_64-win32'
        else:
            return 'x86-win32'
    elif sys.platform == 'darwin':
        if is_64bit_machine():
            return 'x86_64-darwin'
        else:
            return 'x86-darwin'
    else:
        raise Exception("Unknown host platform: %s" % sys.platform)

def format_exe(name, platform):
    prefix = ''
    suffix = ''
    if 'win32' in platform:
        suffix = '.exe'
    elif 'android' in platform:
        prefix = 'lib'
        suffix = '.so'
    elif 'js-web' in platform:
        prefix = ''
        suffix = '.js'
    else:
        suffix = ''
    return '%s%s%s' % (prefix, name, suffix)

def format_lib(name, platform):
    prefix = 'lib'
    suffix = ''
    if 'darwin' in platform:
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
            except Exception,e:
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
        except KeyboardInterrupt,e:
            sys.exit(0)

        if isinstance(self.result, Exception):
            raise self.result
        else:
            return self.result

class Configuration(object):
    def __init__(self, dynamo_home = None,
                 target_platform = None,
                 eclipse_home = None,
                 skip_tests = False,
                 skip_codesign = False,
                 skip_docs = False,
                 skip_builtins = False,
                 skip_bob_light = False,
                 disable_ccache = False,
                 no_colors = False,
                 archive_path = None,
                 set_version = None,
                 eclipse = False,
                 branch = None,
                 channel = None,
                 eclipse_version = None,
                 waf_options = []):

        if sys.platform == 'win32':
            home = os.environ['USERPROFILE']
        else:
            home = os.environ['HOME']

        self.dynamo_home = dynamo_home if dynamo_home else join(os.getcwd(), 'tmp', 'dynamo_home')
        self.ext = join(self.dynamo_home, 'ext')
        self.defold = normpath(join(dirname(abspath(__file__)), '..'))
        self.eclipse_home = eclipse_home if eclipse_home else join(home, 'eclipse')
        self.defold_root = os.getcwd()
        self.host = get_host_platform()
        self.host2 = get_host_platform2()
        self.target_platform = target_platform

        # Like this, since we cannot guarantee that PYTHONPATH has been set up to include BuildUtility yet.
        # N.B. If we upgrade to more recent versions of python, then the method of module loading should also change.
        build_utility_module = imp.load_source('BuildUtility', os.path.join(self.defold, 'build_tools', 'BuildUtility.py'))
        self.build_utility = build_utility_module.BuildUtility(self.target_platform, self.host, self.dynamo_home)
        self._http_cache_module = imp.load_source('http_cache', os.path.join(self.defold, 'build_tools', 'http_cache.py'))

        self.skip_tests = skip_tests
        self.skip_codesign = skip_codesign
        self.skip_docs = skip_docs
        self.skip_builtins = skip_builtins
        self.skip_bob_light = skip_bob_light
        self.disable_ccache = disable_ccache
        self.no_colors = no_colors
        self.archive_path = archive_path
        self.set_version = set_version
        self.eclipse = eclipse
        self.branch = branch
        self.channel = channel
        self.eclipse_version = eclipse_version
        self.waf_options = waf_options

        self.thread_pool = None
        self.s3buckets = {}
        self.futures = []

        with open('VERSION', 'r') as f:
            self.version = f.readlines()[0].strip()

        self._create_common_dirs()

    def __del__(self):
        if len(self.futures) > 0:
            print('ERROR: Pending futures (%d)' % len(self.futures))
            os._exit(5)

    def _create_common_dirs(self):
        for p in ['ext/lib/python', 'share', 'lib/js-web/js']:
            self._mkdirs(join(self.dynamo_home, p))

    def _mkdirs(self, path):
        if not os.path.exists(path):
            os.makedirs(path)

    def _log(self, msg):
        print msg
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
        # Avoid a bug in python 2.7 (fixed in 2.7.2) related to not being able to remove symlinks: http://bugs.python.org/issue10761
        if self.host == 'linux' and version[0] == 2 and version[1] == 7 and version[2] < 2:
            self.exec_env_command(['tar', 'xfz', file], cwd = path)
        else:
            tf = TarFile.open(file, 'r:gz')
            tf.extractall(path)
            tf.close()

    def _extract_zip(self, file, path):
        self._log('Extracting %s to %s' % (file, path))
        zf = zipfile.ZipFile(file, 'r')
        zf.extractall(path)
        zf.close()

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
        path = self._http_cache_module.download(url, lambda count, total: self._log('Downloading %s %.2f%%' % (url, 100 * count / float(total))))
        if not path:
            self._log('Downloading %s failed' % (url))
        return path

    def install_go(self):
        urls = {
            'x86_64-darwin': 'https://storage.googleapis.com/golang/go1.7.1.darwin-amd64.tar.gz',
            'linux'        : 'https://storage.googleapis.com/golang/go1.7.1.linux-386.tar.gz',
            'x86_64-linux' : 'https://storage.googleapis.com/golang/go1.7.1.linux-amd64.tar.gz',
            'win32'        : 'https://storage.googleapis.com/golang/go1.7.1.windows-386.zip',
            'x86_64-win32' : 'https://storage.googleapis.com/golang/go1.7.1.windows-amd64.zip'
        }

        url = urls.get(self.target_platform)

        if url:
            path = self._download(url)
            target_path = join(self.ext, 'go', self.target_platform)
            self._extract(path, target_path)
        else:
            print("No go found for %s" % self.target_platform)

    def _make_package_path(self, platform, package):
        return join(self.defold_root, 'packages', package) + '-%s.tar.gz' % platform

    def _make_package_paths(self, platform, packages):
        return [self._make_package_path(platform, package) for package in packages]

    def _extract_packages(self, platform, packages):
        for package in packages:
            self._extract_tgz(self._make_package_path(platform, package), self.ext)

    def install_ext(self):
        print("Installing common packages")
        for p in PACKAGES_ALL:
            self._extract_tgz(self._make_package_path('common', p), self.ext)

        # TODO: Make sure the order of install does not affect the outcome!

        platform_packages = {
            'win32':          PACKAGES_WIN32,
            'x86_64-win32':   PACKAGES_WIN32_64,
            'linux':          PACKAGES_LINUX,
            'x86_64-linux':   PACKAGES_LINUX_64,
            'darwin':         PACKAGES_DARWIN,
            'x86_64-darwin':  PACKAGES_DARWIN_64,
            'armv7-darwin':   PACKAGES_IOS,
            'arm64-darwin':   PACKAGES_IOS_64,
            'armv7-android':  PACKAGES_ANDROID,
            'js-web':         PACKAGES_EMSCRIPTEN
        }

        base_platforms = self.get_base_platforms()
        target_platform = self.target_platform
        other_platforms = set(platform_packages.keys()).difference(set(base_platforms), set([target_platform, self.host]))

        installed_packages = set()

        for platform in other_platforms:
            packages = platform_packages.get(platform, [])
            package_paths = self._make_package_paths(platform, packages)
            print("Installing %s packages" % platform)
            for path in package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(package_paths)

        for base_platform in self.get_base_platforms():
            packages = list(PACKAGES_HOST)
            packages.extend(platform_packages.get(base_platform, []))
            package_paths = self._make_package_paths(base_platform, packages)
            package_paths = [path for path in package_paths if path not in installed_packages]
            if len(package_paths) != 0:
                print("Installing %s packages" % base_platform)
                for path in package_paths:
                    self._extract_tgz(path, self.ext)
                installed_packages.update(package_paths)

        target_packages = platform_packages.get(self.target_platform, [])
        target_package_paths = self._make_package_paths(self.target_platform, target_packages)
        target_package_paths = [path for path in target_package_paths if path not in installed_packages]

        if len(target_package_paths) != 0:
            print("Installing %s packages" % self.target_platform)
            for path in target_package_paths:
                self._extract_tgz(path, self.ext)
            installed_packages.update(target_package_paths)

        # Is as3-web a supported platform? doesn't say so in --platform?
        print("Installing flash packages")
        self._extract_packages('as3-web', PACKAGES_FLASH)

        print("Installing python eggs")
        for egg in glob(join(self.defold_root, 'packages', '*.egg')):
            self._log('Installing %s' % basename(egg))
            self.exec_env_command(['easy_install', '-q', '-d', join(self.ext, 'lib', 'python'), '-N', egg])

        print("Installing javascripts")
        for n in 'js-web-pre.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        for n in 'js-web-pre-engine.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        print("Installing profiles etc")
        for n in itertools.chain(*[ glob('share/*%s' % ext) for ext in ['.mobileprovision', '.xcent', '.supp']]):
            self._copy(join(self.defold_root, n), join(self.dynamo_home, 'share'))

        node_modules_dir = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR)
        self._mkdirs(node_modules_dir)
        xhr2_tarball = self._download(NODE_MODULE_XHR2_URL)
        self._extract_tgz(xhr2_tarball, node_modules_dir)

        def download_sdk(url, targetfolder, tmpname=None): # tmpname is the top folder name inside the archive, which you wish to rename
            if not os.path.exists(targetfolder):
                dlpath = self._download(url)
                parent_folder = os.path.split(targetfolder)[0]
                if tmpname is None:
                    self._extract_tgz(dlpath, parent_folder)
                else:
                    tmpfolder = os.path.join(parent_folder, tmpname)
                    os.rename(tmpfolder, targetfolder)

        if target_platform in ('darwin', 'x86_64-darwin', 'armv7-darwin', 'arm64-darwin'):
            # macOS SDK
            tgtfolder = join(self.ext, 'SDKs', PACKAGES_MACOS_SDK)
            if not os.path.exists(tgtfolder):
                url = '%s/%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_MACOS_SDK)
                dlpath = self._download(url)
                tmpfolder = join(self.ext, 'SDKs')
                self._extract_tgz(dlpath, tmpfolder)
                os.rename(join(tmpfolder, 'MacOSX.sdk'), tgtfolder)

            # Xcode toolchain
            tgtfolder = join(self.ext, 'SDKs', PACKAGES_XCODE_TOOLCHAIN)
            if not os.path.exists(tgtfolder):
                url = '%s/%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_XCODE_TOOLCHAIN)
                dlpath = self._download(url)
                self._extract_tgz(dlpath, join(self.ext, 'SDKs'))

        if target_platform in ('armv7-darwin', 'arm64-darwin'):
            # iOS SDK
            tgtfolder = join(self.ext, 'SDKs', PACKAGES_IOS_SDK)
            if not os.path.exists(tgtfolder):
                url = '%s/%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_IOS_SDK)
                dlpath = self._download(url)
                tmpfolder = join(self.ext, 'SDKs')
                self._extract_tgz(dlpath, tmpfolder)
                os.rename(join(tmpfolder, 'iPhoneOS.sdk'), tgtfolder)

        if 'win32' in target_platform and not ('win32' in self.host):
            win32_sdk_folder = join(self.ext, 'SDKs', 'Win32')
            download_sdk( '%s/%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_WIN32_SDK_8), join(win32_sdk_folder, 'WindowsKits', '8.1') )
            download_sdk( '%s/%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_WIN32_SDK_10), join(win32_sdk_folder, 'WindowsKits', '10') )
            targetfolder = join(win32_sdk_folder, 'MicrosoftVisualStudio14.0') # let's avoid spaces in the paths
            download_sdk( '%s/%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_WIN32_TOOLCHAIN), targetfolder, 'Microsoft Visual Studio 14.0' )

            # On OSX, the file system is already case insensitive, so no need to duplicate the files as we do on the extender server

    def _form_ems_path(self):
        path = ''
        if 'linux' in self.host:
            path = join(self.ext, EMSCRIPTEN_DIR_LINUX)
        else:
            path = join(self.ext, EMSCRIPTEN_DIR)
        return path

    def install_ems(self):
        url = '%s/%s-%s.tar.gz' % (DEFOLD_PACKAGES_URL, PACKAGES_EMSCRIPTEN_SDK, self.host)
        dlpath = self._download(url)
        if dlpath is None:
            print("Error. Could not download %s" % url)
            sys.exit(1)
        self._extract(dlpath, self.ext)
        self.activate_ems()
        os.environ['EMSCRIPTEN'] = self._form_ems_path()

    def get_ems_sdk_name(self):
        sdk = EMSCRIPTEN_SDK_OSX
        if 'linux' in self.host:
            sdk = EMSCRIPTEN_SDK_LINUX
        return sdk;

    def get_ems_exe_path(self):
        return join(self.ext, 'bin', 'emsdk_portable', 'emsdk')

    def activate_ems(self):
        # Compile a file warm up the emscripten caches (libc etc)
        c_file = tempfile.mktemp(suffix='.c')
        exe_file = tempfile.mktemp(suffix='.js')
        with open(c_file, 'w') as f:
            f.write('int main() { return 0; }')

        self.exec_env_command([self.get_ems_exe_path(), 'activate', self.get_ems_sdk_name()])
        # This sporadically fails on OS X by inability to create the ~/.emscripten_cache dir.
        # Does not seem to help to pre-create it or explicitly setting the --cache flag
        self.exec_env_command(['%s/emcc' % self._form_ems_path(), c_file, '-o', '%s' % exe_file])

    def check_ems(self):
        home = os.path.expanduser('~')
        config = join(home, '.emscripten')
        err = False
        if not os.path.isfile(config):
            print 'No .emscripten file.'
            err = True
        emsDir = join(self.ext, EMSCRIPTEN_DIR)
        if self.host == 'linux':
            emsDir = join(self.ext, EMSCRIPTEN_DIR_LINUX)
        if not os.path.isdir(emsDir):
            print 'Emscripten tools not installed.'
            err = True
        if err:
            print 'Consider running install_ems'

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

    def _add_files_to_zip(self, zip, paths, directory=None, topfolder=None):
        for p in paths:
            if not os.path.isfile(p):
                continue
            an = p
            if directory:
                an = os.path.relpath(p, directory)
            if topfolder:
                an = os.path.join(topfolder, an)
            zip.write(p, an)

    def is_cross_platform(self):
        return self.host != self.target_platform

    def is_desktop_target(self):
        return self.target_platform in ['linux', 'x86_64-linux', 'darwin', 'x86_64-darwin', 'win32', 'x86_64-win32']

    # package the native SDK, return the path to the zip file
    def _package_platform_sdk(self, platform):
        with open(join(self.dynamo_home, 'defoldsdk.zip'), 'wb') as outfile:
            zip = zipfile.ZipFile(outfile, 'w', zipfile.ZIP_DEFLATED)

            topfolder = 'defoldsdk'
            defold_home = os.path.normpath(os.path.join(self.dynamo_home, '..', '..'))

            # Includes
            includes = []
            cwd = os.getcwd()
            os.chdir(self.dynamo_home)
            for root, dirs, files in os.walk("sdk/include"):
                for file in files:
                    if file.endswith('.h'):
                        includes.append(os.path.join(root, file))

            os.chdir(cwd)
            includes = [os.path.join(self.dynamo_home, x) for x in includes]
            self._add_files_to_zip(zip, includes, os.path.join(self.dynamo_home, 'sdk'), topfolder)

            # Configs
            configs = ['extender/build.yml']
            configs = [os.path.join(self.dynamo_home, x) for x in configs]
            self._add_files_to_zip(zip, configs, self.dynamo_home, topfolder)

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
            external_jars = ("facebooksdk.jar",
                             "bolts-android-1.2.0.jar",
                             "google-play-services.jar",
                             "android-support-v4.jar",
                             "android-support-multidex.jar",
                             "android.jar",
                             "in-app-purchasing-2.0.61.jar")
            jardir = os.path.join(self.dynamo_home, 'ext/share/java')
            paths = _findjars(jardir, external_jars)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)

            # JavaScript files
            # js-web-pre-x files
            jsdir = os.path.join(self.dynamo_home, 'share')
            paths = _findjslibs(jsdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)
            # libraries
            jsdir = os.path.join(self.dynamo_home, 'lib/js-web/js/')
            paths = _findjslibs(jsdir)
            self._add_files_to_zip(zip, paths, self.dynamo_home, topfolder)


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
        except Exception, e:
            print "Failed to package sdk for platform %s: %s" % (self.target_platform, e)
        else:
            print "Wrote %s" % path

    def build_builtins(self):
        with open(join(self.dynamo_home, 'share', 'builtins.zip'), 'wb') as f:
            self._ziptree(join(self.dynamo_home, 'content', 'builtins'), outfile = f, directory = join(self.dynamo_home, 'content'))

    def archive_engine(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'engine', self.target_platform).replace('\\', '/')
        share_archive_path = join(self.archive_path, sha1, 'engine', 'share').replace('\\', '/')
        java_archive_path = join(self.archive_path, sha1, 'engine', 'share', 'java').replace('\\', '/')
        dynamo_home = self.dynamo_home

        bin_dir = self.build_utility.get_binary_path()
        lib_dir = self.target_platform

        # upload editor 2.0 launcher
        if self.target_platform in ['linux', 'x86_64-linux', 'darwin', 'x86_64-darwin', 'win32', 'x86_64-win32']:
            launcher_name = format_exe("launcher", self.target_platform)
            launcherbin = join(bin_dir, launcher_name)
            self.upload_file(launcherbin, '%s/%s' % (full_archive_path, launcher_name))

        for n in ['dmengine', 'dmengine_release', 'dmengine_headless']:
            engine_name = format_exe(n, self.target_platform)
            engine = join(bin_dir, engine_name)
            self.upload_file(engine, '%s/%s' % (full_archive_path, engine_name))
            if self.target_platform == 'js-web':
                self.upload_file(join(bin_dir, 'defold_sound.swf'), join(full_archive_path, 'defold_sound.swf'))
                engine_mem = join(bin_dir, engine_name + '.mem')
                if os.path.exists(engine_mem):
                    self.upload_file(engine_mem, '%s/%s.mem' % (full_archive_path, engine_name))
                engine_symbols = join(bin_dir, engine_name + '.symbols')
                if os.path.exists(engine_symbols):
                    self.upload_file(engine_symbols, '%s/%s.symbols' % (full_archive_path, engine_name))

        zip_archs = []
        if not self.skip_docs:
            zip_archs.append('ref-doc.zip')
        if not self.skip_builtins:
            zip_archs.append('builtins.zip')
        for zip_arch in zip_archs:
            self.upload_file(join(dynamo_home, 'share', zip_arch), '%s/%s' % (share_archive_path, zip_arch))

        if self.target_platform == 'x86_64-linux':
            # NOTE: It's arbitrary for which platform we archive dlib.jar. Currently set to linux 64-bit
            self.upload_file(join(dynamo_home, 'share', 'java', 'dlib.jar'), '%s/dlib.jar' % (java_archive_path))

        if 'android' in self.target_platform:
            files = [
                ('share/java', 'classes.dex'),
                ('bin/%s' % (self.target_platform), 'dmengine.apk'),
                ('bin/%s' % (self.target_platform), 'dmengine_release.apk'),
                ('ext/share/java', 'android.jar'),
            ]
            for f in files:
                src = join(dynamo_home, f[0], f[1])
                self.upload_file(src, '%s/%s' % (full_archive_path, f[1]))

            resources = self._ziptree(join(dynamo_home, 'ext', 'share', 'java', 'res'), directory = join(dynamo_home, 'ext', 'share', 'java'))
            self.upload_file(resources, '%s/android-resources.zip' % (full_archive_path))

        libs = ['particle']
        if self.is_desktop_target():
            libs.append('texc')
        for lib in libs:
            lib_name = format_lib('%s_shared' % (lib), self.target_platform)
            lib_path = join(dynamo_home, 'lib', lib_dir, lib_name)
            self.upload_file(lib_path, '%s/%s' % (full_archive_path, lib_name))

        sdkpath = self._package_platform_sdk(self.target_platform)
        self.upload_file(sdkpath, '%s/defoldsdk.zip' % full_archive_path)

    def _get_build_flags(self):
        supported_tests = {}
        supported_tests['darwin'] = ['darwin', 'x86_64-darwin']
        supported_tests['x86_64-win32'] = ['win32', 'x86_64-win32']

        supports_tests = self.target_platform in supported_tests.get(self.host, []) or self.host == self.target_platform
        skip_tests = '--skip-tests' if self.skip_tests or not supports_tests else ''
        skip_codesign = '--skip-codesign' if self.skip_codesign else ''
        disable_ccache = '--disable-ccache' if self.disable_ccache else ''
        eclipse = '--eclipse' if self.eclipse else ''
        return {'skip_tests':skip_tests, 'skip_codesign':skip_codesign, 'disable_ccache':disable_ccache, 'eclipse':eclipse}

    def get_base_platforms(self):
        # Base platforms is the platforms to build the base libs for.
        # The base libs are the libs needed to build bob, i.e. contains compiler code.

        platform_dependencies = {'darwin': ['darwin', 'x86_64-darwin'], # x86_64-darwin from IOS fix 3dea8222
                                 'x86_64-linux': [],
                                 'x86_64-win32': ['win32']}

        platforms = list(platform_dependencies.get(self.host, [self.host]))

        if not self.host in platforms:
            platforms.append(self.host)

        return platforms

    def _build_engine_cmd(self, skip_tests, skip_codesign, disable_ccache, eclipse):
        return 'python %s/ext/bin/waf --prefix=%s %s %s %s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, skip_tests, skip_codesign, disable_ccache, eclipse)

    def _build_engine_lib(self, args, lib, platform, skip_tests = False, dir = 'engine'):
        self._log('Building %s for %s' % (lib, platform))
        skip_build_tests = []
        if skip_tests and '--skip-build-tests' not in self.waf_options:
            skip_build_tests.append('--skip-tests')
            skip_build_tests.append('--skip-build-tests')
        cwd = join(self.defold_root, '%s/%s' % (dir, lib))
        plf_args = ['--platform=%s' % platform]
        self.exec_env_command(args + plf_args + self.waf_options + skip_build_tests, cwd = cwd)

    def build_bob_light(self):
        self._log('Building bob')
        self.exec_env_shell_command(" ".join([join(self.dynamo_home, 'ext/share/ant/bin/ant'), 'clean', 'install']),
                                    cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob'))

    def build_engine(self):
        cmd = self._build_engine_cmd(**self._get_build_flags())
        args = cmd.split()
        host = self.host
        if host == 'darwin':
            host = 'x86_64-darwin'
        # There is a dependency between 32-bit python and the ctypes lib produced in dlib
        if host == 'x86_64-darwin' and self.target_platform != 'darwin':
            self._build_engine_lib(args, 'dlib', 'darwin', skip_tests = True)
        if host == 'x86_64-win32' and self.target_platform != 'win32':
            self._build_engine_lib(args, 'dlib', 'win32', skip_tests = True)
        if not self.skip_bob_light:
            # We must build bob-light, which builds content during the engine build
            # There also seems to be a strange dep between having it built and building dlib for the target, even when target == host
            for lib in ['dlib', 'texc']:
                skip_tests = host != self.target_platform
                self._build_engine_lib(args, lib, host, skip_tests = skip_tests)
            self.build_bob_light()
        # Target libs to build
        engine_libs = list(ENGINE_LIBS)
        if host != self.target_platform:
            engine_libs.insert(0, 'dlib')
            if self.is_desktop_target():
                engine_libs.insert(1, 'texc')
        for lib in engine_libs:
            self._build_engine_lib(args, lib, target_platform)
        self._build_engine_lib(args, 'extender', target_platform, dir = 'share')
        if not self.skip_docs:
            self.build_docs()
        if not self.skip_builtins:
            self.build_builtins()

    def build_go(self):
        exe_ext = '.exe' if 'win32' in self.target_platform else ''
        go = '%s/ext/go/%s/go/bin/go%s' % (self.dynamo_home, self.target_platform, exe_ext)

        if not os.path.exists(go):
            self._log("Missing go for target platform, run install_ext with --platform set.")
            exit(5)

        self.exec_env_command([go, 'clean', '-i', 'github.com/...'])
        self.exec_env_command([go, 'install', 'github.com/...'])
        self.exec_env_command([go, 'clean', '-i', 'defold/...'])
        if not self.skip_tests:
            self.exec_env_command([go, 'test', 'defold/...'])
        self.exec_env_command([go, 'install', 'defold/...'])

        for f in glob(join(self.defold, 'go', 'bin', '*')):
            shutil.copy(f, join(self.dynamo_home, 'bin'))

    def archive_go(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'go', self.target_platform)
        for p in glob(join(self.defold, 'go', 'bin', '*')):
            self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))

    def archive_bob(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'bob').replace('\\', '/')
        for p in glob(join(self.dynamo_home, 'share', 'java', 'bob.jar')):
            self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))

    def copy_local_bob_artefacts(self):
        apkc_name = format_exe('apkc', self.host2)
        texc_name = format_lib('texc_shared', self.host2)
        luajit_dir = tempfile.mkdtemp()
        cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        missing = {}
        def add_missing(plf, txt):
            txts = []
            txts = missing.setdefault(plf, txts)
            txts = txts.append(txt)
        # TODO Haxx to deal with the incorrect usage of x86-darwin in editor1
        for plf in [['win32', 'x86-win32'], ['x86_64-win32', 'x86_64-win32'], ['x86_64-linux', 'x86_64-linux'], ['darwin', 'x86-darwin'], ['x86_64-darwin', 'x86-darwin']]:
            luajit_path = join(cwd, '../../packages/luajit-2.0.5-%s.tar.gz' % (plf[0]))
            if not os.path.exists(luajit_path):
                add_missing(plf[1], "package '%s' could not be found" % (luajit_path))
            else:
                self._extract(luajit_path, luajit_dir)
                luajit_exe = format_exe('luajit', plf[1])
                self._copy(join(luajit_dir, 'bin/%s/%s' % (plf[0], luajit_exe)), join(cwd, 'libexec/%s/%s' % (plf[1], luajit_exe)))
        win32_files = dict([['ext/lib/%s/%s.dll' % (plf[0], lib), 'lib/%s/%s.dll' % (plf[1], lib)] for lib in ['OpenAL32', 'wrap_oal', 'PVRTexLib', 'msvcr120'] for plf in [['win32', 'x86-win32'], ['x86_64-win32', 'x86_64-win32']]])
        osx_files = dict([['ext/lib/%s/lib%s.dylib' % (plf[0], lib), 'lib/%s/lib%s.dylib' % (plf[1], lib)] for lib in ['PVRTexLib'] for plf in [['darwin', 'darwin'], ['x86_64-darwin', 'x86_64-darwin']]])
        linux_files = dict([['ext/lib/%s/lib%s.so' % (plf[0], lib), 'lib/%s/lib%s.so' % (plf[1], lib)] for lib in ['PVRTexLib'] for plf in [['linux', 'x86-linux'], ['x86_64-linux', 'x86_64-linux']]])
        js_files = {'bin/js-web/defold_sound.swf': 'libexec/js-web/defold_sound.swf'}
        # TODO Haxx to deal with the incorrect usage of x86-darwin in editor1
        haxx_host = self.host2 if self.host2 != 'x86_64-darwin' else 'x86-darwin'
        android_files = {'ext/bin/%s/%s' % (self.host2, apkc_name): 'libexec/%s/%s' % (haxx_host, apkc_name),
                         'share/java/classes.dex': 'lib/classes.dex',
                         'ext/share/java/android.jar': 'lib/android.jar'}
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
                     'osx-bundling': osx_files,
                     'linux-bundling': linux_files}
        # Add dmengine to 'artefacts' procedurally
        for type, plfs in {'android-bundling': [['armv7-android', 'armv7-android']],
                           'win32-bundling': [['win32', 'x86-win32'], ['x86_64-win32', 'x86_64-win32']],
                           'js-bundling': [['js-web', 'js-web']],
                           'ios-bundling': [['armv7-darwin', 'armv7-darwin'], ['arm64-darwin', 'arm64-darwin']],
                           # Haxx to deal with the incorrect usage of x86-darwin in editor1
                           'osx-bundling': [['darwin', 'x86-darwin'], ['x86_64-darwin', 'x86-darwin']],
                           'linux-bundling': [['x86_64-linux', 'x86_64-linux']]}.iteritems():
            # plfs is pairs of src-platform -> dst-platform
            # The haxx above makes sure that x86_64-darwin is noted as x86-darwin for dst-platform (because of Ed1 reasons)
            for plf in plfs:
                artefacts[type].update(dict([['bin/%s/%s' % (plf[0], format_exe(name, plf[1])), 'libexec/%s/%s' % (plf[1], format_exe(name, plf[1]))] for name in ['dmengine', 'dmengine_release']]))
        # Perform the actual copy, or list which files are missing
        for type, files in artefacts.iteritems():
            m = []
            for src, dst in files.iteritems():
                src_path = join(self.dynamo_home, src)
                if not os.path.exists(src_path):
                    m.append(src_path)
                else:
                    dst_path = join(cwd, dst)
                    self._mkdirs(os.path.dirname(dst_path))
                    self._copy(src_path, dst_path)
            if m:
                add_missing(type, m)
        self.exec_env_command(['jar', 'cfM', 'lib/android-res.zip', '-C', '%s/ext/share/java/' % (self.dynamo_home), 'res'], cwd = cwd)
        if missing:
            print('*** NOTE! There are missing artefacts.')
            print(json.dumps(missing, indent=2))

    def build_bob(self):
        cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        sha1 = self._git_sha1()
        if os.path.exists(os.path.join(self.dynamo_home, 'archive', sha1)):
            self.exec_env_shell_command("./scripts/copy.sh", cwd = cwd)
        else:
            self.copy_local_bob_artefacts()

        self.exec_env_shell_command(" ".join([join(self.dynamo_home, 'ext/share/ant/bin/ant'), 'clean', 'install-full']),
                              cwd = cwd)

    def build_sdk(self):
        tempdir = tempfile.mkdtemp() # where the sdk ends up

        sha1 = self._git_sha1()
        u = urlparse.urlparse(self.archive_path)
        bucket = self._get_s3_bucket(u.hostname)

        root = urlparse.urlparse(self.archive_path).path[1:]
        base_prefix = os.path.join(root, sha1)

        platforms = ['linux', 'x86_64-linux', 'darwin', 'x86_64-darwin', 'win32', 'x86_64-win32', 'armv7-darwin', 'arm64-darwin', 'armv7-android', 'js-web']
        for platform in platforms:
            platform_sdk_url = join(self.archive_path, sha1, 'engine', platform).replace('\\', '/')

            prefix = os.path.join(base_prefix, 'engine', platform, 'defoldsdk.zip')
            entry = bucket.get_key(prefix)

            if entry is None:
                raise Exception("Could not find sdk: %s" % prefix)

            platform_sdk_zip = tempfile.NamedTemporaryFile(delete = False)
            print "Downloading", entry.key
            entry.get_contents_to_filename(platform_sdk_zip.name)
            print "Downloaded", entry.key, "to", platform_sdk_zip.name

            self._extract_zip(platform_sdk_zip.name, tempdir)
            print "Extracted", platform_sdk_zip.name, "to", tempdir

            os.unlink(platform_sdk_zip.name)
            print ""

        treepath = os.path.join(tempdir, 'defoldsdk')
        sdkpath = self._ziptree(treepath, directory=tempdir)
        print "Packaged defold sdk"

        sdkurl = join(self.archive_path, sha1, 'engine').replace('\\', '/')
        self.upload_file(sdkpath, '%s/defoldsdk.zip' % sdkurl)

    def build_docs(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        self._log('Building API docs')
        cwd = join(self.defold_root, 'engine/docs')
        cmd = 'python %s/ext/bin/waf configure --prefix=%s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, skip_tests)
        self.exec_env_command(cmd.split() + self.waf_options, cwd = cwd)
        with open(join(self.dynamo_home, 'share', 'ref-doc.zip'), 'wb') as f:
            self._ziptree(join(self.dynamo_home, 'share', 'doc'), outfile = f, directory = join(self.dynamo_home, 'share'))

    def test_cr(self):
        cwd = join(self.defold_root, 'com.dynamo.cr', 'com.dynamo.cr.parent')
        self.exec_env_command([join(self.dynamo_home, 'ext/share/maven/bin/mvn'), 'clean', 'verify', '-Declipse-version=%s' % self.eclipse_version],
                              cwd = cwd)

    def _get_cr_builddir(self, product):
        return join(os.getcwd(), 'com.dynamo.cr/com.dynamo.cr.%s-product' % product)

    def build_server(self):
        cwd = join(self.defold_root, 'server')
        self.exec_env_command(['./gradlew', 'clean', 'test', 'distZip'], cwd = cwd)

    def build_editor(self):
        import xml.etree.ElementTree as ET

        self.build_bob_light()

        sha1 = self._git_sha1()

        if self.channel != 'stable':
            qualified_version = self.version + ".qualifier"
        else:
            qualified_version = self.version

        icon_path = '/icons/%s' % self.channel

        tree = ET.parse('com.dynamo.cr/com.dynamo.cr.editor-product/template/cr.product')
        root = tree.getroot()

        root.attrib['version'] = qualified_version
        for n in root.find('launcher'):
            if n.tag == 'win':
                icon = n.find('ico')
                name = os.path.basename(icon.attrib['path'])
                icon.attrib['path'] = 'icons/%s/%s' % (self.channel, name)
            elif 'icon' in n.attrib:
                name = os.path.basename(n.attrib['icon'])
                n.attrib['icon'] = 'icons/%s/%s' % (self.channel, name)

        for n in root.find('configurations').findall('property'):
            if n.tag == 'property':
                name = n.attrib['name']
                if name == 'defold.version':
                    n.attrib['value'] = self.version
                elif name == 'defold.sha1':
                    n.attrib['value'] = sha1
                elif name == 'defold.channel':
                    n.attrib['value'] = options.channel

        with open('com.dynamo.cr/com.dynamo.cr.editor-product/cr-generated.product', 'wb') as f:
            f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
            f.write('<?pde version="3.5"?>\n')
            f.write('\n')
            tree.write(f, encoding='utf-8')

        p2 = """
instructions.configure=\
  addRepository(type:0,location:http${#58}//d.defold.com/%(channel)s/update/);\
  addRepository(type:1,location:http${#58}//d.defold.com/%(channel)s/update/);
"""

        with open('com.dynamo.cr/com.dynamo.cr.editor-product/cr-generated.p2.inf', 'wb') as f:
            f.write(p2 % { 'channel': self.channel })

        self._build_cr('editor')

    def _archive_cr(self, product, build_dir):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, self.channel, product).replace('\\', '/') + '/'
        host, path = full_archive_path.split(':', 1)
        for p in glob(join(build_dir, 'target/products/*.zip')):
            self.upload_file(p, full_archive_path)

        repo_dir = join(build_dir, 'target/repository')
        for root,dirs,files in os.walk(repo_dir):
            for f in files:
                p = join(root, f)
                u = join(full_archive_path, "repository", os.path.relpath(p, repo_dir))
                self.upload_file(p, u)

    def archive_editor(self):
        build_dir = self._get_cr_builddir('editor')
        self._archive_cr('editor', build_dir)

    def archive_server(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'server').replace('\\', '/') + '/'
        host, path = full_archive_path.split(':', 1)
        build_dir = join(self.defold_root, 'server')
        for p in glob(join(build_dir, 'build/distributions/*.zip')):
            self.upload_file(p, full_archive_path)

    def _build_cr(self, product):
        cwd = join(self.defold_root, 'com.dynamo.cr', 'com.dynamo.cr.parent')
        self.exec_env_command([join(self.dynamo_home, 'ext/share/maven/bin/mvn'), 'clean', 'verify', '-P', product, '-Declipse-version=%s' % self.eclipse_version], cwd = cwd)

    def build_editor2(self):
        cmd = ['./scripts/bundle.py',
               '--platform=x86_64-darwin',
               '--platform=x86_64-linux',
               '--platform=x86-win32',
               '--platform=x86_64-win32',
               '--version=%s' % self.version,
               '--channel=%s' % self.channel,
               '--engine-artifacts=auto']

        cwd = join(self.defold_root, 'editor')

        self.exec_env_command(cmd, cwd = cwd)

    def archive_editor2(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'editor2')

        for ext in ['zip', 'dmg']:
            for p in glob(join(self.defold_root, 'editor', 'target', 'editor', 'Defold*.%s' % ext)):
                self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))

        for p in glob(join(self.defold_root, 'editor', 'target', 'editor', 'launcher*')):
            self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))

        for p in glob(join(self.defold_root, 'editor', 'target', 'editor', 'update', '*')):
            self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))
        self.wait_uploads()

    def release_editor2(self):
        if not self.channel:
            self._log("Tried to release with no channel specified, aborting")
            return

        sha1 = self._git_sha1()

        self._log("Releasing editor2 '%s' for channel '%s'" % (sha1, self.channel))

        # Rather than accessing S3 from its web end-point, we always go through the CDN
        update_url = 'https://d.defold.com/editor2/%(sha1)s/editor2' % {'sha1': sha1}
        update_data = {
            'url': update_url,
        }

        archive_url = urlparse.urlparse(self.archive_path)
        bucket = self._get_s3_bucket(archive_url.hostname)
        key = bucket.new_key('editor2/channels/%(channel)s/update.json' % {'channel': self.channel})
        key.content_type = 'application/json'

        self._log("Updating channel '%s': %s" % (self.channel, key))
        key.set_contents_from_string(json.dumps(update_data))

    def bump(self):
        sha1 = self._git_sha1()

        with open('VERSION', 'r') as f:
            current = f.readlines()[0].strip()

        if self.set_version:
            new_version = self.set_version
        else:
            lst = map(int, current.split('.'))
            lst[-1] += 1
            new_version = '.'.join(map(str, lst))

        with open('VERSION', 'w') as f:
            f.write(new_version)

        print 'Bumping engine version from %s to %s' % (current, new_version)
        print 'Review changes and commit'

    def shell(self):
        print 'Setting up shell with DYNAMO_HOME, PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH (where applicable) set'
        if "win32" in self.host:
            preexec_fn = None
        else:
            preexec_fn = self.check_ems

        process = subprocess.Popen([SHELL, '-l'], env = self._form_env(), preexec_fn=preexec_fn)
        output = process.communicate()[0]

        if process.returncode != 0:
            self._log(output)
            sys.exit(process.returncode)

    def _get_tagged_releases(self):
        u = urlparse.urlparse(self.archive_path)
        bucket = self._get_s3_bucket(u.hostname)

        def get_files(sha1):
            root = urlparse.urlparse(self.archive_path).path[1:]
            base_prefix = os.path.join(root, sha1)
            files = []
            prefix = os.path.join(base_prefix, 'engine')
            for x in bucket.list(prefix = prefix):
                if x.name[-1] != '/':
                    # Skip directory "keys". When creating empty directories
                    # a psudeo-key is created. Directories isn't a first-class object on s3
                    if re.match('.*(/dmengine.*|builtins.zip|classes.dex|android-resources.zip|android.jar)$', x.name):
                        name = os.path.relpath(x.name, base_prefix)
                        files.append({'name': name, 'path': '/' + x.name})

            prefix = os.path.join(base_prefix, 'bob')
            for x in bucket.list(prefix = prefix):
                if x.name[-1] != '/':
                    # Skip directory "keys". When creating empty directories
                    # a psudeo-key is created. Directories isn't a first-class object on s3
                    if re.match('.*(/bob.jar)$', x.name):
                        name = os.path.relpath(x.name, base_prefix)
                        files.append({'name': name, 'path': '/' + x.name})

            prefix = os.path.join(base_prefix, self.channel, 'editor')
            for x in bucket.list(prefix = prefix):
                if x.name[-1] != '/':
                    # Skip directory "keys". When creating empty directories
                    # a psudeo-key is created. Directories isn't a first-class object on s3
                    if re.match('.*(/Defold-*)$', x.name):
                        name = os.path.relpath(x.name, base_prefix)
                        files.append({'name': name, 'path': '/' + x.name})
            return files

        tags = self.exec_shell_command("git for-each-ref --sort=taggerdate --format '%(*objectname) %(refname)' refs/tags").split('\n')
        tags.reverse()
        releases = []
        for line in tags:
            line = line.strip()
            if not line:
                continue
            m = re.match('(.*?) refs/tags/(.*?)$', line)
            sha1, tag = m.groups()
            epoch = self.exec_shell_command('git log -n1 --pretty=%%ct %s' % sha1.strip())
            date = datetime.fromtimestamp(float(epoch))
            files = get_files(sha1)
            if len(files) > 0:
                releases.append({'tag': tag,
                                 'sha1': sha1,
                                 'abbrevsha1': sha1[:7],
                                 'date': str(date),
                                 'files': files})

        return releases


    def release(self):
        page = """
<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Defold Downloads</title>
        <link href='//fonts.googleapis.com/css?family=Open+Sans:400,300' rel='stylesheet' type='text/css'>
        <link rel="stylesheet" href="//d.defold.com/static/bootstrap/css/bootstrap.min.css">
        <style>
            body {
                padding-top: 50px;
            }
            .starter-template {
                padding: 40px 15px;
                text-align: center;
            }
            #eula-text{
                height: 400px;
                overflow: scroll;
            }
        </style>

    </head>
    <body>
    <div class="navbar navbar-fixed-top">
        <div class="navbar-inner">
            <div class="container">
                <a class="brand" href="/">Defold Downloads</a>
                <ul class="nav">
                </ul>
            </div>
        </div>
    </div>

    <div class="container">

        <div id="releases"></div>
        <script src="//ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js"></script>
        <script src="//d.defold.com/static/bootstrap/js/bootstrap.min.js"></script>
        <script src="//cdnjs.cloudflare.com/ajax/libs/mustache.js/0.7.2/mustache.min.js"></script>

        <div id="eula" class="container">
            <div class="well well-large">
                <div id="eula-text"></div>
            </div>
            <div id="eula-form" class="alert alert-success">
                <input type="checkbox" id="accept">
                <b>Check to verify that you have read and accepted the "Defold Terms of Service" above.</b>
            </div>
        </div>

        <script id="templ-releases" type="text/html">
            <h2>Editor</h2>
            {{#editor.stable}}
                <p>
                    <a href="{{url}}" class="btn btn-primary" style="width: 20em;" role="button">Download for {{name}}</a>
                </p>
            {{/editor.stable}}

            {{#has_releases}}
                <h2>Releases</h2>
            {{/has_releases}}

            {{#releases}}
                <div class="panel-group" id="accordion">
                  <div class="panel panel-default">
                    <div class="panel-heading">
                      <h4 class="panel-title">
                        <a data-toggle="collapse" data-parent="#accordion" href="#{{sha1}}">
                          <h3>{{tag}} <small>{{date}} ({{abbrevsha1}})</small></h3>
                        </a>
                      </h4>
                    </div>
                    <div id="{{sha1}}" class="panel-collapse collapse ">
                      <div class="panel-body">
                        <table class="table table-striped">
                          <tbody>
                            {{#files}}
                            <tr><td><a href="{{path}}">{{name}}</a></td></tr>
                            {{/files}}
                            {{^files}}
                            <i>No files</i>
                            {{/files}}
                          </tbody>
                        </table>
                      </div>
                    </div>
                  </div>
                </div>
            {{/releases}}
        </script>

        <script>
            var model = %(model)s
            if(document.cookie.match(/^eula-accepted.*/)) {
                // Eula accepted
                $("#eula").html('');
                var output = Mustache.render($('#templ-releases').html(), model);
                $("#releases").html(output);
            } else {
                // Show eula
                var xhttp = new XMLHttpRequest();
                xhttp.onreadystatechange = function() {
                    if (this.readyState == 4 && this.status == 200) {
                        $("#eula-text").html(this.responseText);
                    }
                };
                xhttp.open("GET", "https://www.defold.com/terms-and-conditions/", true);
                xhttp.send();
                $("#accept").change(function() {
                    if(this.checked) {
                        alert('Thank you for accepting the Defold Terms of Service. You may now download Defold software.');
                        document.cookie = 'eula-accepted=yes; expires=Fri, 31 Dec 9999 23:59:59 GMT';
                        location.reload();
                    }
                });
            }
        </script>
      </body>
</html>
"""

        artifacts = """<?xml version='1.0' encoding='UTF-8'?>
<?compositeArtifactRepository version='1.0.0'?>
<repository name='"Defold"'
    type='org.eclipse.equinox.internal.p2.artifact.repository.CompositeArtifactRepository'
    version='1.0.0'>
  <children size='1'>
      <child location='http://%(host)s/archive/%(sha1)s/%(channel)s/editor/repository'/>
  </children>
</repository>"""

        content = """<?xml version='1.0' encoding='UTF-8'?>
<?compositeMetadataRepository version='1.0.0'?>
<repository name='"Defold"'
    type='org.eclipse.equinox.internal.p2.metadata.repository.CompositeMetadataRepository'
    version='1.0.0'>
  <children size='1'>
      <child location='http://%(host)s/archive/%(sha1)s/%(channel)s/editor/repository'/>
  </children>
</repository>
"""

        if self.exec_shell_command('git config -l').find('remote.origin.url') != -1:
            # NOTE: Only run fetch when we have a configured remote branch.
            # When running on buildbot we don't but fetching should not be required either
            # as we're already up-to-date
            self._log('Running git fetch to get latest tags and refs...')
            self.exec_shell_command('git fetch')

        u = urlparse.urlparse(self.archive_path)
        bucket = self._get_s3_bucket(u.hostname)
        host = 'd.defold.com'

        model = {'releases': [],
                 'has_releases': False}

        if self.channel == 'stable':
            # Move artifacts to a separate page?
            model['releases'] = self._get_tagged_releases()
            model['has_releases'] = True

        # NOTE
        # - The stable channel is based on the latest tag
        # - The beta and alpha channels are based on the latest
        #   commit in their branches, i.e. origin/dev for alpha
        if self.channel == 'stable':
            release_sha1 = model['releases'][0]['sha1']
        else:
            release_sha1 = self._git_sha1()

        if sys.stdin.isatty():
            sys.stdout.write('Release %s with SHA1 %s to channel %s? [y/n]: ' % (self.version, release_sha1, self.channel))
            response = sys.stdin.readline()
            if response[0] != 'y':
                return

        model['editor'] = {'stable': [ dict(name='Mac OSX', url='/%s/Defold-macosx.cocoa.x86_64.dmg' % self.channel),
                                       dict(name='Windows', url='/%s/Defold-win32.win32.x86.zip' % self.channel),
                                       dict(name='Linux (64-bit)', url='/%s/Defold-linux.gtk.x86_64.zip' % self.channel),
                                       dict(name='Linux (32-bit)', url='/%s/Defold-linux.gtk.x86.zip' % self.channel)] }

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

        # Create redirection keys for editor
        for name in ['Defold-macosx.cocoa.x86_64.dmg', 'Defold-win32.win32.x86.zip', 'Defold-linux.gtk.x86.zip', 'Defold-linux.gtk.x86_64.zip']:
            key_name = '%s/%s' % (self.channel, name)
            redirect = '/archive/%s/%s/editor/%s' % (release_sha1, self.channel, name)
            self._log('Creating link from %s -> %s' % (key_name, redirect))
            key = bucket.new_key(key_name)
            key.set_redirect(redirect)

        for name, template in [['compositeArtifacts.xml', artifacts], ['compositeContent.xml', content]]:
            full_name = '%s/update/%s' % (self.channel, name)
            self._log('Uploading %s' % full_name)
            key = bucket.new_key(full_name)
            key.content_type = 'text/xml'
            key.set_contents_from_string(template % {'host': host,
                                                     'sha1': release_sha1,
                                                     'channel': self.channel})

    def _get_s3_archive_prefix(self):
        u = urlparse.urlparse(self.archive_path)
        assert (u.scheme == 's3')
        sha1 = self._git_sha1()
        prefix = os.path.join(u.path, sha1)[1:]
        return prefix

    # TODO: Fix upload of .dmg (currently aborts if it finds it in the beta branch)
    def sign_editor(self):
        u = urlparse.urlparse(self.archive_path)
        bucket_name = u.hostname
        bucket = self._get_s3_bucket(bucket_name)
        prefix = self._get_s3_archive_prefix()
        bucket_items = bucket.list(prefix=prefix)

        candidates = {}
        print("Searching for editor signing candidates ...")
        for entry in bucket_items:
            entrypath = os.path.dirname(entry.key)
            if entry.key.endswith('Defold-macosx.cocoa.x86_64.zip'):
                if entrypath not in candidates.keys():
                    candidates[entrypath] = {}
                candidates[entrypath]['zip'] = entry
            if entry.key.endswith('Defold-macosx.cocoa.x86_64.dmg'):
                if entrypath not in candidates.keys():
                    candidates[entrypath] = {}
                candidates[entrypath]['dmg'] = entry

        print("Found %d candidate(s) for editor signing ..." % len(candidates.keys()))
        for candidate in candidates.keys():
            if 'zip' in candidates[candidate].keys():
                if 'dmg' in candidates[candidate].keys():
                    print("Pruning candidate, dmg found: %s" % candidate)
                    del candidates[candidate]
            elif 'dmg' in candidates[candidate].keys():
                print("Pruning candidate, dmg found but zip missing: %s" % candidate)
                del candidates[candidate]

        print("Found %d true candidate(s) for editor signing ..." % (len(candidates.keys())))
        result = False
        for candidate in candidates.keys():
            candidate = candidates[candidate]['zip']
            root = None
            builddir = None
            try:
                root = tempfile.mkdtemp(prefix='defsign.')
                builddir = os.path.join(root, 'build')
                self._mkdirs(builddir)

                # Download the editor from S3
                filepath = os.path.join(root, 'Defold-macosx.cocoa.x86_64.zip')
                if not os.path.isfile(filepath):
                    self._log('Downloading s3://%s/%s -> %s' % (bucket_name, candidate.name, filepath))
                    candidate.get_contents_to_filename(filepath)
                    self._log('Downloaded s3://%s/%s -> %s' % (bucket_name, candidate.name, filepath))

                # Prepare the build directory to create a signed container
                self._log('Signing %s' % (filepath))
                dp_defold = os.path.join(builddir, 'Defold-macosx.cocoa.x86_64')
                fp_defold_app = os.path.join(dp_defold, 'Defold.app')
                fp_defold_dmg = os.path.join(root, 'Defold-macosx.cocoa.x86_64.dmg')

                self.exec_command(['unzip', '-qq', '-o', filepath, '-d', dp_defold])
                self.exec_command(['chmod', '-R', '755', dp_defold])
                os.symlink('/Applications', os.path.join(builddir, 'Applications'))

                # Create a signature for Defold.app and the container
                # This certificate must be installed on the computer performing the operation
                certificate = 'Developer ID Application: Midasplayer Technology AB (ATT58V7T33)'
                self.exec_command(['codesign', '--deep', '-s', certificate, fp_defold_app])
                self.exec_command(['hdiutil', 'create', '-volname', 'Defold', '-srcfolder', builddir, fp_defold_dmg])

                # This step is intermittently failing. In such cases, we retry a few more times
                # E.g. "/var/folders/6l/p6nhw59s2ns2x9chznhy9k1r0000gp/T/defsign.MamsrV/Defold-macosx.cocoa.x86_64.dmg: The timestamp service is not available."
                max_tries = 5

                for i in range(max_tries):
                    try:
                        self.exec_command_no_quit(['codesign', '-s', certificate, fp_defold_dmg])
                        break # it went ok, let's continue the process
                    except ExecException, e:
                        self._log("Failed attempt %d/%d" % (i+1, max_tries))
                        if i == max_tries-1:
                            sys.exit(e.retcode)
                    time.sleep(10) # seconds

                self._log('Signed %s' % (fp_defold_dmg))

                # Upload the signed container to S3
                target = "s3://%s/%s.dmg" % (bucket_name, candidate.name[:-4])
                self.upload_file(fp_defold_dmg, target)
                result = True
            finally:
                self.wait_uploads()
                if root is not None:
                    if builddir is not None:
                        if os.path.islink(os.path.join(builddir, 'Applications')):
                            os.unlink(os.path.join(builddir, 'Applications'))
                    shutil.rmtree(root)

        return result

    def sync_archive(self):
        u = urlparse.urlparse(self.archive_path)
        bucket_name = u.hostname
        bucket = self._get_s3_bucket(bucket_name)

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
        prefix = self._get_s3_archive_prefix()
        for key in bucket.list(prefix = prefix):
            rel = os.path.relpath(key.name, prefix)

            if not pattern.search(rel):
                p = os.path.join(local_dir, sha1, rel)
                self._mkdirs(os.path.dirname(p))
                f = Future(self.thread_pool, download, key, p)
                futures.append(f)

        for f in futures:
            f()

    def _download_editor2(self, sha1):
        bundles = {
            'x86_64-darwin': 'Defold-x86_64-darwin.dmg',
            'x86_64-linux' : 'Defold-x86_64-linux.zip',
            'x86_64-win32' : 'Defold-x86_64-win32.zip'
        }
        host2 = get_host_platform2()
        bundle = bundles.get(host2)
        if bundle:
            url = 'https://d.defold.com/archive/%s/editor2/%s' % (sha1, bundle)
            path = self._download(url)
            # the dev build currently publishes the editor to <host>/editor2 rather than <host>/archive
            if not path:
                url = 'https://d.defold.com/editor2/%s/editor2/%s' % (sha1, bundle)
                path = self._download(url)
            return path
        else:
            print("No editor2 bundle found for %s" % host2)
            return None

    def _install_editor2(self, bundle):
        host2 = get_host_platform2()
        install_path = join('tmp', 'smoke_test')
        if 'darwin' in host2:
            out = self.exec_command(['hdiutil', 'attach', bundle])
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
            self._extract(bundle, install_path)
            install_path = join(install_path, 'Defold')
            result = {'install_path': install_path,
                      'resources_path': 'Defold',
                      'config': join(install_path, 'config')}
            return result

    def _uninstall_editor2(self, info):
        host2 = get_host_platform2()
        shutil.rmtree(info['install_path'])
        if 'darwin' in host2:
            out = self.exec_command(['hdiutil', 'detach', info['fs']])

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
        bundle = self._download_editor2(sha1)
        info = self._install_editor2(bundle)
        config = ConfigParser()
        config.read(info['config'])
        overrides = {'bootstrap.resourcespath': info['resources_path']}
        java = join('Defold.app', 'Contents', 'Resources', 'packages', 'jre', 'bin', 'java')
        jar = self._get_config(config, 'launcher', 'jar', overrides)
        vmargs = self._get_config(config, 'launcher', 'vmargs', overrides).split(',') + ['-Ddefold.log.dir=.']
        vmargs = filter(lambda x: not str.startswith(x, '-Ddefold.update.url='), vmargs)
        main = self._get_config(config, 'launcher', 'main', overrides)
        game_project = '../../editor/test/resources/geometry_wars/game.project'
        args = [java, '-cp', jar] + vmargs + [main, '--preferences=../../editor/test/resources/smoke_test_prefs.json', game_project]
        robot_jar = '%s/ext/share/java/defold-robot.jar' % self.dynamo_home
        robot_args = [java, '-jar', robot_jar, '-s', '../../share/smoke_test.json', '-o', 'result']
        print('Running robot: %s' % robot_args)
        robot_proc = subprocess.Popen(robot_args, cwd = cwd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell = False)
        time.sleep(2)
        self._log('Running editor: %s' % args)
        ed_proc = subprocess.Popen(args, cwd = cwd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell = False)

        output = robot_proc.communicate()[0]
        if ed_proc.poll() == None:
            ed_proc.terminate()
        self._uninstall_editor2(info)

        result_archive_path = '/'.join(['int.d.defold.com', 'archive', sha1, 'editor2', 'smoke_test'])
        def _findwebfiles(libdir):
            paths = os.listdir(libdir)
            paths = [os.path.join(libdir, x) for x in paths if os.path.splitext(x)[1] in ('.html', '.css', '.png')]
            return paths
        for f in _findwebfiles(join(cwd, 'result')):
            self.upload_file(f, 's3://%s/%s' % (result_archive_path, basename(f)))
        self.wait_uploads()
        self._log('Log: https://s3-eu-west-1.amazonaws.com/%s/index.html' % (result_archive_path))

        if robot_proc.returncode != 0:
            sys.exit(robot_proc.returncode)
        return True

    def _get_s3_bucket(self, bucket_name):
        if bucket_name in self.s3buckets:
            return self.s3buckets[bucket_name]

        config = ConfigParser()
        configpath = os.path.expanduser("~/.s3cfg")
        config.read(configpath)

        key = config.get('default', 'access_key')
        secret = config.get('default', 'secret_key')

        if not (key and secret):
            self._log('key/secret not found in "%s"' % configpath)
            sys.exit(5)

        from boto.s3.connection import S3Connection
        from boto.s3.connection import OrdinaryCallingFormat
        from boto.s3.key import Key

        # NOTE: We hard-code host (region) here and it should not be required.
        # but we had problems with certain buckets with period characters in the name.
        # Probably related to the following issue https://github.com/boto/boto/issues/621
        conn = S3Connection(key, secret, host='s3-eu-west-1.amazonaws.com', calling_format=OrdinaryCallingFormat())
        bucket = conn.get_bucket(bucket_name)
        self.s3buckets[bucket_name] = bucket
        return bucket

    def upload_file(self, path, url):
        url = url.replace('\\', '/')
        self._log('Uploading %s -> %s' % (path, url))

        u = urlparse.urlparse(url)

        if u.netloc == '':
            # Assume scp syntax, e.g. host:path
            if 'win32' in self.host:
                path = path.replace('\\', '/')
                # scp interpret c:\path as a network location (host "c")
                if path[1] == ':':
                    path = "/" + path[:1] + path[2:]

            self.exec_env_command(['ssh', u.scheme, 'mkdir -p %s' % u.path])
            self.exec_env_command(['scp', path, url])
        elif u.scheme == 's3':
            bucket = self._get_s3_bucket(u.netloc)

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
                    with open(filepath, 'r') as fhandle:
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

    def _exec_command(self, arg_list, **kwargs):
        arg_str = arg_list
        if not isinstance(arg_str, basestring):
            arg_str = ' '.join(arg_list)
        self._log('[exec] %s' % arg_str)

        if sys.stdout.isatty():
            # If not on CI, we want the colored output, and we get the output as it runs
            process = subprocess.Popen(arg_list, **kwargs)
            output = process.communicate()[0]
            if process.returncode != 0:
                self._log(output)
        else:
            # On the CI machines, we make sure we produce a steady stream of output
            process = subprocess.Popen(arg_list, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, **kwargs)

            output = ''
            while True:
                line = process.stdout.readline()
                if line != '':
                    output += line
                    self._log(line.rstrip())
                else:
                    break

        if process.wait() != 0:
            raise ExecException(process.returncode, output)

        return output

    def exec_command_no_quit(self, args):
        # Executes a command and raises an ExecException if it fails
        return self._exec_command(args, shell = False)

    def exec_command(self, args):
        # Executes a command, and exits if it fails
        try:
            return self._exec_command(args, shell = False)
        except ExecException, e:
            sys.exit(e.returncode)

    def exec_shell_command(self, args):
        # Executes a command, and exits if it fails
        try:
            return self._exec_command(args, shell = True)
        except ExecException, e:
            sys.exit(e.returncode)

    def _form_env(self):
        env = dict(os.environ)

        ld_library_path = 'DYLD_LIBRARY_PATH' if self.host == 'darwin' else 'LD_LIBRARY_PATH'
        env[ld_library_path] = os.path.pathsep.join(['%s/lib/%s' % (self.dynamo_home, self.target_platform),
                                                     '%s/ext/lib/%s' % (self.dynamo_home, self.host)])

        env['PYTHONPATH'] = os.path.pathsep.join(['%s/lib/python' % self.dynamo_home,
                                                  '%s/build_tools' % self.defold,
                                                  '%s/ext/lib/python' % self.dynamo_home])

        env['DYNAMO_HOME'] = self.dynamo_home

        go_root = '%s/ext/go/%s/go' % (self.dynamo_home, self.target_platform)

        paths = os.path.pathsep.join(['%s/bin/%s' % (self.dynamo_home, self.target_platform),
                                      '%s/bin' % (self.dynamo_home),
                                      '%s/ext/bin' % self.dynamo_home,
                                      '%s/ext/bin/%s' % (self.dynamo_home, self.host),
                                      '%s/bin' % go_root])

        env['PATH'] = paths + os.path.pathsep + env['PATH']

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
            env['GTEST_COLOR'] = 'no'

        env['EMSCRIPTEN'] = self._form_ems_path()

        xhr2_path = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR, 'xhr2', 'lib')
        if 'NODE_PATH' in env:
            env['NODE_PATH'] = xhr2_path + os.path.pathsep + env['NODE_PATH']
        else:
            env['NODE_PATH'] = xhr2_path

        return env

    def exec_env_command(self, args, **kwargs):
        return self._exec_command(args, shell = False, env = self._form_env(), **kwargs)

    def exec_env_shell_command(self, args, **kwargs):
        return self._exec_command(args, shell = True, env = self._form_env(), **kwargs)

if __name__ == '__main__':
    boto_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../packages/boto-2.28.0-py2.7.egg'))
    sys.path.insert(0, boto_path)
    usage = '''usage: %prog [options] command(s)

Commands:
distclean       - Removes the DYNAMO_HOME folder
install_ext     - Install external packages
install_ems     - Install emscripten sdk
sync_archive    - Sync engine artifacts from S3
activate_ems    - Used when changing to a branch that uses a different version of emscripten SDK (resets ~/.emscripten)
build_engine    - Build engine
archive_engine  - Archive engine (including builtins) to path specified with --archive-path
install_go      - Install go dev tools
build_go        - Build go code
archive_go      - Archive go binaries
test_cr         - Test editor and server
build_server    - Build server
build_editor    - Build editor
archive_editor  - Archive editor to path specified with --archive-path
sign_editor     - Sign the editor and upload a dmg archive to S3
archive_server  - Archive server to path specified with --archive-path
build_bob       - Build bob with native libraries included for cross platform deployment
archive_bob     - Archive bob to path specified with --archive-path
build_docs      - Build documentation
build_builtins  - Build builtin content archive
bump            - Bump version number
release         - Release editor
shell           - Start development shell
smoke_test      - Test editor and engine in combination

Multiple commands can be specified

To pass on arbitrary options to waf: build.py OPTIONS COMMANDS -- WAF_OPTIONS
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--eclipse-home', dest='eclipse_home',
                      default = None,
                      help = 'Eclipse directory')

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      choices = ['linux', 'x86_64-linux', 'darwin', 'x86_64-darwin', 'win32', 'x86_64-win32', 'armv7-darwin', 'arm64-darwin', 'armv7-android', 'js-web'],
                      help = 'Target platform')

    parser.add_option('--skip-tests', dest='skip_tests',
                      action = 'store_true',
                      default = False,
                      help = 'Skip unit-tests. Default is false')

    parser.add_option('--skip-codesign', dest='skip_codesign',
                      action = 'store_true',
                      default = False,
                      help = 'skip code signing. Default is false')

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

    default_archive_path = 's3://d.defold.com/archive'
    parser.add_option('--archive-path', dest='archive_path',
                      default = default_archive_path,
                      help = 'Archive build. Set ssh-path, host:path, to archive build to. Default is %s' % default_archive_path    )

    parser.add_option('--set-version', dest='set_version',
                      default = None,
                      help = 'Set version explicitily when bumping version')

    parser.add_option('--eclipse', dest='eclipse',
                      action = 'store_true',
                      default = False,
                      help = 'Output build commands in a format eclipse can parse')

    parser.add_option('--branch', dest='branch',
                      default = None,
                      help = 'Current branch. Used only for symbolic information, such as links to latest editor for a branch')

    parser.add_option('--channel', dest='channel',
                      default = 'stable',
                      help = 'Editor release channel (stable, beta, ...)')

    parser.add_option('--eclipse-version', dest='eclipse_version',
                      default = '3.8',
                      help = 'Eclipse version')

    options, all_args = parser.parse_args()

    args = filter(lambda x: x[:2] != '--', all_args)
    waf_options = filter(lambda x: x[:2] == '--', all_args)

    if len(args) == 0:
        parser.error('No command specified')

    target_platform = options.target_platform if options.target_platform else get_host_platform()

    c = Configuration(dynamo_home = os.environ.get('DYNAMO_HOME', None),
                      target_platform = target_platform,
                      eclipse_home = options.eclipse_home,
                      skip_tests = options.skip_tests,
                      skip_codesign = options.skip_codesign,
                      skip_docs = options.skip_docs,
                      skip_builtins = options.skip_builtins,
                      skip_bob_light = options.skip_bob_light,
                      disable_ccache = options.disable_ccache,
                      no_colors = options.no_colors,
                      archive_path = options.archive_path,
                      set_version = options.set_version,
                      eclipse = options.eclipse,
                      branch = options.branch,
                      channel = options.channel,
                      eclipse_version = options.eclipse_version,
                      waf_options = waf_options)

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
