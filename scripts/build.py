#!/usr/bin/env python

import os, sys, shutil, zipfile, re, itertools, json, platform
import optparse, subprocess, urllib, urlparse, tempfile
import imp
from datetime import datetime
from tarfile import TarFile
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
from glob import glob
from threading import Thread, Event
from Queue import Queue

"""
Build utility for installing external packages, building engine, editor and cr
Run build.py --help for help
"""

PACKAGES_ALL="protobuf-2.3.0 waf-1.5.9 gtest-1.5.0 vectormathlibrary-r1649 junit-4.6 protobuf-java-2.3.0 openal-1.1 maven-3.0.1 ant-1.9.3 vecmath vpx-v0.9.7-p1 asciidoc-8.6.7 facebook-4.4.0 luajit-2.0.3 tremolo-0.0.8 PVRTexLib-4.14.6".split()
PACKAGES_HOST="protobuf-2.3.0 gtest-1.5.0 glut-3.7.6 cg-3.1 openal-1.1 vpx-v0.9.7-p1 PVRTexLib-4.14.6 luajit-2.0.3 tremolo-0.0.8".split()
PACKAGES_EGGS="protobuf-2.3.0-py2.5.egg pyglet-1.1.3-py2.5.egg gdata-2.0.6-py2.6.egg Jinja2-2.6-py2.6.egg".split()
PACKAGES_IOS="protobuf-2.3.0 gtest-1.5.0 facebook-4.4.0 luajit-2.0.3 tremolo-0.0.8".split()
PACKAGES_IOS_64="protobuf-2.3.0 gtest-1.5.0 facebook-4.4.0 tremolo-0.0.8".split()
PACKAGES_DARWIN_64="protobuf-2.3.0 gtest-1.5.0 PVRTexLib-4.14.6 luajit-2.0.3 vpx-v0.9.7-p1 tremolo-0.0.8".split()
PACKAGES_WIN32="PVRTexLib-4.5".split()
PACKAGES_LINUX="PVRTexLib-4.5".split()
PACKAGES_ANDROID="protobuf-2.3.0 gtest-1.5.0 facebook-4.4.1 android-support-v4 android-23 google-play-services-4.0.30 luajit-2.0.3 tremolo-0.0.8 amazon-iap-2.0.16".split()
PACKAGES_EMSCRIPTEN="gtest-1.5.0 protobuf-2.3.0".split()
PACKAGES_EMSCRIPTEN_SDK="emsdk-portable.tar.gz".split()
DEFOLD_PACKAGES_URL = "https://s3-eu-west-1.amazonaws.com/defold-packages"
NODE_MODULE_XHR2_URL = "%s/xhr2-0.1.0-common.tar.gz" % (DEFOLD_PACKAGES_URL)
NODE_MODULE_LIB_DIR = os.path.join("ext", "lib", "node_modules")
EMSCRIPTEN_VERSION_STR = "1.22.0"
# The linux tool does not yet support git tags, so we have to treat it as a special case for the moment.
EMSCRIPTEN_VERSION_STR_LINUX = "master"
EMSCRIPTEN_SDK_OSX = "sdk-{0}-64bit".format(EMSCRIPTEN_VERSION_STR)
EMSCRIPTEN_SDK_LINUX = "sdk-{0}-32bit".format(EMSCRIPTEN_VERSION_STR_LINUX)
EMSCRIPTEN_DIR = join('bin', 'emsdk_portable', 'emscripten', EMSCRIPTEN_VERSION_STR)
EMSCRIPTEN_DIR_LINUX = join('bin', 'emsdk_portable', 'emscripten', EMSCRIPTEN_VERSION_STR_LINUX)
PACKAGES_FLASH="gtest-1.5.0".split()
SHELL = os.environ.get('SHELL', 'bash')

