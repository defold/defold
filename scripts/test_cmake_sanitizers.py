import shutil
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class MemorySanitizerConfigurationTests(unittest.TestCase):
    def configure(self, root, extra_cmake='', **options):
        repository = Path(__file__).resolve().parent.parent
        runtime = root / 'runtime with spaces'
        (runtime / 'lib').mkdir(parents=True, exist_ok=True)
        (runtime / 'lib/libc++.a').touch()
        (runtime / 'lib/libc++abi.a').touch()
        options.setdefault('DEFOLD_MSAN_RUNTIME', runtime)
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(sanitizer_configuration LANGUAGES C CXX)
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
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cpp.cpp" "int main() { return 0; }\\n")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sdk" "${DEFOLD_MSAN_PREFIX}/lib/${TARGET_PLATFORM}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sdk/libdependency.a" "")
file(WRITE "${DEFOLD_MSAN_PREFIX}/lib/${TARGET_PLATFORM}/libdependency.a" "")
defold_import_external(dependency "${CMAKE_CURRENT_BINARY_DIR}/sdk/libdependency.a")
add_executable(instrumented_cpp "${CMAKE_CURRENT_BINARY_DIR}/cpp.cpp")
target_link_libraries(instrumented_cpp PRIVATE defold_sdk dependency)
add_executable(excluded_cpp "${CMAKE_CURRENT_BINARY_DIR}/cpp.cpp")
target_link_libraries(excluded_cpp PRIVATE defold_sdk dependency)
defold_target_skip_sanitizer(excluded_cpp)
''' + extra_cmake)
        return subprocess.run([
            'cmake', '-S', str(root), '-B', str(root / 'build'), '-G', 'Ninja',
            '-DDEFOLD_HOME=' + str(repository),
            '-DDEFOLD_MSAN_PREFIX=' + str(root / 'msan'),
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
                cpp = [line for line in commands if 'instrumented_cpp.dir' in line]
                excluded_cpp = [line for line in commands if 'excluded_cpp.dir' in line]
                self.assertTrue(any('-nostdinc++' in line for line in cpp))
                self.assertTrue(any(str(root / 'runtime with spaces/include/c++/v1') in shlex.split(line) for line in cpp))
                self.assertTrue(any(str(root / 'msan') + '/lib/' in line and 'libdependency.a' in line for line in cpp))
                for line in excluded_cpp:
                    self.assertNotIn('-nostdinc++', line)
                    self.assertNotIn('-fsanitize=memory', line)
                    self.assertNotIn('libc++.a', line)
                    self.assertNotIn(str(root / 'msan') + '/lib/', line)

    # Reusing a build must switch all DDF package archives when MSan is enabled or disabled.
    def test_ddf_packages_follow_msan_when_reconfiguring(self):
        packages = (
            ('absl', 'abslConfig.cmake', 'absl::base', 'libabsl_base.a'),
            ('utf8_range', 'utf8_rangeConfig.cmake', 'utf8_range::utf8_validity', 'libutf8_validity.a'),
            ('protobuf', 'protobuf-config.cmake', 'protobuf::libprotobuf', 'libprotobuf.a'),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for prefix in ('sdk', 'msan'):
                library_dir = root / prefix / 'lib/x86_64-linux'
                for package, config, target, archive in packages:
                    package_dir = library_dir / 'cmake' / package
                    package_dir.mkdir(parents=True)
                    (library_dir / archive).touch()
                    (package_dir / config).write_text(
                        f'add_library({target} STATIC IMPORTED)\n'
                        f'set_property(TARGET {target} PROPERTY IMPORTED_LOCATION "{library_dir / archive}")\n')
                with (library_dir / 'cmake/protobuf/protobuf-config.cmake').open('a') as config:
                    config.write('set_property(TARGET protobuf::libprotobuf PROPERTY INTERFACE_LINK_LIBRARIES '
                                 '"absl::base;utf8_range::utf8_validity")\n')
                (library_dir / 'cmake/protobuf/protobuf-config-version.cmake').write_text(
                    'set(PACKAGE_VERSION "35.1.0")\n'
                    'set(PACKAGE_VERSION_EXACT TRUE)\n'
                    'set(PACKAGE_VERSION_COMPATIBLE TRUE)\n')

            # Exercise the real DDF CMake file while replacing unrelated code generation and test setup.
            extra_cmake = '''
set(DEFOLD_EXT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/${TARGET_PLATFORM}")
if(WITH_MSAN)
  set(DEFOLD_EXT_LIB_DIR "${DEFOLD_MSAN_PREFIX}/lib/${TARGET_PLATFORM}")
endif()
foreach(command IN ITEMS ddf_protoc_gen_cpp defold_protoc_gen_protobuf_cpp
    defold_target_link_libraries defold_target_link_socket
    defold_target_link_libraries_web defold_register_test_target)
  function(${command})
  endfunction()
endforeach()
function(defold_add_executable target)
  add_executable(${target} "${CMAKE_BINARY_DIR}/cpp.cpp")
endfunction()
add_subdirectory("${DEFOLD_HOME}/engine/ddf/src/test" ddf)
'''
            for enabled, prefix in (('OFF', 'sdk'), ('ON', 'msan'), ('OFF', 'sdk')):
                with self.subTest(WITH_MSAN=enabled):
                    result = self.configure(root, extra_cmake=extra_cmake,
                                            TARGET_PLATFORM='x86_64-linux', WITH_MSAN=enabled)
                    self.assertEqual(0, result.returncode, result.stdout)
                    commands = subprocess.check_output(
                        ['ninja', '-C', str(root / 'build'), '-t', 'commands', 'test_ddf'], text=True)
                    link_command = shlex.split(commands.splitlines()[-1])
                    for _, _, _, archive in packages:
                        self.assertIn(str(root / prefix / 'lib/x86_64-linux' / archive), link_command)

    # A missing runtime must fail at configure time, before a misleading mixed-runtime test run.
    def test_requires_instrumented_runtime(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = self.configure(root, TARGET_PLATFORM='arm64-linux', WITH_MSAN='ON',
                                    DEFOLD_MSAN_RUNTIME=root / 'missing')
            self.assertNotEqual(0, result.returncode, result.stdout)
            self.assertIn('Missing MSan libc++', result.stdout)

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
