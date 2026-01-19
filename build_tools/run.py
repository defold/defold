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

from log import log
import subprocess
import sys

class ExecException(Exception):
    def __init__(self, retcode, output='', command=''):
        self.retcode = retcode
        self.output = output
        self.command = command
        super().__init__(f"Command failed with exit code {retcode}: {command}")

def _to_str(x):
    if x is None:
        return ''
    elif isinstance(x, (bytes, bytearray)):
        x = str(x, encoding='utf-8')
    return x

def _exec_command(arg_list, **kwargs):
    silent = False
    if 'silent' in kwargs:
        silent = True
        del kwargs['silent']

    arg_str = arg_list
    if not isinstance(arg_str, str):
        arg_list = [_to_str(x) for x in arg_list]
        arg_str = ' '.join(arg_list)
    if not silent: log('[exec] %s' % arg_str)

    if 'stdout' in kwargs:
        stdout_mode = kwargs['stdout']
        del kwargs['stdout']
    else:
        # If not on CI, we want the colored output, and we get the output as it runs, in order to preserve the colors
        stdout_mode = subprocess.PIPE # Only way to get output from the command
    if 'stderr' in kwargs:
        stderr_mode = kwargs['stderr']
        del kwargs['stderr']
    else:
        stderr_mode = None

    process = subprocess.Popen(arg_list, stdout=stdout_mode, stderr=stderr_mode, **kwargs)

    output = ''
    while stdout_mode == subprocess.PIPE:
        line = process.stdout.readline().decode(errors='replace')
        if line != '':
            output += line
            if not silent:
                log(line.rstrip())
        else:
            break

    if process.wait() != 0:
        e = ExecException(process.returncode, output, arg_str)
        e.output = output
        log('[exec] %s' % arg_str)
        log("Error: %s" % _to_str(output))
        raise e

    output = _to_str(output)
    return output.strip()

def command(args, **kwargs):
    if kwargs.get("shell") is None:
        kwargs["shell"] = False
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, **kwargs)
    except ExecException as e:
        log("Error: %s" % _to_str(output))
        sys.exit(e.retcode)

def shell_command(args, **kwargs):
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, shell = True, **kwargs)
    except ExecException as e:
        log("Error: %s" % _to_str(output))
        sys.exit(e.retcode)


def env_command(env, args, **kwargs):
    return _exec_command(args, shell = False, stdout = None, env = env, **kwargs)

def env_shell_command(env, args, **kwargs):
    return _exec_command(args, shell = True, env = env, **kwargs)
