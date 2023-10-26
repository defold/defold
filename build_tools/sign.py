#!/usr/bin/env python
# Copyright 2020-2023 The Defold Foundation
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

import run
import os
import sys

def mac_certificate(codesigning_identity):
    if run.command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(codesigning_identity) >= 0:
        return codesigning_identity
    else:
        return None

def sign_file(platform, options, file):
    if options.skip_codesign:
        return

    if not file:
        print("You must provide a file to sign")
        sys.exit(1)

    print("Signing file " + file)
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

    elif 'macos' in platform:
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
