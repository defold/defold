#!/usr/bin/env python

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

# If you update java version, don't forget to update it here too:
# - /editor/bundle-resources/config at "launcher.jdk" key
# - /scripts/build.py smoke_test, `java` variable
# - /editor/src/clj/editor/updater.clj, `protected-dirs` let binding
java_version = '11.0.1'

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

def mac_certificate():
    # This certificate must be installed on the computer performing the operation
    certificate = 'Developer ID Application: Midasplayer Technology AB (ATT58V7T33)'
    if exec_command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(certificate) >= 0:
        return certificate
    else:
        return None

def sign_files(bundle_dir):
    certificate = mac_certificate()
    if certificate == None:
        print("Warning: Codesigning certificate not found, files will not be signed")
    else:
        exec_command([
            'codesign',
            '--deep',
            '--force',
            '--options', 'runtime',
            '--entitlements', './scripts/entitlements.plist'
            '-s', certificate,
            bundle_dir])

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

        exe_suffix = ''
        if 'win32' in platform:
            exe_suffix = '.exe'

        launcher = launcher_path(options, platform, exe_suffix)

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
        if icon:
            shutil.copy('bundle-resources/%s' % icon, resources_dir)

        # creating editor config file
        config = ConfigParser.ConfigParser()
        config.read('bundle-resources/config')
        config.set('build', 'editor_sha1', options.editor_sha1)
        config.set('build', 'engine_sha1', options.engine_sha1)
        config.set('build', 'version', options.version)
        config.set('build', 'time', datetime.datetime.now().isoformat())

        if options.channel:
            config.set('build', 'channel', options.channel)

        with open('%s/config' % resources_dir, 'wb') as f:
            config.write(f)

        shutil.copy(jar_file, '%s/defold-%s.jar' % (packages_dir, options.editor_sha1))
        shutil.copy(launcher, '%s/Defold%s' % (exe_dir, exe_suffix))
        if not 'win32' in platform:
            exec_command(['chmod', '+x', '%s/Defold%s' % (exe_dir, exe_suffix)])

        extract(jdk, 'tmp', is_mac)

        if is_mac:
            platform_jdk = 'tmp/jdk-%s.jdk/Contents/Home' % java_version
        else:
            platform_jdk = 'tmp/jdk-%s' % java_version

        exec_command(['%s/bin/jlink' % build_jdk,
                      '@jlink-options',
                      '--module-path=%s/jmods' % platform_jdk,
                      '--output=%s/jdk%s' % (packages_dir, java_version)])

        # create final zip file
        zipfile = 'target/editor/Defold-%s.zip' % platform
        if os.path.exists(zipfile):
            os.remove(zipfile)

        print("Creating '%s' bundle from '%s'" % (zipfile, bundle_dir))
        ziptree(bundle_dir, zipfile, 'tmp')


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
    zip = zipfile.ZipFile(bundle_file)
    zip.extractall(dmg_dir)

    # create additional files for the .dmg
    shutil.copy('bundle-resources/dmg_ds_store', '%s/.DS_Store' % dmg_dir)
    shutil.copytree('bundle-resources/dmg_background', '%s/.background' % dmg_dir)
    exec_command(['ln', '-sf', '/Applications', '%s/Applications' % dmg_dir])

    # sign files
    app = os.path.join(dmg_dir, "Defold.app")
    sign_files(app)

    # create dmg
    dmg_file = os.path.join(options.bundle_dir, "Defold-x86_64-darwin.dmg")
    if os.path.exists(dmg_file):
        os.remove(dmg_file)
    exec_command(['hdiutil', 'create', '-fs', 'JHFS+', '-volname', 'Defold', '-srcfolder', dmg_dir, dmg_file])

    # sign the dmg
    certificate = mac_certificate()
    if certificate == None:
        print("Warning: Codesigning certificate not found, DMG will not be signed")
    else:
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
  bundle                Create editor bundle form built files
  installer             Create editor installer from bundle'''

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

    parser.add_option('--bundle-dir', dest='bundle_dir',
                      default = "target/editor",
                      help = 'Path to directory containing editor bundles')

    options, commands = parser.parse_args()

    if (("bundle" in commands) or ("installer" in commands)) and not options.target_platform:
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
        elif command == "bundle":
            create_bundle(options)
        elif command == "installer":
            create_installer(options)
