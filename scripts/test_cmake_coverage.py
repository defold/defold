import contextlib
import io
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class CoverageConfigurationTests(unittest.TestCase):
    def configure(self, root, **options):
        repository = Path(__file__).resolve().parent.parent
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(coverage_configuration LANGUAGES C CXX)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_library(defold_sdk INTERFACE)
include(features)
include(defold_post_project)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/library.c" "int library(void) { return 0; }\\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.c" "int library(void);\\nint main(void) { return library(); }\\n")
defold_add_library(instrumented STATIC "${CMAKE_CURRENT_BINARY_DIR}/library.c")
defold_add_library(excluded STATIC "${CMAKE_CURRENT_BINARY_DIR}/library.c")
defold_target_skip_sanitizer(excluded)
add_executable(test_instrumented "${CMAKE_CURRENT_BINARY_DIR}/main.c")
target_link_libraries(test_instrumented PRIVATE defold_sdk instrumented)
add_custom_target(build_tests)
defold_register_test_target(test_instrumented OFF)
''')
        return subprocess.run([
            'cmake', '-S', str(root), '-B', str(root / 'build'), '-G', 'Ninja',
            '-DDEFOLD_HOME=' + str(repository),
            *[f'-D{name}={value}' for name, value in options.items()],
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    # Coverage must instrument ordinary targets, embed the profile path, skip the
    # JNI-style libraries that opt out of sanitizers, and list objects for the report.
    def test_instrumentation_and_object_list(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            platform = 'arm64-macos' if sys.platform == 'darwin' else 'x86_64-linux'
            result = self.configure(root, TARGET_PLATFORM=platform, WITH_COVERAGE='ON')
            self.assertEqual(0, result.returncode, result.stdout)
            build = root / 'build'
            commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'test_instrumented'],
                                               text=True).splitlines()
            profile = '-fprofile-instr-generate=%s/coverage/profraw/%%m-%%p.profraw' % build.resolve().as_posix()
            instrumented = [line for line in commands if 'instrumented.dir' in line and line.endswith('.c')]
            self.assertTrue(instrumented, commands)
            for line in instrumented:
                self.assertIn('-fcoverage-mapping', line)
                self.assertTrue(profile in line or profile.replace(str(build.resolve()), str(build)) in line, line)
            link = commands[-1]
            self.assertIn('-fprofile-instr-generate=', link)
            excluded = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'excluded'],
                                               text=True)
            self.assertNotIn('-fcoverage-mapping', excluded)
            self.assertNotIn('-fprofile-instr-generate', excluded)

            objects = (build / 'defold_coverage_objects_.txt').read_text().splitlines()
            self.assertTrue(objects[0].endswith('/test_instrumented'), objects)
            self.assertTrue(any('/instrumented.dir/' in line and line.endswith('.o') for line in objects), objects)
            self.assertFalse(any('/excluded.dir/' in line for line in objects), objects)

    # Without the option, builds must not change and no object list is written.
    def test_disabled_by_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = self.configure(root)
            self.assertEqual(0, result.returncode, result.stdout)
            commands = subprocess.check_output(['ninja', '-C', str(root / 'build'), '-t', 'commands'], text=True)
            self.assertNotIn('-fprofile-instr-generate', commands)
            self.assertEqual([], list((root / 'build').glob('defold_coverage_objects_*.txt')))

    # Reject combinations whose results would be misleading instead of silently picking one.
    def test_rejects_unsupported_configurations(self):
        cases = [({'TARGET_PLATFORM': 'x86_64-win32'}, 'requires a macOS or Linux')]
        cases += [({'TARGET_PLATFORM': 'x86_64-linux', sanitizer: 'ON'}, 'cannot be combined with sanitizers')
                  for sanitizer in ('WITH_ASAN', 'WITH_UBSAN', 'WITH_TSAN')]
        for options, error in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                result = self.configure(Path(directory), WITH_COVERAGE='ON', **options)
                self.assertNotEqual(0, result.returncode, result.stdout)
                self.assertIn(error, result.stdout)


class KeepGoingRunnerTests(unittest.TestCase):
    # Coverage runs keep dispatching after a failure so later tests still write profiles.
    def test_keep_going_runs_all_tests_and_fails(self):
        with mock.patch.object(sys, 'path', sys.path + [str(Path(__file__).parent / 'cmake')]):
            import run_tests as runner

        tests = [{'name': 'run_fail', 'group': 'a', 'priority': 2},
                 {'name': 'run_after', 'group': 'b', 'priority': 1}]
        for keep_going, expected in ((False, ['run_fail']), (True, ['run_fail', 'run_after'])):
            with self.subTest(keep_going=keep_going):
                started = []

                def run_test(test, lock_dir):
                    started.append(test['name'])
                    return (1 if test['name'] == 'run_fail' else 0), b'', 0.0

                with contextlib.redirect_stdout(io.TextIOWrapper(io.BytesIO(), write_through=True)), \
                        mock.patch.object(runner, 'run_test', run_test):
                    retcode = runner.run_tests(tests, 1, Path('unused-locks'), keep_going)
                self.assertEqual(1, retcode)
                self.assertEqual(expected, started)


if __name__ == '__main__':
    unittest.main()
