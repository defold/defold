#!/usr/bin/env python

import os, sys, shutil, zipfile, re, itertools
import optparse, subprocess, urllib, urlparse
from tarfile import TarFile
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
from glob import glob

"""
Build utility for installing external packages, building engine, editor and cr
Run build.py --help for help
"""

PACKAGES_ALL="protobuf-2.3.0 waf-1.5.9 gtest-1.5.0 vectormathlibrary-r1649 nvidia-texture-tools-2.0.6 PIL-1.1.6 junit-4.6 protobuf-java-2.3.0 openal-1.1 maven-3.0.1 ant-1.9.3 vecmath vpx-v0.9.7-p1 asciidoc-8.6.7".split()
PACKAGES_HOST="protobuf-2.3.0 gtest-1.5.0 glut-3.7.6 cg-2.1 nvidia-texture-tools-2.0.6 PIL-1.1.6 openal-1.1 vpx-v0.9.7-p1 PVRTexLib-4.5".split()
PACKAGES_EGGS="protobuf-2.3.0-py2.5.egg pyglet-1.1.3-py2.5.egg gdata-2.0.6-py2.6.egg Jinja2-2.6-py2.6.egg".split()
PACKAGES_IOS="protobuf-2.3.0 gtest-1.5.0 facebook-3.5.3".split()
PACKAGES_DARWIN_64="protobuf-2.3.0 gtest-1.5.0 PVRTexLib-4.5".split()
PACKAGES_ANDROID="protobuf-2.3.0 gtest-1.5.0 facebook-3.7 android-support-v4 android-4.2.2 google-play-services-4.0.30".split()

