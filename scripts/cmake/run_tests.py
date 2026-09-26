# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

"""Run ready tests without occupying workers with queued resource-group locks."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from pathlib import Path

from run_test_locked import resource_lock


def run_test(test, lock_dir):
    start = time.monotonic()
    lock = lock_dir / (hashlib.sha256(test['group'].encode('utf-8')).hexdigest() + '.lock')
    try:
        # Preserve exclusion with individually requested run_* targets too.
        with resource_lock(lock):
            result = subprocess.run(test['command'], cwd=test['cwd'],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return result.returncode, result.stdout, time.monotonic() - start
    except OSError as error:
        return 1, (str(error) + '\n').encode('utf-8'), time.monotonic() - start


def run_tests(tests, jobs, lock_dir):
    pending = sorted(tests, key=lambda test: -test['priority'])
    running = {}
    groups = set()
    completed = 0
    failed = False
    with ThreadPoolExecutor(max_workers=jobs) as workers:
        while pending or running:
            while pending and len(running) < jobs and not failed:
                ready = next((i for i, test in enumerate(pending) if test['group'] not in groups), None)
                if ready is None:
                    break
                test = pending.pop(ready)
                groups.add(test['group'])
                print('Starting %s (group %s)' % (test['name'], test['group']), flush=True)
                running[workers.submit(run_test, test, lock_dir)] = test
            if not running:
                break
            done, _ = wait(running, return_when=FIRST_COMPLETED)
            for future in done:
                test = running.pop(future)
                groups.remove(test['group'])
                code, output, seconds = future.result()
                completed += 1
                print('[%d/%d] %s: %s (%.2fs)' %
                      (completed, len(tests), test['name'], 'FAILED' if code else 'PASSED', seconds), flush=True)
                sys.stdout.buffer.write(output)
                sys.stdout.buffer.flush()
                failed |= code != 0
            # Let already running tests finish and report, but stop dispatching
            # after a failure, matching Ninja's default failure behavior.
            if failed:
                pending.clear()
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--jobs', type=int, required=True)
    parser.add_argument('--lock-dir', type=Path, required=True)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    with args.manifest.open(encoding='utf-8') as manifest:
        tests = json.load(manifest)
    return run_tests(tests, args.jobs, args.lock_dir)


if __name__ == '__main__':
    raise SystemExit(main())
