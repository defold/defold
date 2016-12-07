#!/usr/bin/env python

import os
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

def mkdirs(path):
    if not os.path.exists(path):
        os.makedirs(path)

def extract(file, path):
    print('Extracting %s to %s' % (file, path))
    tf = tarfile.TarFile.open(file, 'r:gz')
    tf.extractall(path)
    tf.close()

def download(url, use_cache = True):
    name = os.path.basename(urlparse.urlparse(url).path)
    if use_cache:
        path = os.path.expanduser('~/.dcache/%s' % name)
        if os.path.exists(path):
            return path
    else:
        t = tempfile.mkdtemp()
        path = '%s/%s' % (t, name)

    if not os.path.exists(os.path.dirname(path)):
        os.makedirs(os.path.dirname(path), 0755)

    tmp = path + '_tmp'
    with open(tmp, 'wb') as f:
        print('Downloading %s %d%%' % (name, 0))
        x = urllib.urlopen(url)
        if x.code != 200:
            return None
        file_len = int(x.headers.get('Content-Length', 0))
        buf = x.read(1024 * 1024)
        n = 0
        while buf:
            n += len(buf)
            print('Downloading %s %d%%' % (name, 100 * n / file_len))
            f.write(buf)
            buf = x.read(1024 * 1024)

    if os.path.exists(path): os.unlink(path)
    os.rename(tmp, path)
    return path

def exec_command(args):
    print('[EXEC] %s' % args)
    process = subprocess.Popen(args, shell = False)
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

def git_sha1(ref = 'HEAD'):
    process = subprocess.Popen(['git', 'rev-parse', ref], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        sys.exit(process.returncode)
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

    if options.launcher:
        launcher = options.launcher
    else:
        launcher_version = options.git_sha1
        launcher_url = 'http://d.defold.com/archive/%s/engine/%s/launcher%s' % (launcher_version, platform_to_legacy[platform], exe_suffix)
        launcher = download(launcher_url, use_cache = False)
        if not launcher:
            print 'Failed to download launcher', launcher_url
            sys.exit(5)

    mkdirs('tmp')

    if 'darwin' in platform:
        resources_dir = 'tmp/Defold.app/Contents/Resources'
        packages_dir = 'tmp/Defold.app/Contents/Resources/packages'
        bundle_dir = 'tmp/Defold.app'
        exe_dir = 'tmp/Defold.app/Contents/MacOS'
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
    if icon:
        shutil.copy('bundle-resources/%s' % icon, resources_dir)
    config = ConfigParser.ConfigParser()
    config.read('bundle-resources/config')
    config.set('build', 'sha1', options.git_sha1)
    config.set('build', 'version', options.version)

    with open('%s/config' % resources_dir, 'wb') as f:
        config.write(f)

    with open('target/editor/update/config', 'wb') as f:
        config.write(f)

    shutil.copy('target/editor/update/%s' % jar_file, '%s/%s' % (packages_dir, jar_file))
    shutil.copy(launcher, '%s/Defold%s' % (exe_dir, exe_suffix))
    if not 'win32' in platform:
        exec_command(['chmod', '+x', '%s/Defold%s' % (exe_dir, exe_suffix)])

    if 'win32' in platform:
        exec_command(['java', '-cp', 'target/classes', 'com.defold.util.IconExe', '%s/Defold%s' % (exe_dir, exe_suffix), 'bundle-resources/logo.ico'])

    extract(jre, 'tmp')
    print 'Creating bundle'
    if is_mac:
        jre_glob = 'tmp/jre1.8.0_%s.jre/Contents/Home/*' % (jre_minor)
    else:
        jre_glob = 'tmp/jre1.8.0_%s/*' % (jre_minor)

    for p in glob.glob(jre_glob):
        shutil.move(p, '%s/jre' % packages_dir)

    ziptree(bundle_dir, 'target/editor/Defold-%s.zip' % platform, 'tmp')

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

    parser.add_option('--git-rev', dest='git_rev',
                      default = 'HEAD',
                      help = 'Specific git rev to use. Useful when testing bundling.')

    parser.add_option('--pack-local', dest='pack_local',
                      default = False,
                      action = 'store_true',
                      help = 'Use local artifacts when packing resources for uberjar. Useful when testing bundling.')

    parser.add_option('--launcher', dest='launcher',
                      default = None,
                      help = 'Specific local launcher. Useful when testing bundling.')

    options, all_args = parser.parse_args()

    if not options.target_platform:
        parser.error('No platform specified')

    if not options.version:
        parser.error('No version specified')

    options.git_sha1 = git_sha1(options.git_rev)
    print 'Using git rev=%s, sha1=%s' % (options.git_rev, options.git_sha1)

    rmtree('target/editor')

    print 'Building editor'

    exec_command(['bash', './scripts/lein', 'clean'])

    if options.pack_local:
        exec_command(['bash', './scripts/lein', 'with-profile', '+release', 'pack'])
    else:
        exec_command(['bash', './scripts/lein', 'with-profile', '+release', 'pack', options.git_sha1])

    exec_command(['bash', './scripts/lein', 'with-profile', '+release', 'uberjar'])

    jar_file = 'defold-%s.jar' % options.git_sha1

    mkdirs('target/editor/update')
    shutil.copy('target/defold-editor-2.0.0-SNAPSHOT-standalone.jar', 'target/editor/update/%s' % jar_file)

    for platform in options.target_platform:
        bundle(platform, jar_file, options)

    package_info = {'version' : options.version,
                    'sha1' : options.git_sha1,
                    'packages': [{'url': jar_file,
                                  'platform': '*',
                                  'action': 'copy'}]}
    with open('target/editor/update/manifest.json', 'w') as f:
        f.write(json.dumps(package_info, indent=4))
