# Copyright 2020-2026 The Defold Foundation
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
import optparse
import run
import sys
import shutil
import subprocess
import tempfile
import time
import zipfile

def log(msg):
    print(msg)
    sys.stdout.flush()
    sys.stderr.flush()

def _platform_is_macos(platform):
    return platform.endswith("-macos")

def _platform_is_windows(platform):
    return platform.endswith("-win32")

def _platform_is_linux(platform):
    return platform.endswith("-linux")

class ExclusiveFileLock:
    def __init__(self, path):
        self.path = path
        self.file = None

    def __enter__(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        self.file = open(self.path, 'a+')
        if os.name == 'nt':
            import msvcrt
            while True:
                try:
                    self.file.seek(0)
                    msvcrt.locking(self.file.fileno(), msvcrt.LK_LOCK, 1)
                    break
                except OSError:
                    time.sleep(1)
        else:
            import fcntl
            fcntl.flock(self.file.fileno(), fcntl.LOCK_EX)
        return self

    def __exit__(self, _exc_type, _exc_value, _traceback):
        if os.name == 'nt':
            import msvcrt
            self.file.seek(0)
            msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            fcntl.flock(self.file.fileno(), fcntl.LOCK_UN)
        self.file.close()

def _codesigning_lock_path():
    return os.path.join(os.environ.get('DYNAMO_HOME', tempfile.gettempdir()), 'codesigning.lock')

def mac_certificate(codesigning_identity):
    if run.command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(codesigning_identity) >= 0:
        return codesigning_identity.replace("\"", "")
    else:
        return None

def sign_windows_file(options, file):
    gcloud = shutil.which('gcloud')
    if not gcloud:
        sys.exit("No gcloud tool found")

    env = os.environ.copy()
    env['CLOUDSDK_PYTHON'] = sys.executable

    run.env_command(env, [
        gcloud,
        'auth',
        'activate-service-account',
        '--key-file', options.gcloud_keyfile], silent = True)

    # Capture the token ourselves so we can strip any stray lines emitted by the Windows
    # Microsoft Store shim when `python.exe` is missing (it writes that warning to stdout).
    token_proc = subprocess.run(
        [gcloud, 'auth', 'print-access-token'],
        stdout = subprocess.PIPE,
        stderr = subprocess.PIPE,
        check = False,
        text = True,
        env = env)
    if token_proc.returncode != 0:
        log("gcloud auth print-access-token failed with exit code %d" % token_proc.returncode)
        if token_proc.stderr:
            log(token_proc.stderr.strip())
        sys.exit(1)

    token_lines = [line.strip() for line in token_proc.stdout.splitlines() if line.strip()]
    if not token_lines:
        log("Failed to read Google Cloud access token from gcloud output")
        if token_proc.stderr:
            log(token_proc.stderr.strip())
        sys.exit(1)
    storepass = token_lines[-1]

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

def sign_macos_file(options, file):
    codesigning_identity = options.codesigning_identity
    certificate = mac_certificate(codesigning_identity)
    if certificate is None:
        log("Codesigning certificate not found for signing identity %s" % codesigning_identity)
        sys.exit(1)

    if file.endswith(".app"):
        run.command([
            'codesign',
            '--deep',
            '--force',
            '--options', 'runtime',
            '--entitlements', options.codesigning_entitlements,
            '--sign', certificate,
            file])
    else:
        run.command([
            'codesign',
            '--force',
            '--options', 'runtime',
            '--sign', certificate,
            file])

def sign_file(platform, options, file):
    if _platform_is_windows(platform):
        with ExclusiveFileLock(_codesigning_lock_path()):
            sign_windows_file(options, file)

    if _platform_is_macos(platform):
        sign_macos_file(options, file)


def _main():
    parser = optparse.OptionParser()
    parser.add_option('--platform', dest='platform', help='Target platform')
    parser.add_option('--file', dest='file', help='File to sign')
    parser.add_option('--codesigning-identity', dest='codesigning_identity',
                      default='Developer ID Application: Stiftelsen Defold Foundation (26PW6SVA7H)',
                      help='Codesigning identity for macOS')
    parser.add_option('--gcloud-projectid', dest='gcloud_projectid',
                      help='Google Cloud project id where key ring is stored')
    parser.add_option('--gcloud-location', dest='gcloud_location',
                      help='Google Cloud region where key ring is located')
    parser.add_option('--gcloud-keyringname', dest='gcloud_keyringname',
                      help='Google Cloud key ring name')
    parser.add_option('--gcloud-keyname', dest='gcloud_keyname',
                      help='Google Cloud key name')
    parser.add_option('--gcloud-certfile', dest='gcloud_certfile',
                      help='Google Cloud certificate chain file')
    parser.add_option('--gcloud-keyfile', dest='gcloud_keyfile',
                      help='Google Cloud service account key file')

    options, _args = parser.parse_args()
    if not options.platform:
        parser.error('--platform is required')
    if not options.file:
        parser.error('--file is required')

    sign_file(options.platform, options, options.file)


if __name__ == '__main__':
    _main()
