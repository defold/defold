#!/usr/bin/env python
# Copyright 2020 The Defold Foundation
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



import os
import os.path as path
import stat
import sys
import optparse
import re
import shutil
import subprocess
import tarfile
import zipfile
import ConfigParser
import datetime
import imp
import fnmatch

# If you update java version, don't forget to update it here too:
# - /editor/bundle-resources/config at "launcher.jdk" key
# - /scripts/build.py smoke_test, `java` variable
# - /editor/src/clj/editor/updater.clj, `protected-dirs` let binding
java_version = '11.0.1'
java_version_patch = 'p1'


platform_to_java = {'x86_64-linux': 'linux-x64',
                    'x86_64-darwin': 'osx-x64',
                    'x86_64-win32': 'windows-x64'}

python_platform_to_java = {'linux2': 'linux-x64',
                           'win32': 'windows-x64',
                           'darwin': 'osx-x64'}

def log(msg):
    print(msg)
    sys.stdout.flush()
    sys.stderr.flush()

def mkdirs(path):
    if not os.path.exists(path):
        os.makedirs(path)

def extract(file, path, is_mac):
    print('Extracting %s to %s' % (file, path))

    if is_mac:
        # We use the system tar command for macOSX because tar (and
        # others) on macOS has special handling of ._ files that
        # contain additional HFS+ attributes. See for example:
        # http://superuser.com/questions/61185/why-do-i-get-files-like-foo-in-my-tarball-on-os-x
        exec_command(['tar', '-C', path, '-xzf', file])
    else:
        tf = tarfile.TarFile.open(file, 'r:gz')
        tf.extractall(path)
        tf.close()

modules = {}

def download(url):
    if not modules.has_key('http_cache'):
        modules['http_cache'] = imp.load_source('http_cache', os.path.join('..', 'build_tools', 'http_cache.py'))
    log('Downloading %s' % (url))
    path = modules['http_cache'].download(url, lambda count, total: log('Downloading %s %.2f%%' % (url, 100 * count / float(total))))
    if not path:
        log('Downloading %s failed' % (url))
    return path

def exec_command(args):
    print('[EXEC] %s' % args)
    process = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, shell = False)

    output = ''
    while True:
        line = process.stdout.readline()
        if line != '':
            output += line
            print(line.rstrip())
            sys.stdout.flush()
            sys.stderr.flush()
        else:
            break

    if process.wait() != 0:
        sys.exit(process.returncode)

    return output

def ziptree(path, outfile, directory = None):
    # Directory is similar to -C in tar

    zip = zipfile.ZipFile(outfile, 'w')
    for root, dirs, files in os.walk(path):
        for f in files:
            p = os.path.join(root, f)
            an = p
            if directory:
                an = os.path.relpath(p, directory)
            zip.write(p, an)

    zip.close()
    return outfile