def get_host_platform():
    return 'linux' if sys.platform == 'linux2' else sys.platform

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
                 branch = None):

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
        self.skip_tests = skip_tests
        self.skip_codesign = skip_codesign
        self.disable_ccache = disable_ccache
        self.no_colors = no_colors
        self.archive_path = archive_path
        self.set_version = set_version
        self.eclipse = eclipse
        self.branch = branch

        self._create_common_dirs()

    def _create_common_dirs(self):
        for p in ['ext/lib/python', 'lib/python', 'share']:
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
            self.exec_command(['tar', 'xfz', file], cwd = path)
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

    def _download(self, url):
        name = basename(urlparse.urlparse(url).path)
        path = expanduser('~/.dcache/%s' % name)
        if os.path.exists(path):
            return path

        if not os.path.exists(dirname(path)):
            os.makedirs(dirname(path), 0755)

        tmp = path + '_tmp'
        with open(tmp, 'wb') as f:
            self._log('Downloading %s %d%%' % (name, 0))
            x = urllib.urlopen(url)
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

        if self.host == 'darwin':
            for p in PACKAGES_DARWIN_64:
                self._extract_tgz(make_path('x86_64-darwin'), self.ext)

        for p in PACKAGES_ANDROID:
            self._extract_tgz(make_path('armv7-android'), self.ext)

        for egg in glob(join(self.defold_root, 'packages', '*.egg')):
            self._log('Installing %s' % basename(egg))
            self.exec_command(['easy_install', '-q', '-d', join(self.ext, 'lib', 'python'), '-N', egg])

        for n in 'waf_dynamo.py waf_content.py'.split():
            self._copy(join(self.defold_root, 'share', n), join(self.dynamo_home, 'lib/python'))

        for n in itertools.chain(*[ glob('share/*%s' % ext) for ext in ['.mobileprovision', '.xcent', '.supp']]):
            self._copy(join(self.defold_root, n), join(self.dynamo_home, 'share'))

    def _git_sha1(self, dir = '.'):
        process = subprocess.Popen('git log --oneline -n1'.split(), stdout = subprocess.PIPE)
        out, err = process.communicate()
        if process.returncode != 0:
            sys.exit(process.returncode)

        line = out.split('\n')[0].strip()
        sha1 = line.split()[0]
        return sha1

    def is_cross_platform(self):
        return self.host != self.target_platform

    def archive_engine(self):
        exe_prefix = ''
        if self.target_platform == 'win32':
            exe_ext = '.exe'
        elif 'android' in self.target_platform:
            exe_prefix = 'lib'
            exe_ext = '.so'
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
        host, path = full_archive_path.split(':', 1)
        self.exec_command(['ssh', host, 'mkdir -p %s' % path])
        dynamo_home = self.dynamo_home
        # TODO: Ugly win fix, make better (https://defold.fogbugz.com/default.asp?1066)
        if self.target_platform == 'win32':
            dynamo_home = dynamo_home.replace("\\", "/")
            dynamo_home = "/" + dynamo_home[:1] + dynamo_home[2:]

        if self.is_cross_platform():
            # When cross compiling or when compiling for 64-bit the engine is located
            # under PREFIX/bin/platform/...

            bin_dir = self.target_platform
            lib_dir = self.target_platform
        else:
            bin_dir = ''
            lib_dir = ''

        engine = join(dynamo_home, 'bin', bin_dir, exe_prefix + 'dmengine' + exe_ext)
        engine_release = join(dynamo_home, 'bin', bin_dir, exe_prefix + 'dmengine_release' + exe_ext)
        engine_headless = join(dynamo_home, 'bin', bin_dir, exe_prefix + 'dmengine_headless' + exe_ext)
        if self.target_platform != 'x86_64-darwin':
            # NOTE: Temporary check as we don't build the entire engine to 64-bit
            self._log('Archiving %s' % engine)
            self.exec_command(['scp', engine,
                               '%s/%sdmengine%s' % (full_archive_path, exe_prefix, exe_ext)])
            self.exec_command(['scp', engine_release,
                               '%s/%sdmengine_release%s' % (full_archive_path, exe_prefix, exe_ext)])
            self.exec_command(['scp', engine_headless,
                               '%s/%sdmengine_headless%s' % (full_archive_path, exe_prefix, exe_ext)])

        if 'android' in self.target_platform:
            files = [
                ('share/java', 'classes.dex'),
                ('bin/%s' % (self.target_platform), 'dmengine.apk'),
                ('bin/%s' % (self.target_platform), 'dmengine_release.apk'),
            ]
            for f in files:
                self._log('Archiving %s' % f[1])
                src = join(dynamo_home, f[0], f[1])
                self.exec_command(['scp', src,
                                   '%s/%s' % (full_archive_path, f[1])])

        libs = ['particle']
        if not self.is_cross_platform() or self.target_platform == 'x86_64-darwin':
            libs.append('texc')
        for lib in libs:
            lib_path = join(dynamo_home, 'lib', lib_dir, '%s%s_shared%s' % (lib_prefix, lib, lib_ext))
            self._log('Archiving %s' % lib_path)
            self.exec_command(['scp', lib_path,
                               '%s/%s%s_shared%s' % (full_archive_path, lib_prefix, lib, lib_ext)])

    def build_engine(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        skip_codesign = '--skip-codesign' if self.skip_codesign else ''
        disable_ccache = '--disable-ccache' if self.disable_ccache else ''

        eclipse = '--eclipse' if self.eclipse else ''

        if self.target_platform.startswith('x86_64'):
            # Only partial support for 64-bit
            libs="dlib ddf particle".split()
        else:
            libs="dlib ddf particle glfw graphics hid input physics resource lua extension script render gameobject gui sound gamesys tools record facebook iap push adtruth engine".split()

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
                pf_arg = ""
                if platform != self.host:
                    pf_arg = "--platform=%s" % (platform)
                cmd = 'python %s/ext/bin/waf --prefix=%s %s %s %s %s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, pf_arg, skip_tests, skip_codesign, disable_ccache, eclipse)
                self.exec_command(cmd.split(), cwd = cwd)

        self._log('Building bob')

        self.exec_command(" ".join([join(self.dynamo_home, 'ext/share/ant/bin/ant'), 'clean', 'install']),
                          cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob'),
                          shell = True)

        for lib in libs:
            self._log('Building %s' % lib)
            cwd = join(self.defold_root, 'engine/%s' % lib)
            cmd = 'python %s/ext/bin/waf --prefix=%s --platform=%s %s %s %s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, self.target_platform, skip_tests, skip_codesign, disable_ccache, eclipse)
            self.exec_command(cmd.split(), cwd = cwd)

    def build_go(self):
        # TODO: shell=True is required only on windows
        # otherwise it fails. WHY?
        if not self.skip_tests:
            self.exec_command('go test defold/...', shell=True)
        self.exec_command('go install defold/...', shell=True)
        for f in glob(join(self.defold, 'go', 'bin', '*')):
            shutil.copy(f, join(self.dynamo_home, 'bin'))

    def archive_go(self):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, 'go', self.target_platform).replace('\\', '/')
        host, path = full_archive_path.split(':', 1)
        self.exec_command(['ssh', host, 'mkdir -p %s' % path])

        for p in glob(join(self.defold, 'go', 'bin', '*')):
            # TODO: Ugly win fix, make better (https://defold.fogbugz.com/default.asp?1066)
            if self.target_platform == 'win32':
                p = p.replace("\\", "/")
                p = "/" + p[:1] + p[2:]
            self.exec_command(['scp', p, '%s/%s' % (full_archive_path, basename(p))])

    def build_bob(self):
        cwd = join(self.defold_root, 'com.dynamo.cr/com.dynamo.cr.bob')
        self.exec_command("./scripts/copy_libtexc.sh",
                          cwd = cwd,
                          shell = True)

        self.exec_command(" ".join([join(self.dynamo_home, 'ext/share/ant/bin/ant'), 'clean', 'install']),
                          cwd = cwd,
                          shell = True)

    def archive_bob(self):
        sha1 = self._git_sha1()

        dynamo_home = self.dynamo_home
        # TODO: Ugly win fix, make better (https://defold.fogbugz.com/default.asp?1066)
        if self.target_platform == 'win32':
            dynamo_home = dynamo_home.replace("\\", "/")
            dynamo_home = "/" + dynamo_home[:1] + dynamo_home[2:]

        full_archive_path = join(self.archive_path, sha1, 'bob').replace('\\', '/')
        host, path = full_archive_path.split(':', 1)
        self.exec_command(['ssh', host, 'mkdir -p %s' % path])

        for p in glob(join(self.dynamo_home, 'share', 'java', 'bob.jar')):
            self.exec_command(['scp', p, '%s/%s' % (full_archive_path, basename(p))])

    def build_docs(self):
        skip_tests = '--skip-tests' if self.skip_tests or self.target_platform != self.host else ''
        self._log('Building docs')
        cwd = join(self.defold_root, 'engine/docs')
        cmd = 'python %s/ext/bin/waf configure --prefix=%s %s distclean configure build install' % (self.dynamo_home, self.dynamo_home, skip_tests)
        self.exec_command(cmd.split(), cwd = cwd)

    def test_cr(self):
        cwd = join(self.defold_root, 'com.dynamo.cr', 'com.dynamo.cr.parent')
        self.exec_command([join(self.dynamo_home, 'ext/share/maven/bin/mvn'), 'clean', 'verify'],
                          cwd = cwd)

    def _get_cr_builddir(self, product):
        return join(os.getcwd(), 'com.dynamo.cr/com.dynamo.cr.%s-product' % product)

    def build_server(self):
        self._build_cr('server')

    def build_editor(self):
        self._build_cr('editor')

    def _archive_cr(self, product, build_dir):
        sha1 = self._git_sha1()
        full_archive_path = join(self.archive_path, sha1, product).replace('\\', '/')
        host, path = full_archive_path.split(':', 1)
        self.exec_command(['ssh', host, 'mkdir -p %s' % path])
        for p in glob(join(build_dir, 'target/products/*.zip')):
            self.exec_command(['scp', p, full_archive_path])
        self.exec_command(['tar', '-C', build_dir, '-cz', '-f', join(build_dir, '%s_repository.tgz' % product), 'target/repository'])
        self.exec_command(['scp', join(build_dir, '%s_repository.tgz' % product), full_archive_path])

        if self.branch:
            _, base_path = self.archive_path.split(':', 1)
            self.exec_command(['ssh', host, 'ln -snf %s %s' % (path, join(base_path, '%s_%s' % (product, self.branch)))])

    def archive_editor(self):
        build_dir = self._get_cr_builddir('editor')
        self._archive_cr('editor', build_dir)

    def archive_server(self):
        build_dir = self._get_cr_builddir('server')
        self._archive_cr('server', build_dir)

    def _build_cr(self, product):
        cwd = join(self.defold_root, 'com.dynamo.cr', 'com.dynamo.cr.parent')
        self.exec_command([join(self.dynamo_home, 'ext/share/maven/bin/mvn'), 'clean', 'package', '-P', product], cwd = cwd)

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

        with open('com.dynamo.cr/com.dynamo.cr.editor-product/cr.product', 'a+') as f:
            f.seek(0)
            product = f.read()

            product = re.sub('(<product name=.*?)version="[0-9\.]+"(.*?>)', '\g<1>version="%s"\g<2>' % new_version, product)
            f.truncate(0)
            f.write(product)

        with open('com.dynamo.cr/com.dynamo.cr.editor.core/src/com/dynamo/cr/editor/core/EditorCorePlugin.java', 'a+') as f:
            f.seek(0)
            activator = f.read()

            activator = re.sub('public static final String VERSION = "[0-9\.]+";', 'public static final String VERSION = "%s";' % new_version, activator)
            activator = re.sub('public static final String VERSION_SHA1 = ".*?";', 'public static final String VERSION_SHA1 = "%s";' % sha1, activator)
            f.truncate(0)
            f.write(activator)

        with open('engine/engine/src/engine_version.h', 'a+') as f:
            f.seek(0)
            engine_version = f.read()

            engine_version = re.sub('const char\* VERSION = "[0-9\.]+";', 'const char* VERSION = "%s";' % new_version, engine_version)
            engine_version = re.sub('const char\* VERSION_SHA1 = ".*?";', 'const char* VERSION_SHA1 = "%s";' % sha1, engine_version)
            f.truncate(0)
            f.write(engine_version)

        print 'Bumping engine version from %s to %s' % (current, new_version)
        print 'Review changes and commit'

    def shell(self):
        print 'Setting up shell with DYNAMOH_HOME, PATH and LD_LIBRARY_PATH/DYLD_LIRARY_PATH (where applicable) set'
        self.exec_command(['sh', '-l'])

    def exec_command(self, arg_list, **kwargs):
        env = dict(os.environ)

        ld_library_path = 'DYLD_LIBRARY_PATH' if self.host == 'darwin' else 'LD_LIBRARY_PATH'
        env[ld_library_path] = os.path.pathsep.join(['%s/lib' % self.dynamo_home,
                                                     '%s/ext/lib/%s' % (self.dynamo_home, self.host)])

        env['PYTHONPATH'] = os.path.pathsep.join(['%s/lib/python' % self.dynamo_home,
                                                  '%s/ext/lib/python' % self.dynamo_home])

        env['DYNAMO_HOME'] = self.dynamo_home

        paths = os.path.pathsep.join(['%s/bin' % self.dynamo_home,
                                      '%s/ext/bin' % self.dynamo_home,
                                      '%s/ext/bin/%s' % (self.dynamo_home, self.host),
                                      '%s/ext/go/bin' % self.dynamo_home])

        env['PATH'] = paths + os.path.pathsep + env['PATH']

        go_paths = os.path.pathsep.join(['%s/go' % self.dynamo_home,
                                         join(self.defold, 'go')])
        env['GOPATH'] = go_paths
        env['GOROOT'] = '%s/go' % self.ext

        env['MAVEN_OPTS'] = '-Xms256m -Xmx700m -XX:MaxPermSize=1024m'

        # Force 32-bit python 2.6 on darwin. We should perhaps switch to 2.7 soon?
        env['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
        env['VERSIONER_PYTHON_VERSION'] = '2.6'

        if self.no_colors:
            env['NOCOLOR'] = '1'
            env['GTEST_COLOR'] = 'no'

        process = subprocess.Popen(arg_list, env = env, **kwargs)
        process.wait()
        if process.returncode != 0:
            sys.exit(process.returncode)

if __name__ == '__main__':
    usage = '''usage: %prog [options] command(s)

Commands:
distclean       - Removes the DYNAMO_HOME folder
install_ext     - Install external packages
build_engine    - Build engine
archive_engine  - Archive engine to path specified with --archive-path
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
shell           - Start development shell

Multiple commands can be specified'''

    parser = optparse.OptionParser(usage)

    parser.add_option('--eclipse-home', dest='eclipse_home',
                      default = None,
                      help = 'Eclipse directory')

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      choices = ['linux', 'darwin', 'x86_64-darwin', 'win32', 'armv7-darwin', 'armv7-android'],
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

    parser.add_option('--archive-path', dest='archive_path',
                      default = None,
                      help = 'Archive build. Set ssh-path, host:path, to archive build to')

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

    options, args = parser.parse_args()

    if len(args) == 0:
        parser.error('No command specified')

    target_platform = options.target_platform if options.target_platform else get_host_platform()
    if target_platform == 'darwin':
        # NOTE: Darwin is currently mixed 32 and 64-bit.
        # That's why we have this temporary hack
        # NOTE: Build 64-bit first as 32-bit is the current default
        c = Configuration(dynamo_home = os.environ.get('DYNAMO_HOME', None),
                          target_platform = 'x86_64-%s' % target_platform,
                          eclipse_home = options.eclipse_home,
                          skip_tests = options.skip_tests,
                          skip_codesign = options.skip_codesign,
                          disable_ccache = options.disable_ccache,
                          no_colors = options.no_colors,
                          archive_path = options.archive_path,
                          set_version = options.set_version,
                          eclipse = options.eclipse,
                          branch = options.branch)


        for cmd in args:
            if cmd in ['distclean', 'install_ext', 'build_engine', 'archive_engine']:
                f = getattr(c, cmd, None)
                if not f:
                    parser.error('Unknown command %s' % cmd)
                f()

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
                      branch = options.branch)

    for cmd in args:
        f = getattr(c, cmd, None)
        if not f:
            parser.error('Unknown command %s' % cmd)
        f()
