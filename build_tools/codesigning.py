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
import run
import sys
import shutil
import subprocess
import tempfile
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

def mac_certificate(codesigning_identity):
    if run.command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(codesigning_identity) >= 0:
        return codesigning_identity
    else:
        return None

def sign_file(platform, options, file):
    if _platform_is_windows(platform):
        gcloud = shutil.which('gcloud')
        if not gcloud:
            sys.exit("No gcloud tool found")

        run.command([
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
            text = True)
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

    if _platform_is_macos(platform):
        codesigning_identity = options.codesigning_identity
        certificate = mac_certificate(codesigning_identity)
        if certificate is None:
            log("Codesigning certificate not found for signing identity %s" % codesigning_identity)
            sys.exit(1)

        run.command([
            'codesign',
            '--deep',
            '--force',
            '--options', 'runtime',
            '--entitlements', './scripts/entitlements.plist',
            '-s', certificate,
            file])


def sign_files_in_zip(platform, options, zip_file):
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_zip = os.path.join(tmpdir, 'signed.zip')

        with zipfile.ZipFile(zip_file, 'r') as src_zip:
            with zipfile.ZipFile(tmp_zip, 'w') as dst_zip:
                for info in src_zip.infolist():
                    name = info.filename

                    # Keep directory entries unchanged.
                    if info.is_dir():
                        dst_zip.writestr(info, b'')
                        continue

                    lower_name = name.lower()
                    should_sign = lower_name.endswith('.exe') or lower_name.endswith('.dll')

                    if should_sign:
                        extracted_path = src_zip.extract(info, tmpdir)
                        sign_file(platform, options, extracted_path)

                        # Preserve the original zip metadata/compression settings.
                        with open(extracted_path, 'rb') as f:
                            dst_zip.writestr(info, f.read())
                    else:
                        dst_zip.writestr(info, src_zip.read(name))

        shutil.move(tmp_zip, zip_file)

