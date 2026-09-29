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

import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build


class CompilationDatabaseTests(unittest.TestCase):
    def setUp(self):
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        self.root = Path(temporary_directory.name)
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(self.root)
        self.configuration.host = 'arm64-macos'
        self.configuration._log = lambda message: None

    def write_database(self, platform, commands):
        path = self.root / 'engine' / 'build' / platform / 'compile_commands.json'
        path.parent.mkdir(parents=True)
        path.write_text(json.dumps(commands))

    def read_global_database(self):
        self.configuration.generate_global_compile_commands_json()
        return json.loads((self.root / 'compile_commands.json').read_text())

    def test_native_build_includes_each_command_once(self):
        self.configuration.target_platform = 'arm64-macos'
        commands = [{
            'directory': str(self.root),
            'file': 'engine/dlib/src/hash.cpp',
            'command': 'clang++ -arch arm64 -c engine/dlib/src/hash.cpp',
        }]
        self.write_database('arm64-macos', commands)
        self.assertEqual(commands, self.read_global_database())

    def test_cross_build_includes_host_and_target_commands(self):
        self.configuration.target_platform = 'arm64-android'
        host_commands = [{
            'directory': str(self.root),
            'file': 'engine/dlib/src/hash.cpp',
            'command': 'clang++ -arch arm64 -c engine/dlib/src/hash.cpp',
        }]
        target_commands = [{
            'directory': str(self.root),
            'file': 'engine/dlib/src/hash.cpp',
            'command': 'aarch64-linux-android-clang++ -c engine/dlib/src/hash.cpp',
        }]
        self.write_database('arm64-macos', host_commands)
        self.write_database('arm64-android', target_commands)
        self.write_database('wasm-web', [{'file': 'unrelated.cpp'}])
        self.assertEqual(host_commands + target_commands, self.read_global_database())


class WasmTestSupportTests(unittest.TestCase):
    def setUp(self):
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.host = 'x86_64-linux'
        self.configuration.target_platform = 'wasm-web'
        self.configuration.skip_tests = False
        self.configuration._log = mock.Mock()
        runner = build.wasm_runner.WasmRunner('node', '/test/node')
        self.configuration._find_wasm_test_runner = mock.Mock(return_value=(runner, []))

    # Linux CI must run non-threaded WASM tests instead of silently treating them as unsupported.
    def test_linux_runs_non_threaded_wasm_with_node(self):
        self.assertTrue(self.configuration._can_run_tests())
        self.configuration._find_wasm_test_runner.assert_called_once_with()

    # An explicit --skip-tests must still bypass WASM runner discovery.
    def test_linux_respects_skip_tests(self):
        self.configuration.skip_tests = True
        self.assertFalse(self.configuration._can_run_tests())
        self.configuration._find_wasm_test_runner.assert_not_called()

    # A missing JavaScript runtime must fail the build rather than silently skip the suite.
    def test_linux_fails_when_wasm_runner_is_missing(self):
        self.configuration._find_wasm_test_runner.return_value = (None, ['node is unavailable'])
        with self.assertRaises(SystemExit):
            self.configuration._can_run_tests()
        self.configuration._find_wasm_test_runner.assert_called_once_with()

    # Enabling non-threaded WASM on Linux must leave threaded test support unchanged.
    def test_linux_keeps_threaded_wasm_disabled(self):
        self.configuration.target_platform = 'wasm_pthread-web'
        self.assertFalse(self.configuration._can_run_tests())
        self.configuration._find_wasm_test_runner.assert_not_called()


if __name__ == '__main__':
    unittest.main()
