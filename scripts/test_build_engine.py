# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import json
import tempfile
import unittest
from pathlib import Path

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


if __name__ == '__main__':
    unittest.main()
