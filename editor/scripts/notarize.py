#!/usr/bin/env python
# Copyright 2020-2022 The Defold Foundation
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



import os, sys, shutil, re, subprocess, time

def _log(msg):
    print(msg)
    sys.stdout.flush()
    sys.stderr.flush()

def _exec_command(arg_list, **kwargs):
    arg_str = arg_list
    if not isinstance(arg_str, str):
        arg_str = ' '.join(arg_list)
    _log('[exec] %s' % arg_str)

    if sys.stdout.isatty():
        # If not on CI, we want the colored output, and we get the output as it runs, in order to preserve the colors
        if not 'stdout' in kwargs:
            kwargs['stdout'] = subprocess.PIPE # Only way to get output from the command
        process = subprocess.Popen(arg_list, **kwargs)
        output = process.communicate()[0]
        if process.returncode != 0:
            _log(output)
    else:
        # On the CI machines, we make sure we produce a steady stream of output
        # However, this also makes us lose the color information
        if 'stdout' in kwargs:
            del kwargs['stdout']
        process = subprocess.Popen(arg_list, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, **kwargs)

        output = ''
        while True:
            line = process.stdout.readline().decode()
            if line != '':
                output += line
                _log(line.rstrip())
            else:
                break

    if process.wait() != 0:
        raise ExecException(process.returncode, output)

    return output


def get_status(uuid, notarization_username, notarization_password, notarization_itc_provider = None):
    args = [
        'xcrun',
        'altool',
        '--notarization-info', uuid,
        '-u', notarization_username,
        '-p', notarization_password]

    if notarization_itc_provider:
        args.extend(['-itc_provider', notarization_itc_provider])

    # altool will sometimes fail with "Error: Apple Services operation failed. Could not find the RequestUUID."
    # this can happen even with a valid uuid when Apple servers are busy and
    # there's a lag between receiving an uuid and being able to request info
    # about it
    # we need to catch the error from exec_command() and try again
    status = None
    try:
        res = _exec_command(args)
        pattern = r".*Status: (.*)"
        match = re.search(pattern, res)
        if match:
            status = match.group(1)
    except:
        pass

    return status


def notarize(app, notarization_username, notarization_password, notarization_itc_provider = None):
    if notarization_username is None or notarization_password is None:
        raise Exception("Apple notarization username or password is not set")

    _log('Sending editor for notarization')
    args = ['xcrun', 'altool', '--notarize-app',
                                '--type', 'osx',
                                '--file', app,
                                '--primary-bundle-id', "com.defold.editor",
                                '--username', notarization_username,
                                '--password', notarization_password]
    if notarization_itc_provider:
        args.extend(['-itc_provider', notarization_itc_provider])

    res = _exec_command(args)

    _log('Getting UUID for notarization request')

    # No errors uploading '/Users/runner/runners/2.163.1/work/defold/defold/editor/target/editor/Defold-x86_64-macos.dmg'.
    # RequestUUID = a062ed44-6da0-49c4-b0e6-0d1c28e1670d
    pattern = r".*RequestUUID = (.*)"
    match = re.search(pattern, res)
    if not match:
        _log("Unable to find notarization request UUID")
        sys.exit(1)
    uuid = match.group(1)

    while True:
        time.sleep(15)
        _log('Checking notarization status for "{}"'.format(uuid))
        status = get_status(uuid,notarization_username, notarization_password, notarization_itc_provider)
        if status == "success":
            _log('Notarization was successful')
            break
        elif status == "invalid":
            _log("Notarization failed")
            sys.exit(1)

    # staple approval
    _log('Stapling notarization approval to application')
    res = _exec_command([
        'xcrun',
        'stapler',
        'staple',
        app
    ])

if __name__ == '__main__':
    def _arg(index):
        return sys.argv[index] if len(sys.argv) > index else None

    app = _arg(1)
    username = _arg(2)
    password = _arg(3)
    itc_provider = _arg(4)

    if not app or not username or not password:
        _log("You must provide an app, username and password")
        sys.exit(1)

    notarize(app, username, password, itc_provider)
