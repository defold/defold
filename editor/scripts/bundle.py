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



import os
import os.path as path
import stat
import sys
import optparse
import platform
import re
import shutil
import subprocess
import zipfile
import tarfile
import configparser
import datetime
import fnmatch
import urllib
import urllib.parse

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build_tools'))

# TODO: collect common functions in a more suitable reusable module
try:
    sys.dont_write_bytecode = True
    import build_private
except Exception as e:
    class build_private(object):
        @classmethod
        def get_tag_suffix(self):
            return ''
finally:
    sys.dont_write_bytecode = False

# defold/build_tools
import run
import http_cache


DEFAULT_ARCHIVE_DOMAIN=os.environ.get("DM_ARCHIVE_DOMAIN", "d.defold.com")

# If you update java version, don't forget to update it here too:
# - /editor/bundle-resources/config at "launcher.jdk" key
# - /scripts/build.py smoke_test, `java` variable
# - /editor/src/clj/editor/updater.clj, `protected-dirs` let binding
java_version = '21.0.5+11'

platform_to_java = {'x86_64-linux': 'x64_linux',
                    'x86_64-macos': 'x64_mac',
                    'arm64-macos': 'aarch64_mac',
                    'x86_64-win32': 'x64_windows'}

python_platform_to_java = {'x86_64-linux': 'x64_linux',
                           'x86_64-win32': 'x64_windows',
                           'x86_64-darwin': 'x64_mac',
                           'arm64-darwin': 'aarch64_mac'}

def log(msg):
    print(msg)
    sys.stdout.flush()
    sys.stderr.flush()

def mkdirs(path):
    if not os.path.exists(path):
        os.makedirs(path)

def extract_tar(file, path):
    run.command(['tar', '-C', path, '-xzf', file])

def extract_zip(file, path, is_mac):
    if is_mac:
        # We use the system unzip command for macOS because zip (and
        # others) on macOS has special handling of ._ files that
        # contain additional HFS+ attributes. See for example:
        # http://superuser.com/questions/61185/why-do-i-get-files-like-foo-in-my-tarball-on-os-x
        run.command(['unzip', file, '-d', path])
    else:
        with zipfile.ZipFile(file, 'r') as zf:
            zf.extractall(path)

def extract(file, path, is_mac):
    print('Extracting %s to %s' % (file, path))

    if fnmatch.fnmatch(file, "*.tar.gz-*"):
        extract_tar(file, path)
    elif fnmatch.fnmatch(file, "*.zip-*"):
        extract_zip(file, path, is_mac)
    else:
        assert False, "Don't know how to extract " + file

def download(url):
    path = http_cache.download(url, lambda count, total: log('Downloading %s %.2f%%' % (url, 100 * count / float(total))))
    if not path:
        log('Downloading %s failed' % (url))
    return path

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

def gz_tree(path, outfile, directory=None):
    # compress files and folders in path to outfile using tar:gz compression

    archive = tarfile.open(outfile, 'w:gz')
    for root, dirs, files in os.walk(path):
        for f in files:
            p = os.path.join(root, f)
            an = p
            if directory:
                an = os.path.relpath(p, directory)
            archive.add(p, an)

    archive.close()
    return outfile


def _get_tag_name(version, channel): # from build.py
    if channel and channel != 'stable' and not channel.startswith('editor-'):
        channel = '-' + channel
    else:
        channel = ''

    suffix = build_private.get_tag_suffix() # E.g. '' or 'switch'
    if suffix:
        suffix = '-' + suffix

    return '%s%s%s' % (version, channel, suffix)

