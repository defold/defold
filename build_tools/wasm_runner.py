# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0.

import os
import shutil
import subprocess


WEB_PLATFORMS = ('wasm-web', 'wasm_pthread-web')
BUN_MIN_VERSION = (1, 3, 13)

_CHECK_SCRIPT = r'''
const fs = require("fs");
if (typeof process === "undefined") {
    throw new Error("process is missing");
}
if (!process.versions || !process.versions.node) {
    throw new Error("Node-compatible process.versions.node is missing");
}
if (typeof Buffer === "undefined") {
    throw new Error("Buffer is missing");
}
if (typeof WebAssembly === "undefined") {
    throw new Error("WebAssembly is missing");
}
fs.realpathSync(process.cwd());
'''


class WasmRunner(object):
    def __init__(self, name, path):
        self.name = name
        self.path = path

    def command(self):
        return [self.path]

    def description(self):
        return '%s (%s)' % (self.name, self.path)


def is_web_platform(platform):
    return platform in WEB_PLATFORMS


def _runner_name(path):
    name = os.path.basename(path).lower()
    if name.endswith('.exe'):
        name = name[:-4]
    if name.startswith('bun'):
        return 'bun'
    return 'node'


def _version_string(version):
    return '.'.join(str(v) for v in version)


def _parse_version(version):
    parts = []
    for part in version.strip().split('.'):
        try:
            parts.append(int(part))
        except ValueError:
            break
    while len(parts) < 3:
        parts.append(0)
    return tuple(parts[:3])


def _check_bun_version(path):
    try:
        output = subprocess.check_output([path, '--version'], stderr = subprocess.STDOUT, timeout = 10)
        version = _parse_version(output.decode('utf-8', errors = 'replace'))
        if version < BUN_MIN_VERSION:
            return False, 'Bun %s is too old, need >= %s' % (_version_string(version), _version_string(BUN_MIN_VERSION))
        return True, ''
    except subprocess.CalledProcessError as e:
        output = e.output.decode('utf-8', errors = 'replace') if e.output else ''
        return False, output.strip()
    except OSError as e:
        return False, str(e)
    except subprocess.TimeoutExpired:
        return False, 'timed out'


def _add_candidate(candidates, seen, path):
    if not path:
        return

    fullpath = os.path.abspath(path) if os.path.sep in path else path
    key = os.path.normcase(fullpath)
    if key in seen:
        return

    candidates.append(path)
    seen.add(key)


def _candidate_paths(node_candidates = None):
    candidates = []
    seen = set()

    _add_candidate(candidates, seen, shutil.which('bun'))

    for path in node_candidates or []:
        if path and os.path.isfile(path):
            _add_candidate(candidates, seen, path)

    _add_candidate(candidates, seen, shutil.which('node'))
    _add_candidate(candidates, seen, shutil.which('nodejs'))

    return candidates


def check_runner(path):
    name = _runner_name(path)
    if name == 'bun':
        ok, error = _check_bun_version(path)
        if not ok:
            return ok, error

    try:
        subprocess.check_output([path, '-e', _CHECK_SCRIPT], stderr = subprocess.STDOUT, timeout = 10)
        return True, ''
    except subprocess.CalledProcessError as e:
        output = e.output.decode('utf-8', errors = 'replace') if e.output else ''
        return False, output.strip()
    except OSError as e:
        return False, str(e)
    except subprocess.TimeoutExpired:
        return False, 'timed out'


def find_wasm_runner(node_candidates = None):
    errors = []
    for path in _candidate_paths(node_candidates):
        ok, error = check_runner(path)
        if ok:
            return WasmRunner(_runner_name(path), path), errors
        errors.append('%s: %s' % (path, error))
    return None, errors
