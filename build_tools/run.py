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
    def __init__(self, retcode, output):
        self.retcode = retcode
        self.output = output
    def __init__(self, retcode):
        self.retcode = retcode
        self.output = ''

def _to_str(x):
    if x is None:
        return ''
    elif isinstance(x, (bytes, bytearray)):
        x = str(x, encoding='utf-8')
    return x

def _to_str_set(values):
    if values is None:
        return set()
    if isinstance(values, (list, tuple, set)):
        iterable = values
    else:
        iterable = [values]
    out = set()
    for v in iterable:
        s = _to_str(v)
        if s:
            out.add(s)
    return out

def _sanitize_text(text, secrets):
    text = _to_str(text)
    for secret in secrets:
        if secret:
            text = text.replace(secret, '***')
    return text

def _redact_args(arg_list, redact_flags, secrets):
    redacted = []
    redact_next = False

    for arg in arg_list:
        value = _to_str(arg)
        if redact_next:
            if value:
                secrets.add(value)
            redacted.append('***')
            redact_next = False
            continue

        is_redacted = False
        for flag in redact_flags:
            prefix = flag + '='
            if value == flag:
                redacted.append(value)
                redact_next = True
                is_redacted = True
                break
            elif value.startswith(prefix):
                secret = value[len(prefix):]
                if secret:
                    secrets.add(secret)
                redacted.append(prefix + '***')
                is_redacted = True
                break

        if not is_redacted:
            redacted.append(value)

    return redacted, secrets

def _exec_command(arg_list, **kwargs):
    silent = False
    if 'silent' in kwargs:
        silent = True
        del kwargs['silent']

    redact_flags = kwargs.pop('redact_flags', ['--password', '--passphrase', '--token', '--secret', '--api-key', '--client-secret'])
    if isinstance(redact_flags, str):
        redact_flags = [redact_flags]
    secrets = _to_str_set(kwargs.pop('redacted_values', None))

    arg_str = arg_list
    if not isinstance(arg_str, str):
        arg_list = [_to_str(x) for x in arg_list]
        redacted_args, secrets = _redact_args(arg_list, redact_flags, secrets)
        arg_str = ' '.join(redacted_args)
    else:
        arg_str = _sanitize_text(arg_str, secrets)
    if not silent: log('[exec] %s' % arg_str)

    # Keep stdout and stderr in their original order. In particular, Ninja may
    # print the failed command on stdout while the compiler or linker writes the
    # useful diagnostic to stderr.
    stream_output = 'stdout' not in kwargs and 'stderr' not in kwargs
    if stream_output:
        kwargs['stdout'] = subprocess.PIPE
        kwargs['stderr'] = subprocess.STDOUT

    process = subprocess.Popen(arg_list, **kwargs)
    output = ''
    if stream_output:
        while True:
            line = _to_str(process.stdout.readline())
            if line == '':
                break
            output += line
            if not silent:
                log(_sanitize_text(line, secrets).rstrip('\r\n'))
        process.stdout.close()
    else:
        captured_stdout, captured_stderr = process.communicate()
        output = _to_str(captured_stdout) + _to_str(captured_stderr)

    retcode = process.wait()
    output = _sanitize_text(output, secrets)
    if retcode != 0:
        e = ExecException(retcode)
        e.output = output
        if not silent:
            log('[exec] failed command: %s' % arg_str)
            log('[exec] command failed with exit code %s' % retcode)
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
        sys.exit(e.retcode)

def shell_command(args, **kwargs):
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, shell = True, **kwargs)
    except ExecException as e:
        sys.exit(e.retcode)


def env_command(env, args, **kwargs):
    return _exec_command(args, shell = False, env = env, **kwargs)

def env_shell_command(env, args, **kwargs):
    return _exec_command(args, shell = True, env = env, **kwargs)
