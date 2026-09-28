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

"""Run the C HTTP tests with dlib's Java test server and generated config."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def run_test(args):
    with tempfile.TemporaryDirectory(prefix='defold-test-http-') as directory:
        workdir = Path(directory)
        data_dir = workdir / 'src/test/data'
        data_dir.mkdir(parents=True)
        shutil.copyfile(args.keystore, data_dir / 'keystore')
        config = workdir / 'test_http_server.cfg'

        with (workdir / 'server.log').open('w+') as log:
            server = subprocess.Popen(
                [args.java, '-cp', args.classpath, 'TestHttpServer'],
                cwd=workdir, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 30
                while not config.exists():
                    if server.poll() is not None:
                        raise RuntimeError('HTTP test server exited before writing its config')
                    if time.monotonic() >= deadline:
                        raise RuntimeError('HTTP test server startup timed out')
                    time.sleep(0.1)

                result = subprocess.call(
                    [args.executable, str(config), *args.test_args], cwd=workdir)
                if result != 0:
                    log.seek(0)
                    sys.stderr.write(log.read())
                return result
            except (OSError, RuntimeError) as error:
                print(error, file=sys.stderr)
                log.seek(0)
                sys.stderr.write(log.read())
                return 1
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--java', required=True)
    parser.add_argument('--classpath', required=True)
    parser.add_argument('--keystore', required=True, type=Path)
    parser.add_argument('executable')
    parser.add_argument('test_args', nargs=argparse.REMAINDER)
    return run_test(parser.parse_args())


if __name__ == '__main__':
    sys.exit(main())
