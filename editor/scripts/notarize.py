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



import os, sys, shutil, re, subprocess, time, json

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
        raise Exception(f"Command failed with return code {process.returncode}\nOutput: {output}")

    return output


def get_status(uuid, notarization_username, notarization_password, notarization_team_id = None):
    args = ['xcrun', 'notarytool', 'info',
            '--progress',
            '--output-format', 'json',
            '--apple-id', notarization_username,
            '--password', notarization_password]

    if notarization_team_id:
        args.extend(['--team-id', notarization_team_id])

    args.append(uuid)

    status = None
    try:
        res = json.loads(_exec_command(args))
        status = res["status"]
    except:
        pass

    return status

# https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/customizing_the_notarization_workflow?language=objc
def notarize(app, notarization_username, notarization_password, notarization_team_id = None):
    if notarization_username is None or notarization_password is None:
        raise Exception("Apple notarization username or password is not set")

    _log('Sending editor for notarization')

    args = ['xcrun', 'notarytool', 'submit',
            '--progress',
            '--output-format', 'json',
            '--apple-id', notarization_username,
            '--password', notarization_password]

    if notarization_team_id:
        args.extend(['--team-id', notarization_team_id])

    args.append(app)

    res = json.loads(_exec_command(args))
    id = res["id"]
    _log('UUID for notarization request is "{}"'.format(id))

    while True:
        time.sleep(15)
        _log('Checking notarization status for "{}"'.format(id))
        status = get_status(id, notarization_username, notarization_password, notarization_team_id)
        if status == "Accepted":
            _log('Notarization was successful')
            break
        elif status == "Invalid":
            _log("Notarization failed")
            sys.exit(1)
        else:
            _log("Notarization status is {}".format(status))

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
    team_id = _arg(4)

    if not app or not username or not password:
        _log("You must provide an app, username and password")
        sys.exit(1)

    max_retries = 3
    attempt = 0

    while attempt < max_retries:
        try:
            _log(f"Notarization attempt {attempt + 1} of {max_retries}")
            notarize(app, username, password, team_id)
            _log("Notarization succeeded")
            break  # Exit the loop if notarization is successful
        except Exception as e:
            _log(f"Notarization failed on attempt {attempt + 1}: {e}")
            attempt += 1
            if attempt >= max_retries:
                _log("Max retries reached. Notarization failed.")
                sys.exit(1)
            else:
                _log("Retrying notarization...")
