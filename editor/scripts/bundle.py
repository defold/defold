#!/usr/bin/env python

import os
import glob
import sys
import tempfile
import urllib
import optparse
import urlparse
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
    process = subprocess.Popen(args, stdout = subprocess.PIPE, shell = True)

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


if __name__ == '__main__':
    usage = '''usage: %prog [options] command(s)'''

    parser = optparse.OptionParser(usage)

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      choices = ['x86_64-linux', 'x86-linux', 'x86_64-darwin', 'x86-win32', 'x86_64-win32'],
                      help = 'Target platform')

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version')

    options, all_args = parser.parse_args()
    if not options.target_platform:
        parser.error('No platform specified')

    if not options.version:
        parser.error('No version specified')

    if os.path.exists('tmp'):
        shutil.rmtree('tmp')

    jre_minor = 45
    jre_url = 'https://s3-eu-west-1.amazonaws.com/defold-packages/jre-8u%d-%s.gz' % (jre_minor, platform_to_java[options.target_platform])
    jre = download(jre_url)
    if not jre:
        print('Failed to download %s' % jre_url)
        sys.exit(5)

    # TODO: Hack. We should download from s3 based on platform
    launcher = download('https://s3-eu-west-1.amazonaws.com/defold-packages/slask/launcher', use_cache = False)
    if not launcher:
        print('Failed to download launcher')
        sys.exit(5)

    mkdirs('tmp')

    if 'darwin' in options.target_platform:
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

    print 'Building editor'
    exec_command('lein clean')
    exec_command('lein uberjar')
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
    config.set('launcher', 'jar', 'packages/defold-%s.jar' % options.version)

    with open('%s/config' % resources_dir, 'wb') as f:
        config.write(f)

    shutil.copy('target/defold-editor-2.0.0-SNAPSHOT-standalone.jar', '%s/defold-%s.jar' % (packages_dir, options.version))
    shutil.copy(launcher, '%s/Defold' % exe_dir)
    exec_command('chmod +x %s/Defold' % exe_dir)

    extract(jre, 'tmp')
    print 'Creating bundle'
    if is_mac:
        jre_glob = 'tmp/jre1.8.0_45.jre/Contents/Home/*'
    else:
        jre_glob = 'tmp/jre1.8.0_45.jre/*'

    for p in glob.glob(jre_glob):
        shutil.move(p, '%s/jre' % packages_dir)

    ziptree(bundle_dir, 'target/Defold-%s.zip' % options.target_platform, 'tmp')
