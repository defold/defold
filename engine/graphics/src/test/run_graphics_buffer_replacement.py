#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0 (https://www.defold.com/license).
"""Run buffer replacement and graphics context restore regressions using the graphics CI platform policy."""
import argparse
from contextlib import nullcontext
from pathlib import Path
import re
import subprocess
import sys

MODES = (('buffer-replacement', ('metal', 'opengl')),
         ('uniform-buffer-restore', ('opengl',)),
         ('render-target-restore', ('opengl',)))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--target-platform', required=True)
    parser.add_argument('--available', nargs='*', default=[])
    parser.add_argument('--skip-all')
    args = parser.parse_args(argv)
    tests = [(mode, backend) for mode, backends in MODES for backend in backends if backend in args.available]
    if args.skip_all or not tests:
        print('Buffer replacement: SKIP: ' + (args.skip_all or 'Metal/OpenGL not built'))
        return 0
    session = nullcontext()
    if args.target_platform == 'arm64_sim-ios':
        from graphics_capture_simulator import simulator_capture
        session = simulator_capture(args.executable.resolve())
    failed = False
    with session as launcher:
        for mode, backend in tests:
            command = [str(args.executable.resolve()), backend, mode]
            options = dict(stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=60, text=True, errors='replace')
            try:
                if launcher:
                    process = launcher.run(command, result_key='GRAPHICS_BUFFER_REPLACEMENT_RESULT', **options)
                else:
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
            print('%s %s: %s' % (mode, backend, 'PASS' if passed else 'FAIL'))
            failed = failed or not passed
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
