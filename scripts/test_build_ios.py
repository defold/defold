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

import contextlib
import io
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'build_tools'))
import build_ios


class IOSSimulatorResultTests(unittest.TestCase):
    def launch(self, output, returncode=0):
        command_runner = mock.Mock(return_value=subprocess.CompletedProcess([], returncode, stdout=output))
        runner = build_ios.IOSSimulatorTestRunner(env={}, command_runner=command_runner)
        simulator = build_ios.IOSSimulator('simulator-id', 'Test Simulator', 'runtime', 'Booted', {})
        with contextlib.redirect_stdout(io.StringIO()) as captured:
            result = runner._launch_app(simulator, 'com.defold.tests.fixture', 'font', None)
        self.assertEqual(subprocess.PIPE, command_runner.call_args.kwargs['stdout'])
        self.assertEqual(subprocess.STDOUT, command_runner.call_args.kwargs['stderr'])
        return result, captured.getvalue()

    # Accept and preserve colored C++ results while allowing platform-specific skips.
    def test_colored_cpp_result_passes(self):
        output = b'4 tests \x1b[32mPASSED\x1b[0m and 1 skipped\n'
        result, captured = self.launch(output)
        self.assertEqual(0, result)
        self.assertEqual(output.decode(), captured)

    # Accept the distinct success footer emitted by the C-only jc_test runner.
    def test_c_result_passes(self):
        result, _ = self.launch(b'4 tests passed, 0 skipped and 0 tests failed\n')
        self.assertEqual(0, result)

    # A successful simctl launch must not hide a failing test application.
    def test_failed_test_returns_failure_after_successful_launch(self):
        result, _ = self.launch(b'3 tests passed, 0 skipped and 1 tests \x1b[31mFAILED\x1b[0m\n')
        self.assertEqual(1, result)

    # A later successful suite must not mask a previous failure in the same application.
    def test_earlier_failure_is_preserved(self):
        result, _ = self.launch(b'3 tests passed, 0 skipped and 1 tests FAILED\n4 tests PASSED and 0 skipped\n')
        self.assertEqual(1, result)

    # Missing final results indicate an interrupted/crashed app, not a passing suite.
    def test_missing_result_fails(self):
        with self.assertRaisesRegex(build_ios.IOSTestError, 'without a completed jc_test result'):
            self.launch(b'Fixture.FirstCase PASS\n')

    # Preserve launch errors even when partial output contains a successful result.
    def test_launcher_error_is_preserved(self):
        result, _ = self.launch(b'4 tests PASSED and 0 skipped\n', returncode=9)
        self.assertEqual(9, result)


if __name__ == '__main__':
    unittest.main()
