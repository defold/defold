import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class MemorySanitizerConfigurationTests(unittest.TestCase):
    def configure(self, root, **options):
        repository = Path(__file__).resolve().parent.parent
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(sanitizer_configuration LANGUAGES C)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_library(defold_sdk INTERFACE)
include(features)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/instrumented.c" "int main(void) { return 0; }\\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/excluded.c" "int main(void) { return 0; }\\n")
add_executable(instrumented "${CMAKE_CURRENT_BINARY_DIR}/instrumented.c")
target_link_libraries(instrumented PRIVATE defold_sdk)
add_executable(excluded "${CMAKE_CURRENT_BINARY_DIR}/excluded.c")
target_link_libraries(excluded PRIVATE defold_sdk)
defold_target_skip_sanitizer(excluded)
''')
        return subprocess.run([
            'cmake', '-S', str(root), '-B', str(root / 'build'), '-G', 'Ninja',
            '-DDEFOLD_HOME=' + str(repository),
            *[f'-D{name}={value}' for name, value in options.items()],
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    # MSan must instrument ordinary targets, but host libraries that skip sanitizers remain usable.
    def test_instrumentation_and_exclusion(self):
        for platform in ('x86_64-linux', 'arm64-linux'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result = self.configure(root, TARGET_PLATFORM=platform, WITH_MSAN='ON')
                self.assertEqual(0, result.returncode, result.stdout)
                commands = subprocess.check_output(
                    ['ninja', '-C', str(root / 'build'), '-t', 'commands'], text=True).splitlines()
                instrumented = [line for line in commands if 'instrumented.c.o' in line]
                excluded = [line for line in commands if 'excluded.c.o' in line]
                self.assertEqual(2, len(instrumented), commands)
                self.assertEqual(2, len(excluded), commands)
                for line in instrumented:
                    self.assertIn('-fsanitize=memory', line)
                    self.assertIn('-fsanitize-memory-track-origins=2', line)
                    self.assertIn('-fno-omit-frame-pointer', line)
                for line in excluded:
                    self.assertNotIn('-fsanitize=memory', line)

    # Reject unsupported platforms and conflicting sanitizers rather than silently choosing one.
    def test_rejects_unsupported_configurations(self):
        cases = [({'TARGET_PLATFORM': 'arm64-macos'}, 'requires TARGET_PLATFORM')]
        cases += [({'TARGET_PLATFORM': 'arm64-linux', sanitizer: 'ON'}, 'cannot be combined')
                  for sanitizer in ('WITH_ASAN', 'WITH_HWASAN', 'WITH_UBSAN', 'WITH_TSAN')]
        for options, error in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                result = self.configure(Path(directory), WITH_MSAN='ON', **options)
                self.assertNotEqual(0, result.returncode, result.stdout)
                self.assertIn(error, result.stdout)


if __name__ == '__main__':
    unittest.main()