def get_host_platform():
    arch = platform.architecture()[0]
    if sys.platform == 'linux2':
        if arch == '64bit':
            return 'x86_64-linux'
        else:
            return 'linux'
    else:
        return sys.platform

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
        self.target_platform = target_platform

        # Like this, since we cannot guarantee that PYTHONPATH has been set up to include BuildUtility yet.
        # N.B. If we upgrade to move recent versions of python, then the method of module loading should also change.
        build_utility_module = imp.load_source('BuildUtility', os.path.join(self.defold, 'build_tools', 'BuildUtility.py'))
        self.build_utility = build_utility_module.BuildUtility(self.target_platform, self.host, self.dynamo_home)

        self.skip_tests = skip_tests
        self.skip_codesign = skip_codesign
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
        shutil.rmtree(self.dynamo_home)
        # Recreate dirs
        self._create_common_dirs()

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

    def _download(self, url, use_cache = True):
        name = basename(urlparse.urlparse(url).path)
        path = expanduser('~/.dcache/%s' % name)
        if use_cache and os.path.exists(path):
            return path

        if not os.path.exists(dirname(path)):
            os.makedirs(dirname(path), 0755)

        tmp = path + '_tmp'
        with open(tmp, 'wb') as f:
            self._log('Downloading %s %d%%' % (name, 0))
            x = urllib.urlopen(url)
            if x.code != 200:
                return None
            file_len = int(x.headers.get('Content-Length', 0))
            buf = x.read(1024 * 1024)
            n = 0
            while buf:
                n += len(buf)
                self._log('Downloading %s %d%%' % (name, 100 * n / file_len))
                f.write(buf)
                buf = x.read(1024 * 1024)

        if os.path.exists(path): os.unlink(path)
        os.rename(tmp, path)
        return path

    def _install_go(self):
        urls = {'darwin' : 'http://go.googlecode.com/files/go1.1.darwin-amd64.tar.gz',
                'linux' : 'http://go.googlecode.com/files/go1.1.linux-386.tar.gz',
                'x86_64-linux' : 'http://go.googlecode.com/files/go1.1.linux-amd64.tar.gz',
                'win32' : 'http://go.googlecode.com/files/go1.1.windows-386.zip'}
        url = urls[self.host]
        path = self._download(url)
        self._extract(path, self.ext)

    def install_ext(self):
        def make_path(platform):
            return join(self.defold_root, 'packages', p) + '-%s.tar.gz' % platform

        self._install_go()

        for p in PACKAGES_ALL:
            self._extract_tgz(make_path('common'), self.ext)

        for p in PACKAGES_HOST:
            self._extract_tgz(make_path(self.host), self.ext)

        for p in PACKAGES_IOS:
                self._extract_tgz(make_path('armv7-darwin'), self.ext)

        for p in PACKAGES_IOS_64:
                self._extract_tgz(make_path('arm64-darwin'), self.ext)

        for p in PACKAGES_WIN32:
                self._extract_tgz(make_path('win32'), self.ext)

        for p in PACKAGES_LINUX:
                self._extract_tgz(make_path('linux'), self.ext)

        if self.host == 'darwin':
            for p in PACKAGES_DARWIN_64:
                self._extract_tgz(make_path('x86_64-darwin'), self.ext)

        for p in PACKAGES_ANDROID:
            self._extract_tgz(make_path('armv7-android'), self.ext)

        for p in PACKAGES_EMSCRIPTEN:
            self._extract_tgz(make_path('js-web'), self.ext)

        for p in PACKAGES_FLASH:
            self._extract_tgz(make_path('as3-web'), self.ext)

        for egg in glob(join(self.defold_root, 'packages', '*.egg')):
            self._log('Installing %s' % basename(egg))
            self.exec_env_command(['easy_install', '-q', '-d', join(self.ext, 'lib', 'python'), '-N', egg])

        for n in 'js-web-pre.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        for n in 'js-web-pre-engine.js'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'share'))

        for n in itertools.chain(*[ glob('share/*%s' % ext) for ext in ['.mobileprovision', '.xcent', '.supp']]):
            self._copy(join(self.defold_root, n), join(self.dynamo_home, 'share'))

        node_modules_dir = os.path.join(self.dynamo_home, NODE_MODULE_LIB_DIR)
        self._mkdirs(node_modules_dir)
        xhr2_tarball = self._download(NODE_MODULE_XHR2_URL)
        self._extract_tgz(xhr2_tarball, node_modules_dir)

    def _form_ems_path(self):
        path = ''
        if self.host == 'linux':
            path = join(self.ext, EMSCRIPTEN_DIR_LINUX)
        else:
            path = join(self.ext, EMSCRIPTEN_DIR)
        return path

    def install_ems(self):
        url = '%s/emsdk-%s-%s.tar.gz' % (DEFOLD_PACKAGES_URL, EMSCRIPTEN_VERSION_STR, self.host)
        dlpath = self._download(url)
        self._extract(dlpath, self.ext)
        self.activate_ems()
        os.environ['EMSCRIPTEN'] = self._form_ems_path()

    def get_ems_sdk_name(self):
        sdk = EMSCRIPTEN_SDK_OSX
        if 'linux' == self.host:
            sdk = EMSCRIPTEN_SDK_LINUX
        return sdk;

    def get_ems_exe_path(self):
        return join(self.ext, 'bin', 'emsdk_portable', 'emsdk')

    def activate_ems(self):
        self.exec_env_command([self.get_ems_exe_path(), 'activate', self.get_ems_sdk_name()])

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

        zip = zipfile.ZipFile(outfile, 'w')
        for root, dirs, files in os.walk(path):
            for f in files:
                p = os.path.join(root, f)
                an = p
                if directory:
                    an = os.path.relpath(p, directory)
                zip.write(p, an)

        zip.close()
        return outfile.name

    def is_cross_platform(self):
        return self.host != self.target_platform

    def archive_engine(self):
        exe_prefix = ''
        if self.target_platform == 'win32':
            exe_ext = '.exe'
        elif 'android' in self.target_platform:
            exe_prefix = 'lib'
            exe_ext = '.so'
        elif 'js-web' in self.target_platform:
            exe_prefix = ''
            exe_ext = '.js'
        else:
            exe_ext = ''

        lib_ext = ''
        lib_prefix = 'lib'
        if 'darwin' in self.target_platform:
            lib_ext = '.dylib'
        elif 'win32' in self.target_platform:
            lib_ext = '.dll'
            lib_prefix = ''
        else:
            lib_ext = '.so'

        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'engine', self.target_platform).replace('\\', '/')
        share_archive_path = join(self.archive_path, sha1, 'engine', 'share').replace('\\', '/')
        java_archive_path = join(self.archive_path, sha1, 'engine', 'share', 'java').replace('\\', '/')
        dynamo_home = self.dynamo_home

        bin_dir = self.build_utility.get_binary_path()
        lib_dir = self.target_platform

        # upload editor 2.0 launcher
        if self.target_platform in ['linux', 'x86_64-linux', 'darwin', 'x86_64-darwin', 'win32']:
            launcherbin = join(bin_dir, "launcher" + exe_ext)
            self.upload_file(launcherbin, '%s/%s%s' % (full_archive_path, "launcher", exe_ext))

        for n in ['dmengine', 'dmengine_release', 'dmengine_headless']:
            engine = join(bin_dir, exe_prefix + n + exe_ext)
            self.upload_file(engine, '%s/%s%s%s' % (full_archive_path, exe_prefix, n, exe_ext))
            if self.target_platform == 'js-web':
                self.upload_file(join(bin_dir, 'defold_sound.swf'), join(full_archive_path, 'defold_sound.swf'))
                engine_mem = join(bin_dir, exe_prefix + n + exe_ext + '.mem')
                if os.path.exists(engine_mem):
                    self.upload_file(engine_mem, '%s/%s%s%s.mem' % (full_archive_path, exe_prefix, n, exe_ext))

        if self.target_platform == 'linux':
            # NOTE: It's arbitrary for which platform we archive builtins and doc. Currently set to linux
            builtins = self._ziptree(join(dynamo_home, 'content', 'builtins'), directory = join(dynamo_home, 'content'))
            self.upload_file(builtins, '%s/builtins.zip' % (share_archive_path))

            doc = self._ziptree(join(dynamo_home, 'share', 'doc'), directory = join(dynamo_home, 'share'))
            self.upload_file(doc, '%s/ref-doc.zip' % (share_archive_path))

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
        if not self.is_cross_platform() or self.target_platform == 'x86_64-darwin':
            libs.append('texc')
        for lib in libs:
            lib_path = join(dynamo_home, 'lib', lib_dir, '%s%s_shared%s' % (lib_prefix, lib, lib_ext))
            self.upload_file(lib_path, '%s/%s%s_shared%s' % (full_archive_path, lib_prefix, lib, lib_ext))

    def build_engine(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        skip_codesign = '--skip-codesign' if self.skip_codesign else ''
        disable_ccache = '--disable-ccache' if self.disable_ccache else ''

        eclipse = '--eclipse' if self.eclipse else ''

        libs="dlib ddf particle glfw graphics lua hid input physics resource extension script tracking render gameobject gui sound gamesys tools record iap push iac adtruth facebook crash engine".split()

        # Base platforms is the set of platforms to build the base libs for
        # The base libs are the libs needed to build bob, i.e. contains compiler code
        base_platforms = [self.host]
        if self.host == 'darwin':
            base_platforms.append('x86_64-darwin')
        # NOTE: We run waf using python <PATH_TO_WAF>/waf as windows don't understand that waf is an executable
        base_libs = ['dlib', 'texc']
        for platform in base_platforms:
            for lib in base_libs:
                self._log('Building %s for %s platform' % (lib, platform if platform != self.host else "host"))
                cwd = join(self.defold_root, 'engine/%s' % (lib))
                pf_arg = "--platform=%s" % (platform)
                cmd = 'python %s/ext/bin/waf --prefix=%s %s %s %s %s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, pf_arg, skip_tests, skip_codesign, disable_ccache, eclipse)
                self.exec_env_command(cmd.split() + self.waf_options, cwd = cwd)

        self._log('Building bob')

        self.exec_env_command(" ".join([join(self.dynamo_home, 'ext/share/ant/bin/ant'), 'clean', 'install']),
                              cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob'),
                              shell = True)

        for lib in libs:
            self._log('Building %s' % lib)
            cwd = join(self.defold_root, 'engine/%s' % lib)
            cmd = 'python %s/ext/bin/waf --prefix=%s --platform=%s %s %s %s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, self.target_platform, skip_tests, skip_codesign, disable_ccache, eclipse)
            self.exec_env_command(cmd.split() + self.waf_options, cwd = cwd)

    def build_go(self):
        # TODO: shell=True is required only on windows
        # otherwise it fails. WHY?
        if not self.skip_tests:
            self.exec_env_command('go test defold/...', shell=True)
        self.exec_env_command('go install defold/...', shell=True)
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

    def build_bob(self):
        # NOTE: A bit expensive to sync everything
        self._sync_archive()
        cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')

        for s in ["copy.sh"]:
            self.exec_env_command("./scripts/%s" % s, cwd = cwd, shell = True)

        self.exec_env_command(" ".join([join(self.dynamo_home, 'ext/share/ant/bin/ant'), 'clean', 'install-full']),
                          cwd = cwd,
                          shell = True)

    def build_docs(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        self._log('Building docs')
        cwd = join(self.defold_root, 'engine/docs')
        cmd = 'python %s/ext/bin/waf configure --prefix=%s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, skip_tests)
        self.exec_env_command(cmd.split() + self.waf_options, cwd = cwd)

    def test_cr(self):
        # NOTE: A bit expensive to sync everything
        self._sync_archive()
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

        with open('com.dynamo.cr/com.dynamo.cr.editor-product/cr-generated.product', 'w') as f:
            f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
            f.write('<?pde version="3.5"?>\n')
            f.write('\n')
            tree.write(f, encoding='utf-8')

        p2 = """
instructions.configure=\
  addRepository(type:0,location:http${#58}//d.defold.com.s3-website-eu-west-1.amazonaws.com/%(channel)s/update/);\
  addRepository(type:1,location:http${#58}//d.defold.com.s3-website-eu-west-1.amazonaws.com/%(channel)s/update/);
"""

        with open('com.dynamo.cr/com.dynamo.cr.editor-product/cr-generated.p2.inf', 'w') as f:
            f.write(p2 % { 'channel': self.channel })

        self._build_cr('editor')

    def _archive_cr(self, product, build_dir):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, product).replace('\\', '/') + '/'
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
        self._sync_archive()
        cwd = join(self.defold_root, 'com.dynamo.cr', 'com.dynamo.cr.parent')
        self.exec_env_command([join(self.dynamo_home, 'ext/share/maven/bin/mvn'), 'clean', 'package', '-P', product, '-Declipse-version=%s' % self.eclipse_version], cwd = cwd)

    def check_editor2_reflections(self):
        cwd = join(self.defold_root, 'editor')
        reflection_prefix = 'Reflection warning, ' # final space important
        included_reflections = ['editor/'] # [] = include all
        ignored_reflections = []

        # lein check puts reflection warnings on stderr, redirect to stdout to capture all output
        output = self.exec_env_command(['./scripts/lein', 'check'], cwd=cwd, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
        lines = output.splitlines()
        reflection_lines = (line for line in lines if re.match(reflection_prefix, line))
        reflections = (re.match('(' + reflection_prefix + ')(.*)', line).group(2) for line in reflection_lines)
        filtered_reflections = reflections if not included_reflections else (line for line in reflections if any((re.match(include, line) for include in included_reflections)))
        failures = list(line for line in filtered_reflections if not any((re.match(ignored, line) for ignored in ignored_reflections)))

        if failures:
            for failure in failures:
                print(failure)
            exit(1)

    def build_editor2(self):
        cwd = join(self.defold_root, 'editor')
        self.exec_env_command(['./scripts/install_jars'], cwd = cwd)
        self.exec_env_command(['./scripts/lein', 'clean'], cwd = cwd)
        self.exec_env_command(['./scripts/lein', 'protobuf'], cwd = cwd)
        self.exec_env_command(['./scripts/lein', 'builtins'], cwd = cwd)
        self.check_editor2_reflections()
        self.exec_env_command(['./scripts/lein', 'test'], cwd = cwd)

        # TODO: Version
        self.exec_env_command(['./scripts/bundle.py', '--platform=x86_64-darwin', '--platform=x86-linux', '--platform=x86-win32', '--version=2.0.0'], cwd = cwd)

    def archive_editor2(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'editor2')
        for p in glob(join(self.defold_root, 'editor', 'target', 'editor', 'Defold*.zip')):
            self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))
        for p in glob(join(self.defold_root, 'editor', 'target', 'editor', 'update', '*')):
            self.upload_file(p, '%s/%s' % (full_archive_path, basename(p)))

        u = urlparse.urlparse(self.archive_path)
        bucket = self._get_s3_bucket(u.hostname)
        host = bucket.get_website_endpoint()

        release_sha1 = self._git_sha1()

        self._log('Uploading update.json')
        key = bucket.new_key('editor2/update.json')
        key.content_type = 'application/json'
        key.set_contents_from_string(json.dumps({'url': 'http://%(host)s/editor2/%(sha1)s/editor2' % {'host': host, 'sha1': release_sha1}}))

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
        if "win32" == self.host:
            preexec_fn = None
        else:
            preexec_fn = self.check_ems
        self.exec_env_command([SHELL, '-l'], preexec_fn=preexec_fn)

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
            return files

        tags = self.exec_command("git for-each-ref --sort=taggerdate --format '%(*objectname) %(refname)' refs/tags").split('\n')
        tags.reverse()
        releases = []
        for line in tags:
            line = line.strip()
            if not line:
                continue
            m = re.match('(.*?) refs/tags/(.*?)$', line)
            sha1, tag = m.groups()
            epoch = self.exec_command('git log -n1 --pretty=%%ct %s' % sha1.strip())
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
        <link href='http://fonts.googleapis.com/css?family=Open+Sans:400,300' rel='stylesheet' type='text/css'>
        <link rel="stylesheet" href="http://defold-cdn.s3-website-eu-west-1.amazonaws.com/bootstrap/css/bootstrap.min.css">

        <style>
            body {
                padding-top: 50px;
            }
            .starter-template {
                padding: 40px 15px;
                text-align: center;
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
        <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js"></script>
        <script src="http://defold-cdn.s3-website-eu-west-1.amazonaws.com/bootstrap/js/bootstrap.min.js"></script>
        <script src="http://cdnjs.cloudflare.com/ajax/libs/mustache.js/0.7.2/mustache.min.js"></script>

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
            var output = Mustache.render($('#templ-releases').html(), model);
            $("#releases").html(output);
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
      <child location='http://%(host)s/archive/%(sha1)s/editor/repository'/>
  </children>
</repository>"""

        content = """<?xml version='1.0' encoding='UTF-8'?>
<?compositeMetadataRepository version='1.0.0'?>
<repository name='"Defold"'
    type='org.eclipse.equinox.internal.p2.metadata.repository.CompositeMetadataRepository'
    version='1.0.0'>
  <children size='1'>
      <child location='http://%(host)s/archive/%(sha1)s/editor/repository'/>
  </children>
</repository>
"""

        if self.exec_command('git config -l').find('remote.origin.url') != -1:
            # NOTE: Only run fetch when we have a configured remote branch.
            # When running on buildbot we don't but fetching should not be required either
            # as we're already up-to-date
            self._log('Running git fetch to get latest tags and refs...')
            self.exec_command('git fetch')

        u = urlparse.urlparse(self.archive_path)
        bucket = self._get_s3_bucket(u.hostname)
        host = bucket.get_website_endpoint()

        model = {'releases': [],
                 'has_releases': False}

        if self.channel == 'stable':
            # Move artifacts to a separate page?
            model['releases'] = self._get_tagged_releases()
            model['has_releases'] = True

        # NOTE
        # - The stable channel is based on the latest tag
        # - The beta channel is based on the latest commit in the dev-branch, i.e. origin/dev
        if self.channel == 'stable':
            release_sha1 = model['releases'][0]['sha1']
        elif self.channel == 'beta':
            release_sha1 = self._git_sha1()
        else:
            raise Exception('Unknown channel %s' % self.channel)

        if sys.stdin.isatty():
            sys.stdout.write('Release %s with SHA1 %s to channel %s? [y/n]: ' % (self.version, release_sha1, self.channel))
            response = sys.stdin.readline()
            if response[0] != 'y':
                return

        model['editor'] = {'stable': [ dict(name='Mac OSX', url='/%s/Defold-macosx.cocoa.x86_64.zip' % self.channel),
                                       dict(name='Windows', url='/%s/Defold-win32.win32.x86.zip' % self.channel),
                                       dict(name='Linux', url='/%s/Defold-linux.gtk.x86.zip' % self.channel)] }

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
        for name in ['Defold-macosx.cocoa.x86_64.zip', 'Defold-win32.win32.x86.zip', 'Defold-linux.gtk.x86.zip']:
            key_name = '%s/%s' % (self.channel, name)
            redirect = '/archive/%s/editor/%s' % (release_sha1, name)
            self._log('Creating link from %s -> %s' % (key_name, redirect))
            key = bucket.new_key(key_name)
            key.set_redirect(redirect)

        for name, template in [['compositeArtifacts.xml', artifacts], ['compositeContent.xml', content]]:
            full_name = '%s/update/%s' % (self.channel, name)
            self._log('Uploading %s' % full_name)
            key = bucket.new_key(full_name)
            key.content_type = 'text/xml'
            key.set_contents_from_string(template % {'host': host,
                                                     'sha1': release_sha1})

    def _get_s3_archive_prefix(self):
        u = urlparse.urlparse(self.archive_path)
        assert (u.scheme == 's3')
        sha1 = self._git_sha1()
        prefix = os.path.join(u.path, sha1)[1:]
        return prefix

    def _sync_archive(self):
        u = urlparse.urlparse(self.archive_path)
        bucket_name = u.hostname
        bucket = self._get_s3_bucket(bucket_name)

        local_dir = os.path.join(self.dynamo_home, 'archive')
        self._mkdirs(local_dir)

        if not self.thread_pool:
            self.thread_pool = ThreadPool(8)

        def download(key, path):
            key.get_contents_to_filename(path)

        futures = []
        sha1 = self._git_sha1()
        # Only s3 is supported (scp is deprecated)
        prefix = self._get_s3_archive_prefix()
        for key in bucket.list(prefix = prefix):
            rel = os.path.relpath(key.name, prefix)
            if rel.split('/')[0] != 'editor':
                p = os.path.join(local_dir, sha1, rel)
                self._mkdirs(os.path.dirname(p))
                self._log('s3://%s/%s -> %s' % (bucket_name, key.name, p))
                f = Future(self.thread_pool, download, key, p)
                futures.append(f)

        for f in futures:
            f()

    def _get_s3_bucket(self, bucket_name):
        if bucket_name in self.s3buckets:
            return self.s3buckets[bucket_name]

        from ConfigParser import ConfigParser
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
        self._log('%s -> %s' % (path, url))

        u = urlparse.urlparse(url)

        if u.netloc == '':
            # Assume scp syntax, e.g. host:path
            if self.host == 'win32':
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

            def upload():
                key = bucket.new_key(p)
                key.set_contents_from_filename(path)

            f = Future(self.thread_pool, upload)
            self.futures.append(f)

        else:
            raise Exception('Unsupported url %s' % (url))

    def wait_uploads(self):
        for f in self.futures:
            f()
        self.futures = []

    def exec_command(self, args):
        process = subprocess.Popen(args, stdout = subprocess.PIPE, shell = True)

        output = process.communicate()[0]
        if process.returncode != 0:
            self._log(output)
            sys.exit(process.returncode)
        return output

    def _form_env(self):
        env = dict(os.environ)

        ld_library_path = 'DYLD_LIBRARY_PATH' if self.host == 'darwin' else 'LD_LIBRARY_PATH'
        env[ld_library_path] = os.path.pathsep.join(['%s/lib/%s' % (self.dynamo_home, self.target_platform),
                                                     '%s/ext/lib/%s' % (self.dynamo_home, self.host)])

        env['PYTHONPATH'] = os.path.pathsep.join(['%s/lib/python' % self.dynamo_home,
                                                  '%s/build_tools' % self.defold,
                                                  '%s/ext/lib/python' % self.dynamo_home])

        env['DYNAMO_HOME'] = self.dynamo_home

        paths = os.path.pathsep.join(['%s/bin/%s' % (self.dynamo_home, self.target_platform),
                                      '%s/bin' % (self.dynamo_home),
                                      '%s/ext/bin' % self.dynamo_home,
                                      '%s/ext/bin/%s' % (self.dynamo_home, self.host),
                                      '%s/ext/go/bin' % self.dynamo_home])

        env['PATH'] = paths + os.path.pathsep + env['PATH']

        go_paths = os.path.pathsep.join(['%s/go' % self.dynamo_home,
                                         join(self.defold, 'go')])
        env['GOPATH'] = go_paths
        env['GOROOT'] = '%s/go' % self.ext

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

    def exec_env_command(self, arg_list, **kwargs):
        env = self._form_env()
        process = subprocess.Popen(arg_list, env = env, **kwargs)
        output = process.communicate()[0]

        if process.returncode != 0:
            self._log(output)
            sys.exit(process.returncode)
        return output

if __name__ == '__main__':
    boto_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../packages/boto-2.28.0-py2.7.egg'))
    sys.path.insert(0, boto_path)
    usage = '''usage: %prog [options] command(s)

Commands:
distclean       - Removes the DYNAMO_HOME folder
install_ext     - Install external packages
install_ems     - Install emscripten sdk
activate_ems    - Used when changing to a branch that uses a different version of emscripten SDK (resets ~/.emscripten)
build_engine    - Build engine
archive_engine  - Archive engine (including builtins) to path specified with --archive-path
build_go        - Build go code
archive_go      - Archive go binaries
test_cr         - Test editor and server
build_server    - Build server
build_editor    - Build editor
archive_editor  - Archive editor to path specified with --archive-path
archive_server  - Archive server to path specified with --archive-path
build_bob       - Build bob with native libraries included for cross platform deployment
archive_bob     - Archive bob to path specified with --archive-path
build_docs      - Build documentation
bump            - Bump version number
release         - Release editor
shell           - Start development shell

Multiple commands can be specified

To pass on arbitrary options to waf: build.py OPTIONS COMMANDS -- WAF_OPTIONS
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--eclipse-home', dest='eclipse_home',
                      default = None,
                      help = 'Eclipse directory')

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      choices = ['linux', 'x86_64-linux', 'darwin', 'x86_64-darwin', 'win32', 'armv7-darwin', 'arm64-darwin', 'armv7-android', 'js-web'],
                      help = 'Target platform')

    parser.add_option('--skip-tests', dest='skip_tests',
                      action = 'store_true',
                      default = False,
                      help = 'Skip unit-tests. Default is false')

    parser.add_option('--skip-codesign', dest='skip_codesign',
                      action = 'store_true',
                      default = False,
                      help = 'skip code signing. Default is false')

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
        f()
        c.wait_uploads()