def git_sha1_from_version_file(options):
    """ Gets the version number and checks if that tag exists """
    with open('../VERSION', 'r', encoding='utf-8') as version_file:
        version = version_file.read().strip()

    tag_name = _get_tag_name(version, options.channel)

    process = subprocess.Popen(['git', 'rev-list', '-n', '1', tag_name], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        print("Unable to find git sha from tag=%s" % tag_name)
        return None

    if isinstance(out, (bytes, bytearray)):
        out = str(out, encoding='utf-8')
    return out.strip()

def git_sha1(ref = 'HEAD'):
    try:
        return run.command(['git', 'rev-parse', ref])
    except Exception as e:
        sys.exit("Unable to find git sha from ref: %s" % (ref))

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
    if run.command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(codesigning_identity) >= 0:
        return codesigning_identity
    else:
        return None

def sign_file(platform, options, file):
    if options.skip_codesign:
        return
    if 'win32' in platform:
        run.command([
            'gcloud',
            'auth',
            'activate-service-account',
            '--key-file', options.gcloud_keyfile], silent = True)

        storepass = run.command([
            'gcloud',
            'auth',
            'print-access-token'], silent = True)

        jsign = os.path.join(os.environ['DYNAMO_HOME'], 'ext','share','java','jsign-4.2.jar')
        keystore = "projects/%s/locations/%s/keyRings/%s" % (options.gcloud_projectid, options.gcloud_location, options.gcloud_keyringname)
        run.command([
            'java', '-jar', jsign,
            '--storetype', 'GOOGLECLOUD',
            '--storepass', storepass,
            '--keystore', keystore,
            '--alias', options.gcloud_keyname,
            '--certfile', options.gcloud_certfile,
            '--tsmode', 'RFC3161',
            '--tsaurl', 'http://timestamp.globalsign.com/tsa/r6advanced1',
            file], silent = True)

    if 'macos' in platform:
        codesigning_identity = options.codesigning_identity
        certificate = mac_certificate(codesigning_identity)
        if certificate == None:
            print("Codesigning certificate not found for signing identity %s" % (codesigning_identity))
            sys.exit(1)

        run.command([
            'codesign',
            '--deep',
            '--force',
            '--options', 'runtime',
            '--entitlements', './scripts/entitlements.plist',
            '-s', certificate,
            file])

def launcher_path(options, platform, exe_suffix):
    if options.launcher:
        return options.launcher
    elif options.engine_sha1:
        launcher_version = options.engine_sha1
        launcher_url = 'https://%s/archive/%s/engine/%s/launcher%s' % (options.archive_domain, launcher_version, platform, exe_suffix)
        launcher = download(launcher_url)
        if not launcher:
            print('Failed to download launcher', launcher_url)
            sys.exit(5)
        return launcher
    else:
        return path.join(os.environ['DYNAMO_HOME'], "bin", platform, "launcher%s" % exe_suffix)

def full_jdk_url(jdk_platform):
    version = urllib.parse.quote(java_version)
    platform = urllib.parse.quote(jdk_platform)
    extension = "zip" if "windows" in jdk_platform else "tar.gz"
    major_version = java_version.split('.')[0]
    artifact_version = java_version.replace('+', '_')
    return 'https://github.com/adoptium/temurin%s-binaries/releases/download/jdk-%s/OpenJDK%sU-jdk_%s_hotspot_%s.%s' % (major_version, version, major_version, platform, artifact_version, extension)

def full_build_jdk_url():
    return full_jdk_url(python_platform_to_java["%s-%s" % (platform.machine(), sys.platform)])

def download_build_jdk():
    print('Downloading build jdk')
    jdk_url = full_build_jdk_url()
    jdk = download(jdk_url)
    if not jdk:
        print('Failed to download build jdk %s' % jdk_url)
        sys.exit(5)
    return jdk

def extract_build_jdk(build_jdk):
    print('Extracting build jdk')
    rmtree('build/jdk')
    mkdirs('build/jdk')
    is_mac = sys.platform == 'darwin'
    extract(build_jdk, 'build/jdk', is_mac)
    if is_mac:
        return 'build/jdk/jdk-%s/Contents/Home' % java_version
    else:
        return 'build/jdk/jdk-%s' % java_version
    
def invoke_lein(args, jdk_path=None):
    # this weird dance with env and bash instead of supplying env kwarg to run.command is needed for the build script to work on windows
    jdk_path = jdk_path or os.environ['JAVA_HOME']
    return run.command(['env', 'JAVA_CMD=%s/bin/java' % jdk_path, 'LEIN_HOME=build/lein', 'bash', './scripts/lein'] + args, stdout=True)

def check_reflections(jdk_path):
    reflection_prefix = 'Reflection warning, ' # final space important
    included_reflections = ['editor/', 'util/'] # [] = include all
    ignored_reflections = []

    # lein check puts reflection warnings on stderr, redirect to stdout to capture all output
    output = invoke_lein(['with-profile', '+headless', 'check-and-exit'], jdk_path=jdk_path)
    lines = output.splitlines()
    reflection_lines = (line for line in lines if re.match(reflection_prefix, line))
    reflections = (re.match('(' + reflection_prefix + ')(.*)', line).group(2) for line in reflection_lines)
    filtered_reflections = reflections if not included_reflections else (line for line in reflections if any((re.match(include, line) for include in included_reflections)))
    failures = list(line for line in filtered_reflections if not any((re.match(ignored, line) for ignored in ignored_reflections)))

    if failures:
        for failure in failures:
            print(failure)
        exit(1)

def write_docs(docs_dir, jdk_path=None):
    invoke_lein(['with-profile', '+docs', 'run', '-m', 'editor.docs', docs_dir], jdk_path)

def build(options):
    build_jdk = download_build_jdk()
    extracted_build_jdk = extract_build_jdk(build_jdk)

    print('Building editor')

    init_command = ['with-profile', '+release', 'init']
    if options.engine_sha1:
        init_command += [options.engine_sha1]

    invoke_lein(init_command, jdk_path=extracted_build_jdk)

    build_ns_batches_command = ['run', '-m', 'editor.ns-batch-builder', 'resources/sorted_clojure_ns_list.edn']
    invoke_lein(build_ns_batches_command, jdk_path=extracted_build_jdk)

    check_reflections(extracted_build_jdk)

    if options.skip_tests:
        print('Skipping tests')
    else:
        invoke_lein(['test'], jdk_path=extracted_build_jdk)
        # test that docs can be successfully produced
        write_docs('target/docs', jdk_path=extracted_build_jdk)

    invoke_lein(['prerelease'], jdk_path=extracted_build_jdk)


def get_exe_suffix(platform):
    return ".exe" if 'win32' in platform else ""


def remove_platform_files_from_archive(platform, jar):
    zin = zipfile.ZipFile(jar, 'r')
    files = zin.namelist()
    files_to_remove = []

    # find files to remove from libexec/*
    libexec_platform = "libexec/" + platform
    _unpack_platform = "_unpack/" + platform
    for file in files:
        if file.startswith("libexec"):
            # don't remove any folders
            if file.endswith("/"):
                continue
            # don't touch anything for the current platform
            if file.startswith(libexec_platform):
                continue
            # don't touch any of the dmengine files
            if "dmengine" in file:
                continue
            # don't remove the cross-platform bundletool-all.jar
            if "bundletool-all.jar" in file:
                continue
            # anything else should be removed
            files_to_remove.append(file)
        # keep files needed only for this particular platform (+ shared files in '_defold' and 'shared')
        if file.startswith("_unpack"):
            # don't touch '_unpack/'
            if file == "_unpack/":
                continue
            # don't touch anything for the current platform
            if file.startswith(_unpack_platform):
                continue
            # keep shared files
            if file.startswith("_unpack/shared"):
                continue
            # keep _defold folder
            if file.startswith("_unpack/_defold"):
                continue
            # anything else should be removed
            files_to_remove.append(file)


    # find libs to remove in the root folder
    for file in files:
        if "/" not in file:
            if platform in ["x86_64-macos", "arm64-macos"] and (file.endswith(".so") or file.endswith(".dll")):
                files_to_remove.append(file)
            elif platform in ["x86_64-win32"] and (file.endswith(".so") or file.endswith(".dylib")):
                files_to_remove.append(file)
            elif platform in ["x86_64-linux", "arm64-linux"]  and (file.endswith(".dll") or file.endswith(".dylib")):
                files_to_remove.append(file)

    # write new jar without the files that should be removed
    newjar = jar + "_new"
    zout = zipfile.ZipFile(newjar, 'w', zipfile.ZIP_DEFLATED, allowZip64=True)
    for file in zin.infolist():
        if file.filename not in files_to_remove:
            zout.writestr(file, zin.read(file))
    zout.close()
    zin.close()

    # switch to jar without removed files
    os.remove(jar)
    os.rename(newjar, jar)


def create_bundle(options):
    build_jdk = download_build_jdk()
    extracted_build_jdk = extract_build_jdk(build_jdk)

    mkdirs('target/editor')
    for platform in options.target_platform:
        print("Creating uberjar for platform %s" % platform)
        invoke_lein(['with-profile', 'release,%s' % platform, 'uberjar'], jdk_path=extracted_build_jdk)
        jar_file = 'target/editor-%s-standalone.jar' % platform
        print("Creating bundle for platform %s" % platform)
        rmtree('tmp')

        jdk_url = full_jdk_url(platform_to_java[platform])
        if jdk_url == full_build_jdk_url():
            jdk = build_jdk
        else:
            jdk = download(jdk_url)
            if not jdk:
                print('Failed to download %s' % jdk_url)
                sys.exit(5)

        tmp_dir = "tmp"

        is_mac = 'macos' in platform
        if is_mac:
            resources_dir = os.path.join(tmp_dir, 'Defold.app/Contents/Resources')
            packages_dir = os.path.join(tmp_dir, 'Defold.app/Contents/Resources/packages')
            bundle_dir = os.path.join(tmp_dir, 'Defold.app')
            exe_dir = os.path.join(tmp_dir, 'Defold.app/Contents/MacOS')
            icon = 'logo.icns'
        else:
            resources_dir = os.path.join(tmp_dir, 'Defold')
            packages_dir = os.path.join(tmp_dir, 'Defold/packages')
            bundle_dir = os.path.join(tmp_dir, 'Defold')
            exe_dir = os.path.join(tmp_dir, 'Defold')
            icon = 'logo_blue.png'

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
        config = configparser.ConfigParser()
        config.read('bundle-resources/config')
        config.set('build', 'editor_sha1', options.editor_sha1)
        config.set('build', 'engine_sha1', options.engine_sha1)
        config.set('build', 'version', options.version)
        config.set('build', 'time', datetime.datetime.now().isoformat())
        config.set('build', 'archive_domain', options.archive_domain)

        if options.channel:
            config.set('build', 'channel', options.channel)

        with open('%s/config' % resources_dir, 'w') as f:
            config.write(f)

        defold_jar = '%s/defold-%s.jar' % (packages_dir, options.editor_sha1)
        shutil.copy(jar_file, defold_jar)

        # strip tools and libs for the platforms we're not currently bundling
        remove_platform_files_from_archive(platform, defold_jar)

        # copy editor executable (the launcher)
        launcher = launcher_path(options, platform, get_exe_suffix(platform))
        defold_exe = '%s/Defold%s' % (exe_dir, get_exe_suffix(platform))
        shutil.copy(launcher, defold_exe)
        if not 'win32' in platform:
            run.command(['chmod', '+x', defold_exe])

        extract(jdk, tmp_dir, is_mac)

        if is_mac:
            platform_jdk = '%s/jdk-%s/Contents/Home' % (tmp_dir, java_version)
        else:
            platform_jdk = '%s/jdk-%s' % (tmp_dir, java_version)

        # use jlink to generate a custom Java runtime to bundle with the editor
        packages_jdk = '%s/jdk-%s' % (packages_dir, java_version)
        run.command(['%s/bin/jlink' % extracted_build_jdk,
                      '@jlink-options',
                      '--module-path=%s/jmods' % platform_jdk,
                      '--output=%s' % packages_jdk])

        # create final zip file
        zipfile = 'target/editor/Defold-%s.zip' % platform
        if os.path.exists(zipfile):
            os.remove(zipfile)

        print("Creating '%s' bundle from '%s'" % (zipfile, bundle_dir))
        ziptree(bundle_dir, zipfile, tmp_dir)
        print(f"The file size is: {os.path.getsize(zipfile) / 1048576:.2f} MB")

        # create additional tar.gz bundle in case of Linux
        is_linux = 'linux' in platform
        if is_linux:
            gz_file = 'target/editor/Defold-%s.tar.gz' % platform
            if os.path.exists(gz_file):
                os.remove(gz_file)

            print("Creating '%s' bundle from '%s'" % (gz_file, bundle_dir))
            gz_tree(bundle_dir, gz_file, tmp_dir)


def sign(options):
    for platform in options.target_platform:
        # check that we have an editor bundle to sign
        if 'win32' in platform:
            bundle_file = os.path.join(options.bundle_dir, "Defold-x86_64-win32.zip")
        elif 'macos' in platform:
            bundle_file = os.path.join(options.bundle_dir, "Defold-%s.zip" % platform)
        else:
            print("No signing support for platform %s" % platform)
            continue

        if not os.path.exists(bundle_file):
            print('Editor bundle %s does not exist' % bundle_file)
            run.command(['ls', '-la', options.bundle_dir])
            sys.exit(1)

        # setup
        sign_dir = os.path.join("build", "sign")
        rmtree(sign_dir)
        mkdirs(sign_dir)

        # unzip
        run.command(['unzip', bundle_file, '-d', sign_dir])

        # sign files
        if 'macos' in platform:
            # we need to sign the binaries in Resources folder manually as codesign of
            # the *.app will not process files in Resources
            jdk_dir = "jdk-%s" % (java_version)
            jdk_path = os.path.join(sign_dir, "Defold.app", "Contents", "Resources", "packages", jdk_dir)
            for exe in find_files(os.path.join(jdk_path, "bin"), "*"):
                sign_file('macos', options, exe)
            for lib in find_files(os.path.join(jdk_path, "lib"), "*.dylib"):
                sign_file('macos', options, lib)
            sign_file('macos', options, os.path.join(jdk_path, "lib", "jspawnhelper"))
            sign_file('macos', options, os.path.join(sign_dir, "Defold.app"))
        elif 'win32' in platform:
            sign_file('win32', options, os.path.join(sign_dir, "Defold", "Defold.exe"))

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

def create_dmg(options, platform):
    print("Creating .dmg from file in '%s'" % options.bundle_dir)

    # check that we have an editor bundle to create a dmg from
    bundle_file = os.path.join(options.bundle_dir, "Defold-%s.zip" % platform)
    if not os.path.exists(bundle_file):
        print('Editor bundle %s does not exist' % bundle_file)
        run.command(['ls', '-la', options.bundle_dir])
        sys.exit(1)

    # setup
    dmg_dir = os.path.join("build", "dmg")
    rmtree(dmg_dir)
    mkdirs(dmg_dir)

    # unzip (.zip -> .app)
    run.command(['unzip', bundle_file, '-d', dmg_dir])

    # create additional files for the .dmg
    shutil.copy('bundle-resources/dmg_ds_store', '%s/.DS_Store' % dmg_dir)
    shutil.copytree('bundle-resources/dmg_background', '%s/.background' % dmg_dir)
    run.command(['ln', '-sf', '/Applications', '%s/Applications' % dmg_dir])

    # create dmg
    dmg_file = os.path.join(options.bundle_dir, "Defold-%s.dmg" % platform)
    if os.path.exists(dmg_file):
        os.remove(dmg_file)
    run.command(['hdiutil', 'create', '-fs', 'JHFS+', '-volname', 'Defold', '-srcfolder', dmg_dir, dmg_file])

    # sign the dmg
    if not options.skip_codesign:
        certificate = mac_certificate(options.codesigning_identity)
        if certificate == None:
            error("Codesigning certificate not found for signing identity %s" % (options.codesigning_identity))
            sys.exit(1)

        run.command(['codesign', '-s', certificate, dmg_file])


def create_installer(options):
    print("Creating installers")
    for platform in options.target_platform:
        if platform == "x86_64-macos" or platform == "arm64-macos":
            print("Creating installer for platform %s" % platform)
            create_dmg(options, platform)
        else:
            print("Ignoring platform %s" % platform)


if __name__ == '__main__':
    usage = '''%prog [options] command(s)

Commands:
  build                 Build editor
  bundle                Create editor bundle (zip) from built files
  sign                  Sign editor bundle (zip)
  docs                  Produce docs (editor.apidoc)
  installer             Create editor installer from bundle (zip)'''

    parser = optparse.OptionParser(usage)

    parser.add_option('--platform', dest='target_platform',
                      default = None,
                      action = 'append',
                      choices = ['x86_64-linux', 'arm64-macos', 'x86_64-macos', 'x86_64-win32'],
                      help = 'Target platform to create editor bundle for. Specify multiple times for multiple platforms')

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version to set in editor config when creating the bundle')

    parser.add_option('--channel', dest='channel',
                      default = None,
                      help = 'Channel to set in editor config when creating the bundle')

    default_archive_domain = DEFAULT_ARCHIVE_DOMAIN
    parser.add_option('--archive-domain', dest='archive_domain',
                      default = DEFAULT_ARCHIVE_DOMAIN,
                      help = 'Domain to set in the editor config where builds are archived. Default is %s' % default_archive_domain)

    parser.add_option('--engine-artifacts', dest='engine_artifacts',
                      default = 'auto',
                      help = "Which engine artifacts to use when building. Can be 'auto', 'dynamo-home', 'archived', 'archived-stable' or a sha1.")

    parser.add_option('--launcher', dest='launcher',
                      default = None,
                      help = 'Specific local launcher to use when creating the bundle, e.g. "../engine/tools/build/src/launcher/launcher". Useful when testing.')

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

    parser.add_option('--bundle-dir', dest='bundle_dir',
                      default = "target/editor",
                      help = 'Path to directory containing editor bundles')

    parser.add_option('--docs-dir',
                      default = 'target/docs',
                      dest = 'docs_dir',
                      help = 'Path to directory for docs output')

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
        options.engine_sha1 = git_sha1_from_version_file(options) or git_sha1('HEAD')
    elif options.engine_artifacts == 'dynamo-home':
        options.engine_sha1 = None
    elif options.engine_artifacts == 'archived':
        options.engine_sha1 = git_sha1('HEAD')
    elif options.engine_artifacts == 'archived-stable':
        options.engine_sha1 = git_sha1_from_version_file(options)
        if not options.engine_sha1:
            print("Unable to find git sha from VERSION file")
            sys.exit(1)
    else:
        options.engine_sha1 = options.engine_artifacts

    print('Resolved engine_artifacts=%s to sha1=%s' % (options.engine_artifacts, options.engine_sha1))

    for command in commands:
        if command == "docs":
            write_docs(options.docs_dir)
        if command == "build":
            build(options)
        elif command == "sign":
            sign(options)
        elif command == "bundle":
            create_bundle(options)
        elif command == "installer":
            create_installer(options)
