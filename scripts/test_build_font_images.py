import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class FontImageConfigurationTests(unittest.TestCase):
    # Delegating the matrix must preserve font unit tests, manual targets, and default/nightly coverage.
    def test_image_matrix_selection(self):
        repository = Path(__file__).resolve().parent.parent
        cases = (
            ('x86_64-linux', True, '1', False, False),
            ('arm64-linux', True, '', False, True),
            ('x86_64-linux', True, '', False, True),
            ('x86_64-linux', False, '', False, True),
            ('arm64-macos', True, '', False, False),
            ('x86_64-linux', True, '', True, False),
        )
        for platform, ci, skip_images, msan, expected in cases:
            with self.subTest(platform=platform, ci=ci, skip_images=skip_images, msan=msan), \
                    tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                # Configure the real font test definitions without an SDK or compiling native code.
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(font_test_selection LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(FONT_RENDERER_IS_DESKTOP ON)
set(HOST_PLATFORM "${TARGET_PLATFORM}")
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
function(defold_log)
endfunction()
include(functions_test)
file(WRITE "${CMAKE_BINARY_DIR}/dummy.cpp" "int main() { return 0; }\\n")
add_library(fontc_shared SHARED "${CMAKE_BINARY_DIR}/dummy.cpp")
add_custom_target(font_renderer_jar)
add_custom_target(build_tests)
add_custom_target(run_tests)
foreach(command IN ITEMS defold_target_link_libraries defold_target_link_socket
    defold_target_link_graphics defold_target_link_platform defold_target_link_opengl
    defold_embed_to_header defold_create_exported_symbols_file target_sources)
  function(${command})
  endfunction()
endforeach()
function(defold_add_executable target)
  add_executable(${target} "${CMAKE_BINARY_DIR}/dummy.cpp")
endfunction()
function(defold_register_test_target target run)
  if(run)
    add_custom_target(run_${target} COMMAND "$<TARGET_FILE:${target}>")
    defold_add_to_run_tests(run_${target} RUN_GROUP font
      COMMAND "$<TARGET_FILE:${target}>" DEPENDS ${target})
  endif()
endfunction()
add_subdirectory("${DEFOLD_HOME}/engine/font/src/test" font)
if(NOT TARGET generate_font_test_images)
  message(FATAL_ERROR "The manual font image target must remain available")
endif()
get_property(sequential GLOBAL PROPERTY DEFOLD_RUN_TESTS_SEQUENTIAL_SCRIPT_CONTENT)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sequential.cmake" CONTENT "${sequential}")
''')
                env = os.environ.copy()
                env.pop('GITHUB_ACTIONS', None)
                if ci:
                    env['GITHUB_ACTIONS'] = 'true'
                env['DEFOLD_SKIP_FONT_IMAGE_TESTS'] = skip_images
                result = subprocess.run([
                    'cmake', '-S', str(root), '-B', str(root / 'build'), '-G', 'Ninja',
                    '-DDEFOLD_HOME=' + repository.as_posix(),
                    '-DDEFOLD_BUILD_HOME=' + root.as_posix(),
                    '-DTARGET_PLATFORM=' + platform,
                    '-DWITH_MSAN=' + ('ON' if msan else 'OFF'),
                ], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                self.assertEqual(0, result.returncode, result.stdout)
                manifest = next((root / 'build').glob('defold_run_tests_*.json'))
                names = {test['name'] for test in json.loads(manifest.read_text())}
                self.assertEqual(expected, 'generate_font_test_images' in names)
                self.assertIn('run_test_font', names)
                self.assertIn('run_test_font_skribidi', names)
                if platform.endswith('linux'):
                    self.assertIn('run_test_font_images_python', names)
                sequential = (root / 'build/sequential.cmake').read_text()
                self.assertEqual(expected, 'run_font_images.cmake' in sequential)
                self.assertIn('test_font_images.py', sequential)


if __name__ == '__main__':
    unittest.main()
