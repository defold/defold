#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0 (https://www.defold.com/license).
"""Run buffer replacement regressions using the graphics CI platform policy."""
import argparse
from pathlib import Path
import re
import subprocess
import sys


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--target-platform', required=True)
    parser.add_argument('--available', nargs='*', default=[])
    parser.add_argument('--skip-all')
    args = parser.parse_args(argv)
    backends = [backend for backend in ('metal', 'opengl') if backend in args.available]
    if args.skip_all or not backends:
        print('Buffer replacement: SKIP: ' + (args.skip_all or 'Metal/OpenGL not built'))
        return 0
    failed = False
    for backend in backends:
        command = [str(args.executable.resolve()), backend, 'buffer-replacement']
        options = dict(stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       timeout=60, text=True, errors='replace')
        try:
            process = subprocess.run(command, **options)
            print(process.stdout, end='')
            results = re.findall(r'^GRAPHICS_BUFFER_REPLACEMENT_RESULT=(-?\d+)$', process.stdout, re.MULTILINE)
            passed = process.returncode == 0 and results == ['0']
            passed = passed and not re.search(r'ERROR:|FATAL:|Validation Error', process.stdout, re.IGNORECASE)
        except (OSError, subprocess.TimeoutExpired) as error:
            if isinstance(error, subprocess.TimeoutExpired) and error.stdout:
                print(error.stdout.decode(errors='replace') if isinstance(error.stdout, bytes) else error.stdout)
            print(error)
            passed = False
        print('Buffer replacement %s: %s' % (backend, 'PASS' if passed else 'FAIL'))
        failed = failed or not passed
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
