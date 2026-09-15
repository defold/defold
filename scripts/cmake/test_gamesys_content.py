# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


CONTENT_MODULE = Path(__file__).resolve().parents[2] / 'engine/gamesys/src/gamesys/test/test_content.cmake'
FOLDERS = ('first', 'second', 'third', 'fourth')


def fake_bob(control, arguments):
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path)
    parser.add_argument('--output')
    parser.add_argument('--settings', type=Path)
    args, _ = parser.parse_known_args(arguments)
    folder = Path(args.output).name
    started = time.monotonic_ns()
    metadata = args.root / '.internal/owner'
    metadata.parent.mkdir(parents=True, exist_ok=True)
    metadata.write_text(folder)

    # All project resources, including references outside this folder, must exist.
    for source_folder in FOLDERS:
        assert (args.root / source_folder / 'source.resource').is_file()
    assert args.settings.read_text() == '[project]\n'
    assert (args.root / 'builtins/graphics/default.texture_profiles').is_file()

    (control / (folder + '.started')).touch()
    deadline = time.monotonic() + 10
    while len(list(control.glob('*.started'))) < 2:
        if time.monotonic() > deadline:
            raise RuntimeError('Content builds are serialized')
        time.sleep(0.01)
    time.sleep(0.1)
    assert metadata.read_text() == folder, 'Bob project metadata was shared'

    output = args.root / args.output / folder / 'generated.resourcec'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(folder)
    (control / (folder + '.json')).write_text(json.dumps({
        'root': str(args.root),
        'tools': os.environ['DM_BOB_ROOTFOLDER'],
        'started': started,
        'finished': time.monotonic_ns(),
    }))


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class GamesysContentTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='gamesys-content-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'source'
        self.runtime = self.root / 'runtime'
        self.build = self.root / 'build'
        self.control = self.root / 'control'
        self.control.mkdir()
        self.write('source/test_data_folders.txt', '\n'.join(FOLDERS))
        self.write('source/common_build.inputs', '/shared.raw\n')
        self.write('source/shared.raw', 'shared raw data')
        self.write('content/builtins/graphics/default.texture_profiles', 'texture profiles')
        self.write('sdk/share/java/bob-light.jar', '')
        for folder in FOLDERS:
            self.write(f'source/{folder}/build.inputs', f'/{folder}/source.resource\n')
            self.write(f'source/{folder}/source.resource', folder)
            self.write(f'source/{folder}/game.project', '[project]\n')
            self.write(f'source/{folder}/fixture.prebuilt_scriptc', 'prebuilt ' + folder)
        self.write('CMakeLists.txt', f'''
cmake_minimum_required(VERSION 4.0)
project(GamesysContentTest NONE)
set(GS_TEST_ROOT "${{CMAKE_CURRENT_SOURCE_DIR}}/source")
set(GS_TEST_RUNTIME_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}/runtime")
set(GS_ENGINE_CONTENT_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}/content")
set(DEFOLD_SDK_ROOT "${{CMAKE_CURRENT_SOURCE_DIR}}/sdk")
set(TARGET_PLATFORM "test-platform")
set(Java_JAVA_EXECUTABLE "{Path(sys.executable).as_posix()}")
set(DEFOLD_JAVA_RUNTIME_FLAGS_LIST "{Path(__file__).resolve().as_posix()}" --fake-bob "${{CMAKE_CURRENT_SOURCE_DIR}}/control")
include("{CONTENT_MODULE.as_posix()}")
add_custom_target(content ALL DEPENDS ${{gamesys_content_outputs}})
''')
        self.env = os.environ.copy()
        self.env['DM_BOB_ROOTFOLDER'] = str(self.root / 'tools')
        self.run_command('cmake', '-S', str(self.root), '-B', str(self.build), '-G', 'Ninja')

    def write(self, relative_path, content):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def run_command(self, *command):
        result = subprocess.run(command, env=self.env, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def build_content(self):
        self.run_command('cmake', '--build', str(self.build), '--parallel', '8')

    # Verify actual parallel build execution, its memory limit, and isolated Bob state/output staging.
    def test_parallel_builds_preserve_content_and_isolate_projects(self):
        self.build_content()
        runs = [json.loads((self.control / (folder + '.json')).read_text()) for folder in FOLDERS]
        self.assertEqual(len(FOLDERS), len({run['root'] for run in runs}))
        self.assertEqual(len(FOLDERS), len({run['tools'] for run in runs}))
        events = sorted(event for run in runs for event in ((run['started'], 1), (run['finished'], -1)))
        active = peak = 0
        for _, change in events:
            active += change
            peak = max(peak, active)
        self.assertEqual(2, peak)
        for folder in FOLDERS:
            self.assertEqual(folder, (self.runtime / folder / folder / 'generated.resourcec').read_text())
            self.assertEqual('prebuilt ' + folder, (self.runtime / folder / folder / 'fixture.scriptc').read_text())
            self.assertEqual('shared raw data', (self.runtime / folder / 'shared.rawc').read_text())
        self.assertFalse((self.source / '.internal').exists())
        self.assertFalse((self.source / 'build').exists())

    # Rebuilding one folder must not trigger or overwrite other folders through a serial dependency chain.
    def test_rebuilding_one_folder_leaves_other_folders_untouched(self):
        self.build_content()
        before = {folder: (self.control / (folder + '.json')).read_text() for folder in FOLDERS}
        (self.build / '.bob/first.stamp').unlink()
        self.build_content()
        for folder in FOLDERS:
            after = (self.control / (folder + '.json')).read_text()
            if folder == 'first':
                self.assertNotEqual(before[folder], after)
            else:
                self.assertEqual(before[folder], after)
            self.assertEqual(folder, (self.runtime / folder / folder / 'generated.resourcec').read_text())
        completed = {folder: (self.control / (folder + '.json')).read_text() for folder in FOLDERS}
        self.build_content()
        self.assertEqual(completed, {folder: (self.control / (folder + '.json')).read_text() for folder in FOLDERS})


if __name__ == '__main__':
    if sys.argv[1:2] == ['--fake-bob']:
        fake_bob(Path(sys.argv[2]), sys.argv[3:])
    else:
        unittest.main()
