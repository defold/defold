# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CMAKE_MODULES = Path(__file__).resolve().parent


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class RuntimeDependenciesTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='test-runtime-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.build = self.root / 'build'
        self.write('input.txt', 'ready')
        self.write('generated.h.in', '#define EXPECTED \'r\'\n')
        self.write('test.c', '''
#include "generated.h"
#include <stdio.h>
int main(void) {
    FILE* file = fopen("content.txt", "r");
    if (!file) return 1;
    int value = fgetc(file);
    fclose(file);
    if (value != EXPECTED) return 2;
    file = fopen("ran.txt", "w");
    if (!file) return 3;
    fclose(file);
    return 0;
}
''')
        self.write('content.py', '''
from pathlib import Path
import shutil
import time
deadline = time.monotonic() + 10
while not Path('compiled.txt').exists():
    if time.monotonic() > deadline:
        raise RuntimeError('Compilation waited for runtime content')
    time.sleep(0.01)
shutil.copyfile('../input.txt', 'content.txt')
''')
        self.write('wrappers/testserver.py', '''
import os
import subprocess
import sys
os.chdir(sys.argv[sys.argv.index('--workdir') + 1])
for exe in sys.argv[sys.argv.index('--') + 1:]:
    subprocess.run([exe], check=True)
''')
        self.write('build_tools/build_android.py', '''
from pathlib import Path
import os
import subprocess
import sys
os.chdir(sys.argv[sys.argv.index('--cwd') + 1])
if sys.argv[1] == 'prepare':
    assert Path('content.txt').read_text() == 'ready', 'Staged before content was ready'
    Path('prepared.txt').touch()
elif sys.argv[1] == 'run-test':
    assert Path('prepared.txt').exists()
    subprocess.run([sys.argv[sys.argv.index('--program') + 1]], check=True)
''')
        self.write('scripts/cmake/run_interactive.py', (CMAKE_MODULES / 'run_interactive.py').read_text())
        self.write('CMakeLists.txt', f'''
cmake_minimum_required(VERSION 4.0)
project(RuntimeDependencies C)
macro(defold_log)
endmacro()
macro(_defold_force_load_ios_testmain)
endmacro()
set(DEFOLD_HOME "${{CMAKE_CURRENT_SOURCE_DIR}}")
set(DEFOLD_SDK_ROOT "${{CMAKE_CURRENT_SOURCE_DIR}}/sdk")
set(DEFOLD_CMAKE_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}/wrappers")
set(TARGET_PLATFORM "${{FIXTURE_PLATFORM}}")
include("{CMAKE_MODULES.as_posix()}/functions_test.cmake")
include("{CMAKE_MODULES.as_posix()}/functions_testserver.cmake")
add_custom_target(build_tests)
add_custom_command(OUTPUT generated.h
  COMMAND "${{CMAKE_COMMAND}}" -E copy "${{CMAKE_CURRENT_SOURCE_DIR}}/generated.h.in" generated.h
  DEPENDS generated.h.in)
add_executable(unit test.c "${{CMAKE_CURRENT_BINARY_DIR}}/generated.h")
target_include_directories(unit PRIVATE "${{CMAKE_CURRENT_BINARY_DIR}}")
add_custom_command(TARGET unit POST_BUILD
  COMMAND "${{CMAKE_COMMAND}}" -E touch compiled.txt)
add_custom_command(OUTPUT content.txt
  COMMAND "{Path(sys.executable).as_posix()}" "${{CMAKE_CURRENT_SOURCE_DIR}}/content.py"
  DEPENDS input.txt)
add_custom_target(content DEPENDS content.txt)
defold_register_test_target(unit ON "${{CMAKE_CURRENT_BINARY_DIR}}"
  RUNTIME_DEPENDS content)
if(NOT TARGET_PLATFORM MATCHES "android$")
  defold_register_test_with_server(unit "${{TARGET_PLATFORM}}" WORKDIR "${{CMAKE_CURRENT_BINARY_DIR}}")
  defold_register_tests_with_server(units "${{TARGET_PLATFORM}}"
    TARGETS unit WORKDIR "${{CMAKE_CURRENT_BINARY_DIR}}")
  defold_finalize_sequential_run_tests()
endif()
''')

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def command(self, *args):
        result = subprocess.run(args, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def configure(self, platform='arm64-macos'):
        self.command('cmake', '-G', 'Ninja', '-S', str(self.root), '-B', str(self.build),
                     '-DFIXTURE_PLATFORM=' + platform)

    def build_target(self, target):
        self.command('cmake', '--build', str(self.build), '--target', target, '--parallel', '4')

    # A binary needs generated headers but runtime content must not block its compilation.
    def test_binary_build_does_not_generate_runtime_content(self):
        self.configure()
        self.build_target('unit')
        self.assertTrue((self.build / 'generated.h').is_file())
        self.assertTrue((self.build / 'compiled.txt').is_file())
        self.assertFalse((self.build / 'content.txt').exists())
        self.build_target('run_unit')
        self.assertTrue((self.build / 'ran.txt').is_file())

    # Aggregate builds must generate current runtime assets alongside compilation, without running tests.
    def test_build_tests_prepares_content_without_a_compile_barrier(self):
        self.configure()
        self.build_target('build_tests')
        self.assertEqual('ready', (self.build / 'content.txt').read_text())
        self.assertFalse((self.build / 'ran.txt').exists())
        (self.build / 'content.txt').unlink()
        self.write('input.txt', 'refreshed')
        self.build_target('build_tests')
        self.assertEqual('refreshed', (self.build / 'content.txt').read_text())

    # Direct, server-backed, grouped and sequential runners must all rebuild missing runtime assets.
    def test_all_native_runners_wait_for_runtime_content(self):
        self.configure()
        for target in ('run_unit', 'run_unit_server', 'run_units_server', 'run_tests_sequential'):
            with self.subTest(target=target):
                (self.build / 'content.txt').unlink(missing_ok=True)
                (self.build / 'ran.txt').unlink(missing_ok=True)
                self.build_target(target)
                self.assertTrue((self.build / 'ran.txt').is_file())

    # Android batch preparation must wait for runtime data before staging it onto the device.
    def test_android_batch_stages_after_content_is_ready(self):
        self.configure('x86_64-android')
        self.build_target('run_tests')
        self.assertTrue((self.build / 'prepared.txt').is_file())
        self.assertTrue((self.build / 'ran.txt').is_file())


if __name__ == '__main__':
    unittest.main()