def git_sha1_from_version_file():
    """ Gets the version number and checks if that tag exists """
    with open('../VERSION', 'r') as version_file:
        version = version_file.read().strip()

    process = subprocess.Popen(['git', 'rev-list', '-n', '1', version], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        return None
    return out.strip()

def git_sha1(ref = 'HEAD'):
    process = subprocess.Popen(['git', 'rev-parse', ref], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        sys.exit("Unable to find git sha from ref: %s" % (ref))
    return out.strip()

def remove_readonly_retry(function, path, excinfo):
    try:
        os.chmod(path, stat.S_IWRITE)
        function(path)
    except Exception as e:
        print("Failed to remove %s, error %s" % (path, e))

def rmtree(path):
    if os.path.exists(path):
        shutil.rmtree(path, onerror=remove_readonly_retry)

def mac_certificate(codesigning_identity):
    if exec_command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(codesigning_identity) >= 0:
        return codesigning_identity
    else:
        return None

def sign_files(platform, options, dir):
    if options.skip_codesign:
        return
    if 'win32' in platform:
        certificate = options.windows_cert
        certificate_pass_path = options.windows_cert_pass
        if certificate == None:
            print("No codesigning certificate specified")
            sys.exit(1)

        if not os.path.exists(certificate):
            print("Certificate file does not exist:", certificate)
            sys.exit(1)

        certificate_pass = 'invalid'
        with open(certificate_pass_path, 'rb') as f:
            certificate_pass = f.read()

        signtool = os.path.join(os.environ['DYNAMO_HOME'], 'ext','SDKs','Win32','WindowsKits','10','bin','10.0.18362.0','x64','signtool.exe')
        if not os.path.exists(signtool):
            print("signtool.exe file does not exist:", signtool)
            sys.exit(1)
        exec_command([
            signtool,
            'sign',
            '/fd', 'sha256',
            '/a',
            '/f', certificate,
            '/p', certificate_pass,
            '/tr', 'http://timestamp.digicert.com',
            dir])
    elif 'darwin' in platform:
        codesigning_identity = options.codesigning_identity
        certificate = mac_certificate(codesigning_identity)
        if certificate == None:
            print("Codesigning certificate not found for signing identity %s" % (codesigning_identity))
            sys.exit(1)

        exec_command([
            'codesign',
            '--deep',
            '--force',
            '--options', 'runtime',
            '--entitlements', './scripts/entitlements.plist',
            '-s', certificate,
            dir])

def launcher_path(options, platform, exe_suffix):
    if options.launcher:
        return options.launcher
    elif options.engine_sha1:
        launcher_version = options.engine_sha1
        launcher_url = 'https://d.defold.com/archive/%s/engine/%s/launcher%s' % (launcher_version, platform, exe_suffix)
        launcher = download(launcher_url)
        if not launcher:
            print('Failed to download launcher', launcher_url)
            sys.exit(5)
        return launcher
    else:
        return path.join(os.environ['DYNAMO_HOME'], "bin", platform, "launcher%s" % exe_suffix)


def full_jdk_url(jdk_platform):
    return 'https://s3-eu-west-1.amazonaws.com/defold-packages/openjdk-%s_%s_bin.tar.gz' % (java_version, jdk_platform)


def download_build_jdk():
    print('Downloading build jdk')
    rmtree('build/jdk')
    is_mac = sys.platform == 'darwin'
    jdk_url = full_jdk_url(python_platform_to_java[sys.platform])
    jdk = download(jdk_url)
    if not jdk:
        print('Failed to download build jdk %s' % jdk_url)
        sys.exit(5)
    mkdirs('build/jdk')
    extract(jdk, 'build/jdk', is_mac)
    if is_mac:
        return 'build/jdk/jdk-%s.jdk/Contents/Home' % java_version
    else:
        return 'build/jdk/jdk-%s' % java_version



def check_reflections(java_cmd_env):
    reflection_prefix = 'Reflection warning, ' # final space important
    included_reflections = ['editor/', 'util/'] # [] = include all
    ignored_reflections = []

    # lein check puts reflection warnings on stderr, redirect to stdout to capture all output
    output = exec_command(['env', java_cmd_env, 'bash', './scripts/lein', 'with-profile', '+headless', 'check-and-exit'])
    lines = output.splitlines()
    reflection_lines = (line for line in lines if re.match(reflection_prefix, line))
    reflections = (re.match('(' + reflection_prefix + ')(.*)', line).group(2) for line in reflection_lines)
    filtered_reflections = reflections if not included_reflections else (line for line in reflections if any((re.match(include, line) for include in included_reflections)))
    failures = list(line for line in filtered_reflections if not any((re.match(ignored, line) for ignored in ignored_reflections)))

    if failures:
        for failure in failures:
            print(failure)
        exit(1)


def build(options):
    build_jdk = download_build_jdk()
    java_cmd_env = 'JAVA_CMD=%s/bin/java' % build_jdk

    print('Building editor')

    init_command = ['env', java_cmd_env, 'bash', './scripts/lein', 'with-profile', '+release', 'init']
    if options.engine_sha1:
        init_command += [options.engine_sha1]

    exec_command(init_command)

    build_ns_batches_command = ['env', java_cmd_env, 'bash', './scripts/lein', 'with-profile', '+release', 'build-ns-batches']
    exec_command(build_ns_batches_command)

    check_reflections(java_cmd_env)

    if options.skip_tests:
        print('Skipping tests')
    else:
        exec_command(['env', java_cmd_env, 'bash', './scripts/lein', 'test'])

    exec_command(['env', java_cmd_env, 'bash', './scripts/lein', 'with-profile', '+release', 'uberjar'])


def get_exe_suffix(platform):
    return ".exe" if 'win32' in platform else ""


def create_bundle(options):
    jar_file = 'target/defold-editor-2.0.0-SNAPSHOT-standalone.jar'
    build_jdk = download_build_jdk()

    mkdirs('target/editor')
    for platform in options.target_platform:
        print("Creating bundle for platform %s" % platform)
        rmtree('tmp')

        jdk_url = full_jdk_url(platform_to_java[platform])
        jdk = download(jdk_url)
        if not jdk:
            print('Failed to download %s' % jdk_url)
            sys.exit(5)

        tmp_dir = "tmp"

        if 'darwin' in platform:
            resources_dir = os.path.join(tmp_dir, 'Defold.app/Contents/Resources')
            packages_dir = os.path.join(tmp_dir, 'Defold.app/Contents/Resources/packages')
            bundle_dir = os.path.join(tmp_dir, 'Defold.app')
            exe_dir = os.path.join(tmp_dir, 'Defold.app/Contents/MacOS')
            icon = 'logo.icns'
            is_mac = True
        else:
            resources_dir = os.path.join(tmp_dir, 'Defold')
            packages_dir = os.path.join(tmp_dir, 'Defold/packages')
            bundle_dir = os.path.join(tmp_dir, 'Defold')
            exe_dir = os.path.join(tmp_dir, 'Defold')
            icon = None
            is_mac = False

        mkdirs(tmp_dir)
        mkdirs(bundle_dir)
        mkdirs(exe_dir)
        mkdirs(resources_dir)
        mkdirs(packages_dir)

        if is_mac:
            shutil.copy('bundle-resources/Info.plist', '%s/Contents' % bundle_dir)
            shutil.copy('bundle-resources/Assets.car', resources_dir)
            shutil.copy('bundle-resources/document_legacy.icns', resources_dir)
        if icon:
            shutil.copy('bundle-resources/%s' % icon, resources_dir)

        # creating editor config file
        config = ConfigParser.ConfigParser()
        config.read('bundle-resources/config')
        config.set('build', 'editor_sha1', options.editor_sha1)
        config.set('build', 'engine_sha1', options.engine_sha1)
        config.set('build', 'version', options.version)
        config.set('build', 'time', datetime.datetime.now().isoformat())
        config.set('build', 'archive_domain', "d.defold.com")

        if options.channel:
            config.set('build', 'channel', options.channel)

        with open('%s/config' % resources_dir, 'wb') as f:
            config.write(f)

        shutil.copy(jar_file, '%s/defold-%s.jar' % (packages_dir, options.editor_sha1))

        # copy editor executable (the launcher)
        launcher = launcher_path(options, platform, get_exe_suffix(platform))
        defold_exe = '%s/Defold%s' % (exe_dir, get_exe_suffix(platform))
        shutil.copy(launcher, defold_exe)
        if not 'win32' in platform:
            exec_command(['chmod', '+x', defold_exe])

        extract(jdk, tmp_dir, is_mac)

        if is_mac:
            platform_jdk = '%s/jdk-%s.jdk/Contents/Home' % (tmp_dir, java_version)
        else:
            platform_jdk = '%s/jdk-%s' % (tmp_dir, java_version)

        # use jlink to generate a custom Java runtime to bundle with the editor
        packages_jdk = '%s/jdk%s-%s' % (packages_dir, java_version, java_version_patch)
        exec_command(['%s/bin/jlink' % build_jdk,
                      '@jlink-options',
                      '--module-path=%s/jmods' % platform_jdk,
                      '--output=%s' % packages_jdk])

        # create final zip file
        zipfile = 'target/editor/Defold-%s.zip' % platform
        if os.path.exists(zipfile):
            os.remove(zipfile)

        print("Creating '%s' bundle from '%s'" % (zipfile, bundle_dir))
        ziptree(bundle_dir, zipfile, tmp_dir)


def sign(options):
    for platform in options.target_platform:
        # check that we have an editor bundle to sign
        if 'win32' in platform:
            bundle_file = os.path.join(options.bundle_dir, "Defold-x86_64-win32.zip")
        elif 'darwin' in platform:
            bundle_file = os.path.join(options.bundle_dir, "Defold-x86_64-darwin.zip")
        else:
            print("No signing support for platform %s" % platform)
            continue

        if not os.path.exists(bundle_file):
            print('Editor bundle %s does not exist' % bundle_file)
            exec_command(['ls', '-la', options.bundle_dir])
            sys.exit(1)

        # setup
        sign_dir = os.path.join("build", "sign")
        rmtree(sign_dir)
        mkdirs(sign_dir)

        # unzip
        exec_command(['unzip', bundle_file, '-d', sign_dir])

        # sign files
        if 'darwin' in platform:
            # we need to sign the binaries in Resources folder manually as codesign of
            # the *.app will not process files in Resources
            jdk_dir = "jdk%s-%s" % (java_version, java_version_patch)
            jdk_path = os.path.join(sign_dir, "Defold.app", "Contents", "Resources", "packages", jdk_dir)
            for exe in find_files(os.path.join(jdk_path, "bin"), "*"):
                sign_files('darwin', options, exe)
            for lib in find_files(os.path.join(jdk_path, "lib"), "*.dylib"):
                sign_files('darwin', options, lib)
            sign_files('darwin', options, os.path.join(jdk_path, "lib", "jspawnhelper"))
            sign_files('darwin', options, os.path.join(sign_dir, "Defold.app"))
        elif 'win32' in platform:
            sign_files('win32', options, os.path.join(sign_dir, "Defold", "Defold.exe"))

        # create editor bundle with signed files
        os.remove(bundle_file)
        ziptree(sign_dir, bundle_file, sign_dir)
        rmtree(sign_dir)



def find_files(root_dir, file_pattern):
    matches = []
    for root, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            fullname = os.path.join(root, filename)
            if fnmatch.fnmatch(filename, file_pattern):
                matches.append(fullname)
    return matches

def create_dmg(options):
    print("Creating .dmg from file in '%s'" % options.bundle_dir)

    # check that we have an editor bundle to create a dmg from
    bundle_file = os.path.join(options.bundle_dir, "Defold-x86_64-darwin.zip")
    if not os.path.exists(bundle_file):
        print('Editor bundle %s does not exist' % bundle_file)
        exec_command(['ls', '-la', options.bundle_dir])
        sys.exit(1)

    # setup
    dmg_dir = os.path.join("build", "dmg")
    rmtree(dmg_dir)
    mkdirs(dmg_dir)

    # unzip (.zip -> .app)
    exec_command(['unzip', bundle_file, '-d', dmg_dir])

    # create additional files for the .dmg
    shutil.copy('bundle-resources/dmg_ds_store', '%s/.DS_Store' % dmg_dir)
    shutil.copytree('bundle-resources/dmg_background', '%s/.background' % dmg_dir)
    exec_command(['ln', '-sf', '/Applications', '%s/Applications' % dmg_dir])

    # create dmg
    dmg_file = os.path.join(options.bundle_dir, "Defold-x86_64-darwin.dmg")
    if os.path.exists(dmg_file):
        os.remove(dmg_file)
    exec_command(['hdiutil', 'create', '-fs', 'JHFS+', '-volname', 'Defold', '-srcfolder', dmg_dir, dmg_file])

    # sign the dmg
    if not options.skip_codesign:
        certificate = mac_certificate(options.codesigning_identity)
        if certificate == None:
            error("Codesigning certificate not found for signing identity %s" % (options.codesigning_identity))
            sys.exit(1)

        exec_command(['codesign', '-s', certificate, dmg_file])


def create_installer(options):
    print("Creating installers")
    for platform in options.target_platform:
        if platform == "x86_64-darwin":
            print("Creating installer for platform %s" % platform)
            create_dmg(options)
        else:
            print("Ignoring platform %s" % platform)


if __name__ == '__main__':
    usage = '''%prog [options] command(s)

Commands:
  build                 Build editor
  bundle                Create editor bundle (zip) from built files
  sign                  Sign editor bundle (zip)
  installer             Create editor installer from bundle (zip)'''

    parser = optparse.OptionParser(usage)

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      action = 'append',
                      choices = ['x86_64-linux', 'x86_64-darwin', 'x86_64-win32'],
                      help = 'Target platform to create editor bundle for. Specify multiple times for multiple platforms')

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version to set in editor config when creating the bundle')

    parser.add_option('--channel', dest='channel',
                      default = None,
                      help = 'Channel to set in editor config when creating the bundle')

    parser.add_option('--engine-artifacts', dest='engine_artifacts',
                      default = 'auto',
                      help = "Which engine artifacts to use when building. Can be 'auto', 'dynamo-home', 'archived', 'archived-stable' or a sha1.")

    parser.add_option('--launcher', dest='launcher',
                      default = None,
                      help = 'Specific local launcher to use when creating the bundle. Useful when testing.')

    parser.add_option('--skip-tests', dest='skip_tests',
                      action = 'store_true',
                      default = False,
                      help = 'Skip tests when building')

    parser.add_option('--skip-codesign', dest='skip_codesign',
                      action = 'store_true',
                      default = False,
                      help = 'Skip code signing when bundling')

    parser.add_option('--codesigning-identity', dest='codesigning_identity',
                      default = 'Developer ID Application: Stiftelsen Defold Foundation (26PW6SVA7H)',
                      help = 'Codesigning identity for macOS')

    parser.add_option('--windows-cert', dest='windows_cert',
                      default = None,
                      help = 'Path to Windows certificate (pfx)')

    parser.add_option('--windows-cert-pass', dest='windows_cert_pass',
                      default = None,
                      help = 'Windows certificate password')

    parser.add_option('--bundle-dir', dest='bundle_dir',
                      default = "target/editor",
                      help = 'Path to directory containing editor bundles')

    options, commands = parser.parse_args()

    if (("bundle" in commands) or ("installer" in commands) or ("sign" in commands)) and not options.target_platform:
        parser.error('No platform specified')

    if "bundle" in commands and not options.version:
        parser.error('No version specified')

    options.editor_sha1 = git_sha1('HEAD')

    if options.engine_artifacts == 'auto':
        # If the VERSION file contains a version for which a tag
        # exists, then we're on a branch that uses a stable engine
        # (ie. editor-dev or branch based on editor-dev), so use that.
        # Otherwise use archived artifacts for HEAD.
        options.engine_sha1 = git_sha1_from_version_file() or git_sha1('HEAD')
    elif options.engine_artifacts == 'dynamo-home':
        options.engine_sha1 = None
    elif options.engine_artifacts == 'archived':
        options.engine_sha1 = git_sha1('HEAD')
    elif options.engine_artifacts == 'archived-stable':
        options.engine_sha1 = git_sha1_from_version_file()
        if not options.engine_sha1:
            sys.exit("Unable to find git sha from VERSION file")
    else:
        options.engine_sha1 = options.engine_artifacts

    print('Resolved engine_artifacts=%s to sha1=%s' % (options.engine_artifacts, options.engine_sha1))

    for command in commands:
        if command == "build":
            build(options)
        elif command == "sign":
            sign(options)
        elif command == "bundle":
            create_bundle(options)
        elif command == "installer":
            create_installer(options)
