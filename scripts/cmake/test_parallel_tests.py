# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


MODULES = Path(__file__).resolve().parent


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class ParallelTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='parallel-tests-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.build = self.root / 'build'
        (self.root / 'worker.py').write_text('''
from pathlib import Path
import json
import sys
import time
name, group, barrier = sys.argv[1:]
started = time.monotonic_ns()
owner = Path(group + '.owner')
with owner.open('x') as file:
    file.write(name)
try:
    Path(name + '.started').touch()
    if barrier == 'ON':
        deadline = time.monotonic() + 10
        while len(list(Path('.').glob('*.started'))) < 2:
            if time.monotonic() > deadline:
                raise RuntimeError('Independent test groups did not overlap')
            time.sleep(0.01)
    time.sleep(0.15)
    Path(name + '.json').write_text(json.dumps({'group': group,
        'start': started, 'end': time.monotonic_ns()}))
finally:
    owner.unlink()
''')
        (self.root / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 4.0)
project(ParallelTestCommands NONE)
macro(defold_log)
endmacro()
set(TARGET_PLATFORM "${{FIXTURE_PLATFORM}}")
include("{MODULES.as_posix()}/functions_test.cmake")
foreach(name IN LISTS TEST_NAMES)
  if(SHARED)
    set(group "")
    set(owner shared)
  else()
    set(group "${{name}}")
    set(owner "${{name}}")
  endif()
  defold_test_run_settings(runner options "${{group}}")
  add_custom_target(${{name}} ALL
    COMMAND ${{runner}} "{Path(sys.executable).as_posix()}" "${{CMAKE_CURRENT_SOURCE_DIR}}/worker.py"
      "${{name}}" "${{owner}}" "${{BARRIER}}"
    ${{options}} VERBATIM)
endforeach()
''')

    def configure(self, names, *, shared=False, barrier=False, jobs=2, platform='arm64-macos'):
        result = subprocess.run(['cmake', '-G', 'Ninja', '-S', str(self.root), '-B', str(self.build),
                                 '-DTEST_NAMES=' + ';'.join(names), '-DFIXTURE_PLATFORM=' + platform,
                                 '-DSHARED=' + ('ON' if shared else 'OFF'),
                                 '-DBARRIER=' + ('ON' if barrier else 'OFF'),
                                 '-DDEFOLD_TEST_JOBS=' + str(jobs)], capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def build_all(self):
        result = subprocess.run(['cmake', '--build', str(self.build), '--parallel', '8'],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return [json.loads(path.read_text()) for path in self.build.glob('*.json')]

    def peak(self, runs):
        active = peak = 0
        for _, change in sorted(event for run in runs for event in ((run['start'], 1), (run['end'], -1))):
            active += change
            peak = max(peak, active)
        return peak

    # Independent resource groups must actually overlap, guarding against the Ninja console-pool regression.
    def test_independent_groups_overlap(self):
        self.configure(['first', 'second'], barrier=True)
        self.assertEqual(2, self.peak(self.build_all()))

    # Unclassified tests must retain mutual exclusion even when Ninja has spare test workers.
    def test_shared_group_is_serialized(self):
        self.configure(['first', 'second', 'third'], shared=True)
        self.assertEqual(1, self.peak(self.build_all()))

    # The native test pool must cap all groups together regardless of the outer Ninja worker count.
    def test_global_pool_limits_concurrent_groups(self):
        self.configure(['first', 'second', 'third', 'fourth'])
        self.assertEqual(2, self.peak(self.build_all()))

    # Setting one test worker must restore serial execution across otherwise independent groups.
    def test_one_worker_serializes_all_groups(self):
        self.configure(['first', 'second'], jobs=1)
        self.assertEqual(1, self.peak(self.build_all()))

    # Device test commands must retain terminal-pool serialization and avoid desktop locking wrappers.
    def test_device_commands_keep_existing_scheduling(self):
        self.configure(['first', 'second'], platform='arm64-android')
        self.assertEqual(1, self.peak(self.build_all()))
        self.assertNotIn('run_test_locked.py', (self.build / 'build.ninja').read_text())

    # A failed child must propagate its status and release the persistent lock for the next invocation.
    def test_failure_propagates_and_releases_lock(self):
        command = [sys.executable, str(MODULES / 'run_test_locked.py'),
                   '--lock', str(self.root / 'resource.lock'), '--', sys.executable, '-c']
        failed = subprocess.run(command + ['raise SystemExit(7)'], timeout=10)
        self.assertEqual(7, failed.returncode)
        passed = subprocess.run(command + ['pass'], timeout=10)
        self.assertEqual(0, passed.returncode)

    # OS locks must be released even when an owner exits without running Python cleanup handlers.
    def test_crashed_owner_does_not_leave_a_stale_lock(self):
        lock = self.root / 'resource.lock'
        script = (f'import sys; sys.path.insert(0, {str(MODULES)!r}); '
                  'import os; from pathlib import Path; from run_test_locked import resource_lock\n'
                  f'with resource_lock(Path({str(lock)!r})):\n    os._exit(7)\n')
        crashed = subprocess.run([sys.executable, '-c', script], timeout=10)
        self.assertEqual(7, crashed.returncode)
        passed = subprocess.run([sys.executable, str(MODULES / 'run_test_locked.py'),
                                 '--lock', str(lock), '--', sys.executable, '-c', 'pass'], timeout=10)
        self.assertEqual(0, passed.returncode)


if __name__ == '__main__':
    unittest.main()
