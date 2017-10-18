#!/usr/bin/env python

import os
import os.path as path
import stat
import glob
import sys
import json
import tempfile
import urllib
import optparse
import urlparse
import re
import shutil
import subprocess
import tarfile
import zipfile
import ConfigParser
import datetime
import imp

platform_to_java = {'x86_64-linux': 'linux-x64',
                    'x86-linux': 'linux-i586',
                    'x86_64-darwin': 'macosx-x64',
                    'x86-win32': 'windows-i586',
                    'x86_64-win32': 'windows-x64'}

platform_to_legacy = {'x86_64-linux': 'x86_64-linux',
                      'x86-linux': 'linux',
                      'x86_64-darwin': 'x86_64-darwin',
                      'x86-win32': 'win32',
                      'x86_64-win32': 'x86_64-win32'}

def log(msg):
    print msg
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

def exec_command(args, stdout = None, stderr = None):
    print('[EXEC] %s' % args)
    process = subprocess.Popen(args, stdout=stdout, stderr=stderr, shell = False)
    output = process.communicate()[0]
    if process.returncode != 0:
        print(output)
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
    with open('../VERSION', 'r') as version_file:
        version = version_file.read().strip()

    process = subprocess.Popen(['git', 'rev-list', '-n', '1', version], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        sys.exit("Unable to find git sha from VERSION file: %s" % (version))
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

def create_dmg(dmg_dir, bundle_dir, dmg_file):
    # This certificate must be installed on the computer performing the operation
    certificate = 'Developer ID Application: Midasplayer Technology AB (ATT58V7T33)'

    certificate_found = exec_command(['security', 'find-identity', '-p', 'codesigning', '-v'], stdout = subprocess.PIPE).find(certificate) >= 0

    if not certificate_found:
        print("Warning: Codesigning certificate not found, DMG will not be signed")

    # sign files in bundle
    if certificate_found:
        exec_command(['codesign', '--deep', '-s', certificate, bundle_dir])

    # create dmg
    exec_command(['hdiutil', 'create', '-volname', 'Defold', '-srcfolder', dmg_dir, dmg_file])

    # sign dmg
    if certificate_found:
        exec_command(['codesign', '-s', certificate, dmg_file])

def launcher_path(options, platform, exe_suffix):
    if options.launcher:
        return options.launcher
    elif options.engine_sha1:
        launcher_version = options.engine_sha1
        launcher_url = 'https://d.defold.com/archive/%s/engine/%s/launcher%s' % (launcher_version, platform_to_legacy[platform], exe_suffix)
        launcher = download(launcher_url)
        if not launcher:
            print 'Failed to download launcher', launcher_url
            sys.exit(5)
        return launcher
    else:
        return path.join(os.environ['DYNAMO_HOME'], "bin", platform_to_legacy[platform], "launcher%s" % exe_suffix)

def bundle(platform, jar_file, options):
    rmtree('tmp')

    jre_minor = 102
    ext = 'tar.gz'
    jre_url = 'https://s3-eu-west-1.amazonaws.com/defold-packages/jre-8u%d-%s.%s' % (jre_minor, platform_to_java[platform], ext)
    jre = download(jre_url)
    if not jre:
        print('Failed to download %s' % jre_url)
        sys.exit(5)

    exe_suffix = ''
    if 'win32' in platform:
        exe_suffix = '.exe'

    launcher = launcher_path(options, platform, exe_suffix)

    mkdirs('tmp')

    if 'darwin' in platform:
        dmg_dir = 'tmp/dmg'
        resources_dir = 'tmp/dmg/Defold.app/Contents/Resources'
        packages_dir = 'tmp/dmg/Defold.app/Contents/Resources/packages'
        bundle_dir = 'tmp/dmg/Defold.app'
        exe_dir = 'tmp/dmg/Defold.app/Contents/MacOS'
        icon = 'logo.icns'
        is_mac = True
    else:
        resources_dir = 'tmp/Defold'
        packages_dir = 'tmp/Defold/packages'
        bundle_dir = 'tmp/Defold'
        exe_dir = 'tmp/Defold'
        icon = None
        is_mac = False

    mkdirs(exe_dir)
    mkdirs(resources_dir)
    mkdirs(packages_dir)
    mkdirs('%s/jre' % packages_dir)

    if is_mac:
        shutil.copy('bundle-resources/Info.plist', '%s/Contents' % bundle_dir)
        shutil.copy('bundle-resources/dmg_ds_store', '%s/.DS_Store' % dmg_dir)
        shutil.copytree('bundle-resources/dmg_background', '%s/.background' % dmg_dir)
        exec_command(['ln', '-sf', '/Applications', '%s/Applications' % dmg_dir])
    if icon:
        shutil.copy('bundle-resources/%s' % icon, resources_dir)

    config = ConfigParser.ConfigParser()
    config.read('bundle-resources/config')
    config.set('build', 'editor_sha1', options.editor_sha1)
    config.set('build', 'engine_sha1', options.engine_sha1)
    config.set('build', 'version', options.version)
    config.set('build', 'time', datetime.datetime.now().isoformat())

    with open('%s/config' % resources_dir, 'wb') as f:
        config.write(f)

    with open('target/editor/update/config', 'wb') as f:
        config.write(f)

    shutil.copy('target/editor/update/%s' % jar_file, '%s/%s' % (packages_dir, jar_file))
    shutil.copy(launcher, '%s/Defold%s' % (exe_dir, exe_suffix))
    shutil.copy(launcher, 'target/editor/launcher-%s%s' % (platform, exe_suffix))
    if not 'win32' in platform:
        exec_command(['chmod', '+x', '%s/Defold%s' % (exe_dir, exe_suffix)])

    if 'win32' in platform:
        exec_command(['java', '-cp', 'target/classes', 'com.defold.util.IconExe', '%s/Defold%s' % (exe_dir, exe_suffix), 'bundle-resources/logo.ico'])

    extract(jre, 'tmp', is_mac)

    print 'Creating bundle'
    if is_mac:
        jre_glob = 'tmp/jre1.8.0_%s.jre/Contents/Home/*' % (jre_minor)
    else:
        jre_glob = 'tmp/jre1.8.0_%s/*' % (jre_minor)

    for p in glob.glob(jre_glob):
        shutil.move(p, '%s/jre' % packages_dir)

    if is_mac:
        ziptree(bundle_dir, 'target/editor/Defold-%s.zip' % platform, dmg_dir)
    else:
        ziptree(bundle_dir, 'target/editor/Defold-%s.zip' % platform, 'tmp')

    if is_mac:
        create_dmg(dmg_dir, bundle_dir, 'target/editor/Defold-%s.dmg' % platform)

def check_reflections():
    reflection_prefix = 'Reflection warning, ' # final space important
    included_reflections = ['editor/'] # [] = include all
    ignored_reflections = []

    # lein check puts reflection warnings on stderr, redirect to stdout to capture all output
    output = exec_command(['./scripts/lein', 'check'], stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    lines = output.splitlines()
    reflection_lines = (line for line in lines if re.match(reflection_prefix, line))
    reflections = (re.match('(' + reflection_prefix + ')(.*)', line).group(2) for line in reflection_lines)
    filtered_reflections = reflections if not included_reflections else (line for line in reflections if any((re.match(include, line) for include in included_reflections)))
    failures = list(line for line in filtered_reflections if not any((re.match(ignored, line) for ignored in ignored_reflections)))

    if failures:
        for failure in failures:
            print(failure)
        exit(1)

if __name__ == '__main__':
    usage = '''usage: %prog [options] command(s)'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      action = 'append',
                      choices = ['x86_64-linux', 'x86-linux', 'x86_64-darwin', 'x86-win32', 'x86_64-win32'],
                      help = 'Target platform. Specify multiple times for multiple platforms')

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version')

    parser.add_option('--engine-artifacts', dest='engine_artifacts',
                      default = 'archived',
                      help = "Which engine artifacts to use, can be 'dynamo-home', 'archived', 'archived-stable' or a sha1.")

    parser.add_option('--launcher', dest='launcher',
                      default = None,
                      help = 'Specific local launcher. Useful when testing bundling.')

    options, all_args = parser.parse_args()

    if not options.target_platform:
        parser.error('No platform specified')

    if not options.version:
        parser.error('No version specified')

    options.editor_sha1 = git_sha1('HEAD')

    if options.engine_artifacts == 'dynamo-home':
        options.engine_sha1 = None
    elif options.engine_artifacts == 'archived':
        options.engine_sha1 = git_sha1('HEAD')
    elif options.engine_artifacts == 'archived-stable':
        options.engine_sha1 = git_sha1_from_version_file()
    else:
        options.engine_sha1 = options.engine_artifacts

    print 'Resolved engine_artifacts=%s to sha1=%s' % (options.engine_artifacts, options.engine_sha1)

    rmtree('target/editor')

    print 'Building editor'

    init_command = ['bash', './scripts/lein', 'with-profile', '+release', 'init']
    if options.engine_sha1:
        init_command += [options.engine_sha1]

    exec_command(init_command)
    check_reflections()
    exec_command(['./scripts/lein', 'test'])
    exec_command(['bash', './scripts/lein', 'with-profile', '+release', 'uberjar'])

    jar_file = 'defold-%s.jar' % options.editor_sha1

    mkdirs('target/editor/update')
    shutil.copy('target/defold-editor-2.0.0-SNAPSHOT-standalone.jar', 'target/editor/update/%s' % jar_file)

    for platform in options.target_platform:
        bundle(platform, jar_file, options)

    package_info = {'version' : options.version,
                    'sha1' : options.editor_sha1,
                    'packages': [{'url': jar_file,
                                  'platform': '*',
                                  'action': 'copy'}]}
    with open('target/editor/update/manifest.json', 'w') as f:
        f.write(json.dumps(package_info, indent=4))
