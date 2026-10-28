#!/usr/bin/env python3
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
"""Run Metal mip upload and memory-budget regressions with the graphics CI policy."""
import argparse
from contextlib import nullcontext
from pathlib import Path
import re
import subprocess
import sys


CASES = (
    ('default-workers', ('texture-upload-batch',)),
    ('default-no-workers', ('texture-upload-batch', 'upload-no-workers')),
    ('small-workers', ('texture-upload-batch', 'upload-small-budget')),
    ('small-no-workers', ('texture-upload-batch', 'upload-small-budget', 'upload-no-workers')),
    ('shutdown', ('issue-12878',)),
)
RESULT_KEY = 'GRAPHICS_TEXTURE_UPLOAD_RESULT'
ERROR = re.compile(r'ERROR:|FATAL:|Validation Error|WARNING: ThreadSanitizer', re.IGNORECASE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--target-platform', required=True)
    parser.add_argument('--available', nargs='*', default=[])
    parser.add_argument('--skip-all')
    args = parser.parse_args(argv)
    if args.skip_all or 'metal' not in args.available:
        print('Texture upload: SKIP: ' + (args.skip_all or 'Metal not built'))
        return 0
    session = nullcontext()
    if args.target_platform == 'arm64_sim-ios':
        from graphics_capture_simulator import simulator_capture
        session = simulator_capture(args.executable.resolve())
    failed = False
    with session as launcher:
        for name, arguments in CASES:
            command = [str(args.executable.resolve()), 'metal', *arguments]
            options = dict(stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=60, text=True, errors='replace')
            try:
                if launcher:
                    process = launcher.run(command, result_key=RESULT_KEY, **options)
                else:
                    process = subprocess.run(command, **options)
                print(process.stdout, end='')
                results = re.findall(r'^' + RESULT_KEY + r'=(-?\d+)$', process.stdout, re.MULTILINE)
                passed = process.returncode == 0 and results == ['0'] and not ERROR.search(process.stdout)
            except (OSError, subprocess.TimeoutExpired) as error:
                if isinstance(error, subprocess.TimeoutExpired) and error.stdout:
                    print(error.stdout.decode(errors='replace') if isinstance(error.stdout, bytes) else error.stdout)
                print(error)
                passed = False
            print('Texture upload %s: %s' % (name, 'PASS' if passed else 'FAIL'))
            failed = failed or not passed
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
