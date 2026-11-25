import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class PlatformLinkTests(unittest.TestCase):
    def require_xcode(self):
        if not shutil.which('xcodebuild') or subprocess.run(
                ['xcodebuild', '-version'], capture_output=True).returncode:
            self.skipTest('Xcode is required')

    def configure_project(self, root, generator='Ninja', **definitions):
        repository = Path(__file__).resolve().parent.parent
        build = root / 'build'
        query = build / '.cmake/api/v1/query'
        query.mkdir(parents=True, exist_ok=True)
        (query / 'codemodel-v2').touch()
        return subprocess.run([
            'cmake', '-S', str(root), '-B', str(build), '-G', generator,
            '-DDEFOLD_HOME=' + repository.as_posix(),
            '-DDEFOLD_BUILD_HOME=' + root.as_posix(),
            *[f'-D{name}={value}' for name, value in definitions.items()],
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    def configure_target(self, root, target_name, **definitions):
        result = self.configure_project(root, **definitions)
        self.assertEqual(0, result.returncode, result.stdout)
        return self.read_target(root, target_name)

    def read_target(self, root, target_name):
        reply = root / 'build/.cmake/api/v1/reply'
        codemodel = json.loads(next(reply.glob('codemodel-v2-*.json')).read_text())
        target = next(target for target in codemodel['configurations'][0]['targets']
                      if target['name'] == target_name)
        return json.loads((reply / target['jsonFile']).read_text())

    def write_dlib_link_project(self, root):
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(mbedtls_link_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(DEFOLD_LIB_DIR "${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}")
set(DEFOLD_EXT_LIB_DIR "${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}")
set(BUILD_TESTS OFF)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
# These tests inspect the build graph, so an empty archive satisfies the SDK import.
file(WRITE "${DEFOLD_EXT_LIB_DIR}/liblz4${CMAKE_STATIC_LIBRARY_SUFFIX}" "!<arch>\\n")
add_library(defold_sdk INTERFACE)
include(functions)
include(features)
add_subdirectory("${DEFOLD_HOME}/engine/dlib" dlib)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.c" "int main() { return 0; }\\n")
add_executable(mbedtls_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.c")
defold_target_link_libraries(mbedtls_consumer "${TARGET_PLATFORM}"
    dlib dlib_noasan mbedtls mbedtls_noasan)
''')

    def write_sdk_header_project(self, root):
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(sdk_header_test LANGUAGES CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_SDK_ROOT "${CMAKE_BINARY_DIR}/installed")
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_custom_target(build_tests)
foreach(target IN ITEMS ddf_generated_sources gameobject_proto_headers gamesys_proto_headers
    gamesys_model_proto_headers graphics_proto_header input_proto_headers particle_proto_headers
    render_proto_headers rig_proto_headers)
  add_custom_target(${target})
endforeach()
file(GLOB DEFOLD_DMSDK_HEADER_NAMES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(TRANSFORM DEFOLD_DMSDK_HEADER_NAMES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/" OUTPUT_VARIABLE DEFOLD_DMSDK_HEADER_SOURCES)
install(FILES ${DEFOLD_DMSDK_HEADER_SOURCES} DESTINATION sdk/include/dmsdk COMPONENT defold_sdk_headers)
add_subdirectory("${DEFOLD_HOME}/engine/sdk/src/test" sdk)
''')

    # Unchanged builds skip staging, missing outputs are restored, and retired headers stop satisfying includes.
    def test_sdk_header_staging_is_incremental(self):
        generators = ['Ninja']
        if os.name != 'nt' and shutil.which('make'):
            generators.append('Unix Makefiles')
        for generator in generators:
            with self.subTest(generator=generator), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.write_sdk_header_project(root)
                header = root / 'public.h'
                header.write_text('#include <dmsdk/retired.h>\n#include <dmsdk/vectormath/package.h>\n')
                retired = root / 'retired.h'
                retired.write_text('#pragma once\n')
                # Keep a staged header older than the inputs that trigger staging.
                os.utime(header, (1, 1))
                installed = root / 'build/installed/sdk/include/dmsdk'
                (installed / 'vectormath').mkdir(parents=True)
                (installed / 'vectormath/package.h').write_text('#pragma once\n')
                (installed / 'retired.h').write_text(retired.read_text())
                result = self.configure_project(root, generator=generator, TARGET_PLATFORM='x86_64-linux',
                                                CMAKE_BUILD_TYPE='Debug')
                self.assertEqual(0, result.returncode, result.stdout)
                build = root / 'build'
                command = ['cmake', '--build', str(build), '--target', 'test_sdk_includes_cpp']
                stage = build / 'sdk/sdk-headers/Debug'
                staging = 'Staging public headers for SDK tests'
                self.assertIn(staging, subprocess.check_output(command, text=True))
                self.assertNotIn(staging, subprocess.check_output(command, text=True))
                staged_header, = stage.rglob('public.h')
                staged_header.unlink()
                subprocess.check_output(command)
                self.assertEqual(header.read_text(), staged_header.read_text())

                retired.unlink()
                result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                self.assertEqual([], list(stage.rglob('retired.h')))
                self.assertNotEqual(0, result.returncode, result.stdout)
                self.assertIn('retired.h', result.stdout)
                (root / 'replacement.h').write_text('#pragma once\n')
                header.write_text('#include <dmsdk/replacement.h>\n#include <dmsdk/vectormath/package.h>\n')
                subprocess.check_output(command)

    # Ninja must recompile a changed header even when both source edits fall in the same second.
    def test_sdk_header_staging_detects_subsecond_edits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_sdk_header_project(root)
            header = root / 'public.h'
            header.write_text('#pragma once\n')
            os.utime(header, ns=(1_250_000_000, 1_250_000_000))
            (root / 'unchanged.h').write_text('#pragma once\n')
            result = self.configure_project(root, TARGET_PLATFORM='x86_64-linux', CMAKE_BUILD_TYPE='Debug')
            self.assertEqual(0, result.returncode, result.stdout)
            build = root / 'build'
            command = ['cmake', '--build', str(build), '--target', 'test_sdk_includes_cpp']
            subprocess.check_output(command)
            stage = build / 'sdk/sdk-headers/Debug'
            unchanged, = stage.rglob('unchanged.h')
            before = unchanged.stat().st_mtime_ns

            header.write_text('#error public_header_changed\n')
            os.utime(header, ns=(1_750_000_000, 1_750_000_000))
            # Force staging of these deliberately old inputs to exercise Ninja's output timestamp check.
            stamp, = stage.rglob('headers.stamp')
            stamp.unlink()
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertNotEqual(0, result.returncode, result.stdout)
            self.assertIn('public_header_changed', result.stdout)
            self.assertEqual(before, unchanged.stat().st_mtime_ns)

    # A test build must preserve the file list written by an actual SDK header component install.
    def test_sdk_header_staging_preserves_install_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_sdk_header_project(root)
            (root / 'public.h').write_text('#pragma once\n')
            result = self.configure_project(root, TARGET_PLATFORM='x86_64-linux', CMAKE_BUILD_TYPE='Debug')
            self.assertEqual(0, result.returncode, result.stdout)
            build = root / 'build'
            prefix = root / 'installed sdk'
            subprocess.check_output(['cmake', '--install', str(build), '--config', 'Debug',
                                     '--component', 'defold_sdk_headers', '--prefix', str(prefix)])
            manifest = build / 'install_manifest_defold_sdk_headers.txt'
            installed_files = manifest.read_bytes()
            self.assertEqual([prefix / 'sdk/include/dmsdk/public.h'],
                             [Path(line) for line in installed_files.decode().splitlines()])
            subprocess.check_output(['cmake', '--build', str(build), '--target', 'test_sdk_includes_cpp'])
            self.assertEqual(installed_files, manifest.read_bytes())

    # A packaging DESTDIR must redirect the outer install and leave the SDK test stage inside the build tree.
    @unittest.skipIf(os.name == 'nt', 'DESTDIR packaging requires Unix paths')
    def test_sdk_header_stage_ignores_packaging_destdir(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_sdk_header_project(root)
            (root / 'public.h').write_text('#pragma once\n')
            result = self.configure_project(root, TARGET_PLATFORM='x86_64-linux', CMAKE_BUILD_TYPE='Debug',
                                            CMAKE_INSTALL_PREFIX='/defold')
            self.assertEqual(0, result.returncode, result.stdout)
            build = root / 'build'
            package = root / 'package'
            header = Path('sdk/include/dmsdk/public.h')
            result = subprocess.run(['cmake', '--build', str(build), '--target', 'test_sdk_includes_cpp', 'install'],
                                    env=dict(os.environ, DESTDIR=str(package)), stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True)
            self.assertEqual(0, result.returncode, result.stdout)
            self.assertEqual(1, len(list((build / 'sdk/sdk-headers/Debug').rglob('public.h'))))
            self.assertEqual([package / 'defold' / header], list(package.rglob('public.h')))

    # Single/batched HTTP suites must use the same lock in focused and aggregate runs.
    def test_server_groups_match_direct_target_locks(self):
        for platform in ('arm64-macos', 'x86_64-linux', 'x86_64-win32'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(test_server_groups LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
set(DEFOLD_SDK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/sdk")
foreach(target IN ITEMS single batched default_single default_batched)
  add_executable(${target} IMPORTED)
  set_target_properties(${target} PROPERTIES IMPORTED_LOCATION "${CMAKE_COMMAND}")
endforeach()
defold_register_test_with_server(single "${TARGET_PLATFORM}"
  PORT 9001 RUN_GROUP http_9001 WORKDIR "${CMAKE_CURRENT_SOURCE_DIR}")
defold_register_tests_with_server(batch "${TARGET_PLATFORM}"
  TARGETS batched PORT 9001 RUN_GROUP http_9001 WORKDIR "${CMAKE_CURRENT_SOURCE_DIR}")
defold_register_test_with_server(default_single "${TARGET_PLATFORM}")
defold_register_tests_with_server(default_batch "${TARGET_PLATFORM}" TARGETS default_batched)
''')
                result = self.configure_project(root, TARGET_PLATFORM=platform)
                self.assertEqual(0, result.returncode, result.stdout)
                build = root / 'build'
                manifest = json.loads(next(build.glob('defold_run_tests_*.json')).read_text())
                expected = {'run_single_server': 'http_9001', 'run_batch_server': 'http_9001',
                            'run_default_single_server': 'shared', 'run_default_batch_server': 'shared'}
                self.assertEqual(expected, {test['name']: test['group'] for test in manifest})
                for target, group in expected.items():
                    result = subprocess.run(
                        ['ninja', '-C', str(build), '-t', 'commands', target],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(0, result.returncode, result.stdout)
                    lock = hashlib.sha256(group.encode('utf-8')).hexdigest() + '.lock'
                    self.assertIn(lock, result.stdout)

    # Per-test loader settings must reach every runner without affecting other tests.
    def test_run_environment_is_scoped_to_registered_test(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'probe.c').write_text('''
#include <stdlib.h>
#include <string.h>
int main(void) {
    const char* value = getenv("DEFOLD_TEST_RUN_VALUE");
    return value && strcmp(value, EXPECTED_VALUE) == 0 ? 0 : 1;
}
''')
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(test_environment LANGUAGES C)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
set(DEFOLD_SDK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/sdk")
add_custom_target(build_tests)
add_executable(configured probe.c)
target_compile_definitions(configured PRIVATE EXPECTED_VALUE="child value")
defold_register_test_target(configured ON "${CMAKE_CURRENT_SOURCE_DIR}"
  RUN_GROUP graphics ENVIRONMENT "DEFOLD_TEST_RUN_VALUE=child value")
add_executable(inherited probe.c)
target_compile_definitions(inherited PRIVATE EXPECTED_VALUE="parent value")
defold_register_test_target(inherited ON)
defold_finalize_sequential_run_tests()
''')
            result = self.configure_project(root, TARGET_PLATFORM='arm64-linux')
            self.assertEqual(0, result.returncode, result.stdout)
            environment = dict(os.environ, DEFOLD_TEST_RUN_VALUE='parent value')
            for target in ('run_configured', 'run_inherited', 'run_tests', 'run_tests_sequential'):
                with self.subTest(target=target):
                    result = subprocess.run(
                        ['cmake', '--build', str(root / 'build'), '--target', target],
                        env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(0, result.returncode, result.stdout)

    # Focused render builds must compile shared sanitizer support without configuring graphics.
    def test_focused_render_build_can_include_graphics_sanitizer_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'probe.cpp').write_text('''
#include <graphics/graphics_test_sanitizers.h>
int main() { return GraphicsTestRetainDriLibraries() ? 0 : 1; }
''')
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(focused_render_header LANGUAGES CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_SDK_ROOT "${CMAKE_BINARY_DIR}/sdk")
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
# Exercise the render test configuration without graphics targets or SDK libraries.
foreach(command IN ITEMS defold_target_link_libraries defold_target_link_graphics
    defold_target_link_platform defold_target_link_app defold_target_link_socket
    defold_target_link_glfw defold_target_link_libraries_web defold_register_test_target)
  function(${command})
  endfunction()
endforeach()
function(defold_add_executable target)
  add_executable(${target} "${CMAKE_SOURCE_DIR}/probe.cpp")
endfunction()
add_subdirectory("${DEFOLD_HOME}/engine/render/src/test" render)
''')
            result = self.configure_project(root, TARGET_PLATFORM='arm64-linux')
            self.assertEqual(0, result.returncode, result.stdout)
            result = subprocess.run(
                ['cmake', '--build', str(root / 'build'), '--target', 'test_gltf_render'],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(0, result.returncode, result.stdout)

    # Apple consumers must omit mbedTLS by default, including dependencies propagated by dlib.
    def test_mbedtls_linking_matches_crypto_backend(self):
        apple_platforms = ('arm64-macos', 'x86_64-macos', 'arm64-ios', 'arm64_sim-ios')
        cases = [(platform, enabled, '', 'OFF', bool(enabled))
                 for platform in apple_platforms for enabled in ('', 'mbedtls')]
        cases += [(platform, '', '', 'OFF', True)
                  for platform in ('x86_64-linux', 'x86_64-win32', 'arm64-android', 'wasm-web')]
        cases += [('arm64-macos', '', '', 'ON', True),
                  ('arm64-macos', 'mbedtls', 'mbedtls', 'ON', False)]
        for platform, enabled, disabled, legacy, expected in cases:
            with self.subTest(platform=platform, enabled=enabled, disabled=disabled, legacy=legacy), \
                    tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.write_dlib_link_project(root)
                consumer = self.configure_target(
                    root, 'mbedtls_consumer', TARGET_PLATFORM=platform,
                    DEFOLD_ENABLE_FEATURES=enabled, DEFOLD_DISABLE_FEATURES=disabled, WITH_MBEDTLS=legacy)
                libraries = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments']
                                     if fragment['role'] == 'libraries')
                for library in ('dmbedtls', 'dmbedtls_noasan'):
                    self.assertEqual(expected, bool(re.search(r'\b(?:lib)?' + library + r'\.(?:a|lib)\b', libraries)),
                                     libraries)

    # Prevent duplicate compilation in ordinary builds and ASan instrumentation in the JNI archives.
    def test_dlib_static_archives_reuse_objects_only_without_asan(self):
        for with_asan in ('OFF', 'ON'):
            with self.subTest(with_asan=with_asan), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                build = root / 'build'
                self.write_dlib_link_project(root)
                result = self.configure_project(root, TARGET_PLATFORM='x86_64-win32',
                                                WITH_ASAN=with_asan, CMAKE_EXPORT_COMPILE_COMMANDS='ON')
                self.assertEqual(0, result.returncode, result.stdout)
                commands = {
                    Path(entry['directory'], entry['output']).resolve(): entry
                    for entry in json.loads((build / 'compile_commands.json').read_text())
                }
                archive_objects = {}
                for name in ('dlib', 'dlib_noasan', 'mbedtls', 'mbedtls_noasan'):
                    archive = self.read_target(root, name)['artifacts'][0]['path']
                    graph = subprocess.check_output(['ninja', '-t', 'query', archive], cwd=build, text=True)
                    inputs = graph.split('  outputs:', 1)[0]
                    objects = {Path(build, line.strip()).resolve() for line in inputs.splitlines()
                               if line.startswith('    ') and line.endswith(('.o', '.obj'))}
                    self.assertTrue(objects, graph)
                    archive_objects[name] = objects
                    instrumented = with_asan == 'ON' and not name.endswith('_noasan')
                    for obj in objects:
                        command = commands[obj]['command']
                        self.assertEqual(instrumented, 'fsanitize=address' in command, command)

                for name in ('dlib', 'mbedtls'):
                    normal = archive_objects[name]
                    noasan = archive_objects[name + '_noasan']
                    if with_asan == 'ON':
                        self.assertTrue(normal.isdisjoint(noasan), name)
                        self.assertEqual({commands[obj]['file'] for obj in normal},
                                         {commands[obj]['file'] for obj in noasan})
                    else:
                        self.assertEqual(normal, noasan, name)

    # Xcode needs a compiled source on each static library to emit its archive.
    def test_xcode_dlib_archives_have_compiled_sources(self):
        self.require_xcode()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_dlib_link_project(root)
            result = self.configure_project(root, generator='Xcode', TARGET_PLATFORM='arm64-macos')
            self.assertEqual(0, result.returncode, result.stdout)
            for name in ('dlib', 'dlib_noasan', 'mbedtls', 'mbedtls_noasan'):
                with self.subTest(target=name):
                    self.assertTrue(self.read_target(root, name).get('compileGroups'))

    # Generate the complete Xcode project to catch conflicting producers for shared test fixtures.
    def test_xcode_engine_project_with_tests(self):
        self.require_xcode()
        sdk = os.environ.get('DYNAMO_HOME')
        if not sdk:
            self.skipTest('Run from the Defold build shell with the macOS SDK installed')
        repository = Path(__file__).resolve().parent.parent
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = subprocess.run([
                'cmake', '-G', 'Xcode', '-S', str(repository), '-B', str(root / 'project'),
                f'-DDEFOLD_BUILD_HOME={root}', f'-DDEFOLD_SDK_ROOT={sdk}',
                f'-DTARGET_PLATFORM={os.uname().machine}-macos', '-DBUILD_TESTS=ON',
            ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(0, result.returncode, result.stdout)

    # Xcode must run each custom command once, including with an existing macOS or iOS Run Tests target.
    def test_xcode_run_tests_includes_custom_commands_once(self):
        self.require_xcode()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'record.cmake').write_text('file(APPEND ran.txt "${TEST_NAME}\\n")\n')
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(custom_tests LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
if(PRECREATE_RUN_TESTS)
  add_custom_target(run_tests COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/existing-ran")
endif()
foreach(name IN ITEMS first second)
  set(command "${CMAKE_COMMAND}" "-DTEST_NAME=${name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/record.cmake")
  add_custom_target(run_${name} COMMAND ${command} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set(options)
  if(name STREQUAL "second")
    set(options SEQUENTIAL_NAME second)
  endif()
  defold_add_to_run_tests(run_${name} ${options} COMMAND ${command} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()
defold_finalize_xcode_run_tests()
''')
            for platform, precreated in (('arm64-macos', 'OFF'), ('arm64-macos', 'ON'), ('arm64-ios', 'ON')):
                with self.subTest(platform=platform, precreated=precreated):
                    result = self.configure_project(root, generator='Xcode', TARGET_PLATFORM=platform,
                                                    PRECREATE_RUN_TESTS=precreated)
                    self.assertEqual(0, result.returncode, result.stdout)
                    result = subprocess.run(['cmake', '--build', str(root / 'build'), '--config', 'Debug',
                                             '--target', 'run_tests'],
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(0, result.returncode, result.stdout)
                    existing = root / 'build/existing-ran'
                    self.assertEqual(precreated == 'ON', existing.is_file())
                    existing.unlink(missing_ok=True)
                    record = root / 'ran.txt'
                    self.assertTrue(record.is_file(), result.stdout)
                    names = record.read_text().splitlines()
                    if platform == 'arm64-macos':
                        self.assertEqual(['first', 'second'], names)
                    else:
                        self.assertCountEqual(['first', 'second'], names)
                    record.unlink()

    # Aggregate custom tests must resolve default and relative work directories from the registering subdirectory.
    def test_custom_test_working_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subdir = root / 'suite'
            subdir.mkdir()
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(custom_test_directories LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_custom_target(run_tests)
add_subdirectory(suite)
defold_finalize_sequential_run_tests()
''')
            (subdir / 'CMakeLists.txt').write_text('''
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work dir,tests/Debug")
foreach(name IN ITEMS default relative)
  set(options)
  if(name STREQUAL "relative")
    set(options WORKING_DIRECTORY "work dir,tests/$<CONFIG>")
  endif()
  set(command "${CMAKE_COMMAND}" -E touch "${name}.ran")
  add_custom_target(run_${name} COMMAND ${command} ${options} VERBATIM)
  defold_add_to_run_tests(run_${name} SEQUENTIAL_NAME ${name} COMMAND ${command} ${options})
endforeach()
''')
            result = self.configure_project(root, TARGET_PLATFORM='arm64-macos', CMAKE_BUILD_TYPE='Debug')
            self.assertEqual(0, result.returncode, result.stdout)
            build = root / 'build'
            for targets in (['run_default', 'run_relative'], ['run_tests_sequential'], ['run_tests']):
                with self.subTest(targets=targets):
                    result = subprocess.run(['cmake', '--build', str(build), '--target', *targets],
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(0, result.returncode, result.stdout)
                    for marker in (build / 'suite/default.ran', build / 'suite/work dir,tests/Debug/relative.ran'):
                        self.assertTrue(marker.is_file(), marker)
                        marker.unlink()

    # Keep the default build limited to the font tools used by each CI test suite.
    def test_font_tools_follow_enabled_test_suites(self):
        generator = 'test_font_bitmap_gen'
        generators = {generator, generator + '_skribidi', generator + '_plain', generator + '_skribidi_plain'}
        benchmarks = {'benchmark_text_layout', 'benchmark_text_layout_null'}
        tools = generators | benchmarks | {'fontviewer'}
        cli = 'run_test_font_images_python'
        viewer = 'run_test_fontviewer_color'
        images = 'generate_font_test_images'
        cases = (
            ('x86_64-win32', '0', set(), set()),
            ('arm64-macos', '0', set(), set()),
            ('x86_64-linux', '1', {generator, 'fontviewer'}, {cli, viewer}),
            ('arm64-linux', '0', generators, {cli, images}),
            ('x86_64-linux', '0', generators | {'fontviewer'}, {cli, viewer, images}),
        )
        for platform, skip_images, expected_tools, expected_checks in cases:
            with self.subTest(platform=platform, skip_images=skip_images), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                build = root / 'build'
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(font_test_graph LANGUAGES C CXX)
set(ENV{GITHUB_ACTIONS} true)
set(ENV{DEFOLD_SKIP_FONT_IMAGE_TESTS} "${SKIP_FONT_IMAGES}")
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(DEFOLD_TEST_JOBS 2)
set(FONT_RENDERER_IS_DESKTOP ON)
set(FONT_RENDER_SRC_DIR "${DEFOLD_HOME}/engine/font/src/render")
set(FONTC_SRC_DIR "${DEFOLD_HOME}/engine/font/src/fontc")
set(DEFOLD_BOB_LIGHT_JAR "${CMAKE_BINARY_DIR}/bob-light.jar")
file(WRITE "${DEFOLD_BOB_LIGHT_JAR}" "")
# This configure-only graph probe stands in for the compiler DLL and SDK libraries.
file(WRITE "${CMAKE_BINARY_DIR}/fontc.cpp" "")
add_library(fontc_shared SHARED "${CMAKE_BINARY_DIR}/fontc.cpp")
add_library(basis_transcoder INTERFACE)
add_library(defold_sdk INTERFACE)
add_custom_target(build_tests)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
include(features)
add_subdirectory("${DEFOLD_HOME}/engine/font/src/test" font-tests)
''')
                result = self.configure_project(root, TARGET_PLATFORM=platform, SKIP_FONT_IMAGES=skip_images)
                self.assertEqual(0, result.returncode, result.stdout)

                def built_tools(*targets):
                    commands = subprocess.check_output(['ninja', '-t', 'commands', *targets], cwd=build, text=True)
                    commands = commands.replace('\\', '/')
                    return {tool for tool in tools if f'CMakeFiles/{tool}.dir/' in commands}

                self.assertEqual(expected_tools, built_tools('all', 'build_tests'))
                self.assertEqual(expected_tools, built_tools('run_tests'))
                manifest = json.loads(next(build.glob('defold_run_tests_*.json')).read_text())
                scheduled_checks = {test['name'] for test in manifest}
                self.assertEqual(expected_checks, scheduled_checks & {cli, viewer, images})
                self.assertIn('run_test_font', scheduled_checks)

                if platform == 'x86_64-win32':
                    self.assertEqual({generator}, built_tools(cli))
                    self.assertEqual(generators, built_tools(images))
                    self.assertEqual({'fontviewer'}, built_tools(viewer))
                    self.assertEqual(benchmarks, built_tools(*sorted(benchmarks)))

    # Host tools must compile and link for their own macOS minimum, even when the engine targets a newer OS.
    def test_macos_deployment_target_override(self):
        cases = (('', '', '12.0'), ('11.0', '', '11.0'), ('13.0', '11.0', '11.0'))
        for platform in ('arm64-macos', 'x86_64-macos'):
            for default, override, expected in cases:
                with self.subTest(platform=platform, default=default, override=override), \
                        tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(macos_deployment_test LANGUAGES C)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
set(SDK_VERSION_MACOSX_MIN 12.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET "${DEFAULT_DEPLOYMENT_TARGET}")
add_library(defold_sdk INTERFACE)
include(platform_macos)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.c" "int main() { return 0; }\\n")
add_executable(macos_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.c")
target_link_libraries(macos_consumer PRIVATE defold_sdk)
if(TARGET_DEPLOYMENT_TARGET)
    set_target_properties(macos_consumer PROPERTIES
        DEFOLD_MACOS_DEPLOYMENT_TARGET "${TARGET_DEPLOYMENT_TARGET}")
endif()
''')
                    consumer = self.configure_target(
                        root, 'macos_consumer', TARGET_PLATFORM=platform,
                        DEFAULT_DEPLOYMENT_TARGET=default, TARGET_DEPLOYMENT_TARGET=override)
                    fragments = (consumer['compileGroups'][0]['compileCommandFragments'],
                                 consumer['link']['commandFragments'])
                    for command_fragments in fragments:
                        command = ' '.join(fragment['fragment'] for fragment in command_fragments)
                        versions = re.findall(r'-mmacosx-version-min=([0-9.]+)', command)
                        self.assertTrue(versions, command)
                        self.assertEqual(expected, versions[-1], command)

    # Normal dependency builds must use DXC packages and avoid rebuilding LLVM on every desktop host.
    def test_external_graph_uses_dxc_packages(self):
        cases = (
            ('arm64-macos', 'arm64-macos'),
            ('arm64-macos', 'x86_64-macos'),
            ('x86_64-macos', 'arm64-macos'),
            ('x86_64-macos', 'x86_64-macos'),
            ('x86_64-win32', 'x86_64-win32'),
            ('arm64-linux', 'arm64-linux'),
            ('x86_64-linux', 'x86_64-linux'),
            ('arm64-macos', 'arm64-ios'),
            ('x86_64-linux', 'arm64-android'),
        )
        for host, platform in cases:
            with self.subTest(host=host, platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(external_graph_test LANGUAGES NONE)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST NONE)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
add_custom_target(external_graph)
# Record the real dependency selection without configuring unrelated packages.
function(add_subdirectory directory)
    file(APPEND "${CMAKE_BINARY_DIR}/external-subdirectories.txt" "${directory}\\n")
endfunction()
include("${DEFOLD_HOME}/external/CMakeLists.txt")
''')
                self.configure_target(root, 'external_graph', HOST_PLATFORM=host, TARGET_PLATFORM=platform)
                directories = (root / 'build/external-subdirectories.txt').read_text().splitlines()
                self.assertNotIn('dxc', directories)

    # Explicit macOS package builds must still produce a matching DXC executable and library when cross-building.
    def test_macos_dxc_package_targets_requested_architecture(self):
        for platform in ('arm64-macos', 'x86_64-macos'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(dxc_platform_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(HOST_PLATFORM arm64-macos)
# Model the macOS target even when the configure-only test runs on another OS.
set(WIN32 FALSE)
set(APPLE TRUE)
set(CMAKE_SHARED_LIBRARY_PREFIX lib)
set(CMAKE_SHARED_LIBRARY_SUFFIX .dylib)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_library(defold_sdk INTERFACE)
set(SDK_VERSION_MACOSX_MIN 12.0)
include(platform_macos)
# Explicit packaging configures the DXC wrapper independently of the normal
# dependency graph; configuring alone does not download or compile LLVM.
add_subdirectory("${DEFOLD_HOME}/external/dxc" "${CMAKE_BINARY_DIR}/dxc")
''')
                self.configure_target(root, 'dxc_build_external', TARGET_PLATFORM=platform)
                command = (root / 'build/dxc/dxc_build_external-prefix/tmp/dxc_build_external-cfgcmd.txt').read_text()
                architecture = platform.split('-')[0]
                self.assertIn('-DCMAKE_OSX_ARCHITECTURES=' + architecture, command)
                self.assertIn('-DCMAKE_INSTALL_RPATH=@loader_path/../../lib/' + platform, command)
                install = (root / 'build/dxc/cmake_install.cmake').read_text()
                self.assertIn('/bin/' + platform, install)
                self.assertIn('/lib/' + platform, install)
                self.assertIn('dxcompiler', install)

    # Graphics-only Windows builds must resolve DXC without configuring shaderc first.
    def test_focused_graphics_dxc_dependency(self):
        for in_tree in ('OFF', 'ON'):
            with self.subTest(in_tree=in_tree), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 4.0)
project(focused_graphics_test LANGUAGES C CXX)
set(TARGET_PLATFORM x86_64-win32)
set(GRAPHICS_SRC_DIR "${DEFOLD_HOME}/engine/graphics/src")
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(DEFOLD_EXT_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include")
file(MAKE_DIRECTORY "${DEFOLD_EXT_INCLUDE_DIR}/directx")
file(WRITE "${DEFOLD_SDK_ROOT}/ext/bin/${TARGET_PLATFORM}/dxcompiler.dll" "runtime")
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
if(IN_TREE_SHADERC)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sibling/CMakeLists.txt"
        "include(shaderc_dxc)\\nshaderc_import_dxc()\\nadd_library(shaderc INTERFACE)\\n")
    add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/sibling" sibling-build)
endif()
add_library(graphics_dx12 INTERFACE)
add_custom_target(graphics_test_sdk_headers)
function(defold_create_exported_symbols_file path)
    file(WRITE "${path}" "")
endfunction()
function(defold_add_executable name)
    add_executable(${name} ${ARGN})
endfunction()
function(defold_target_link_libraries name platform)
    target_link_libraries(${name} PRIVATE ${ARGN})
endfunction()
function(defold_target_link_socket)
endfunction()
function(defold_target_link_platform)
endfunction()
function(defold_target_link_glfw)
endfunction()
function(defold_register_test_target)
endfunction()
function(defold_get_graphics_symbols)
endfunction()
add_subdirectory("${DEFOLD_HOME}/engine/graphics/src/test" graphics-tests)
''')
                consumer = self.configure_target(root, 'test_graphics_dx12', IN_TREE_SHADERC=in_tree)
                sources = [source['path'] for source in consumer['sources']]
                self.assertTrue(any(path.endswith('/shaderc_dxc.cpp') for path in sources), sources)
                libraries = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments'])
                self.assertIn('dxcompiler.dll', libraries)
                self.assertNotRegex(libraries, r'(?<![a-z])(?:lib)?shaderc(?:\.lib|\.a|\s)')
                includes = [item['path'] for group in consumer['compileGroups'] for item in group.get('includes', [])]
                self.assertIn((root / 'sdk/ext/include/directx').as_posix(), includes)
                build = (root / 'build/build.ninja').read_text()
                self.assertIn('copy_if_different', build)
                self.assertIn('dxcompiler.dll', build)

    def write_linux_dxc_project(self, root):
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(dxc_linux_test LANGUAGES NONE)
# Model the runner without compiling Linux sources on the test machine.
set(CMAKE_SYSTEM_NAME "${RUNNER_SYSTEM}")
set(CMAKE_SYSTEM_PROCESSOR "${RUNNER_PROCESSOR}")
set(WIN32 FALSE)
set(APPLE FALSE)
set(CMAKE_SHARED_LIBRARY_PREFIX lib)
set(CMAKE_SHARED_LIBRARY_SUFFIX .so)
add_subdirectory("${DEFOLD_HOME}/external/dxc" "${CMAKE_BINARY_DIR}/dxc")
''')

    # Native Linux package configurations must retain relocatable library lookup and matching install paths.
    def test_linux_dxc_packages_use_relative_runtime_paths(self):
        for platform, processor in (('arm64-linux', 'aarch64'), ('x86_64-linux', 'x86_64')):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.write_linux_dxc_project(root)
                self.configure_target(root, 'dxc_build_external', TARGET_PLATFORM=platform,
                                      RUNNER_SYSTEM='Linux', RUNNER_PROCESSOR=processor)
                command = (root / 'build/dxc/dxc_build_external-prefix/tmp/dxc_build_external-cfgcmd.txt').read_text()
                self.assertIn('-DCMAKE_INSTALL_RPATH=$ORIGIN/../../lib/' + platform, command)
                self.assertIn('-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON', command)
                install = (root / 'build/dxc/cmake_install.cmake').read_text()
                self.assertIn('/bin/' + platform, install)
                self.assertIn('/lib/' + platform, install)
                self.assertIn('libdxcompiler.so', install)

    # Refuse incompatible Linux package runners before they can produce host binaries in the wrong archive.
    def test_linux_dxc_packages_reject_incompatible_runners(self):
        cases = (
            ('arm64-linux', 'Linux', 'x86_64'),
            ('x86_64-linux', 'Linux', 'aarch64'),
            ('arm64-linux', 'Darwin', 'arm64'),
            ('x86_64-linux', 'Darwin', 'x86_64'),
        )
        for platform, system, processor in cases:
            with self.subTest(platform=platform, system=system), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.write_linux_dxc_project(root)
                result = self.configure_project(root, TARGET_PLATFORM=platform,
                                                RUNNER_SYSTEM=system, RUNNER_PROCESSOR=processor)
                self.assertNotEqual(0, result.returncode)
                self.assertIn('DXC Linux tools must be built on a native runner for ' + platform, result.stdout)

    # Linux consumers must discard unused LZ4 wrapper paths with the shipping linker settings.
    def test_linux_lz4_uses_function_sections_and_section_gc(self):
        for platform in ('x86_64-linux', 'arm64-linux'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(linux_lz4_link_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/sdk")
set(DEFOLD_LIB_DIR "${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}")
set(DEFOLD_EXT_LIB_DIR "${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}")
set(BUILD_TESTS OFF)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
include(features)
add_library(defold_sdk INTERFACE)
include(platform_linux)
add_library(lz4 INTERFACE)
add_subdirectory("${DEFOLD_HOME}/engine/dlib" dlib)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.c" "int main() { return 0; }\\n")
add_executable(lz4_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.c")
target_link_libraries(lz4_consumer PRIVATE dlib)
''')
                consumer = self.configure_target(root, 'lz4_consumer', TARGET_PLATFORM=platform)
                library = self.read_target(root, 'dlib_objects')
                source = next(source for source in library['sources']
                              if source['path'].endswith('/dlib/lz4.cpp'))
                group = library['compileGroups'][source['compileGroupIndex']]
                flags = ' '.join(fragment['fragment'] for fragment in group['compileCommandFragments'])
                self.assertIn('-ffunction-sections', flags)
                self.assertIn('-fdata-sections', flags)
                flags = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments'])
                self.assertIn('-Wl,--gc-sections', flags)

    # Linux defaults to Vulkan and GL, without requiring GLU, X11-xcb, or the Vulkan loader at startup.
    def test_linux_vulkan_runtime_linking(self):
        for platform, gl_symbol, gl_libraries in (
                ('x86_64-linux', 'GraphicsAdapterOpenGL', {'graphics', 'GL'}),
                ('arm64-linux', 'GraphicsAdapterOpenGLES', {'graphics_opengles', 'GLESv2'})):
            for vulkan in (None, False, True):
                with self.subTest(platform=platform, vulkan=vulkan), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(linux_adapters LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
include(features)
add_library(basis_transcoder INTERFACE)
add_library(engine INTERFACE)
defold_target_link_graphics(engine "${TARGET_PLATFORM}" SCOPE INTERFACE)
defold_target_link_platform(engine "${TARGET_PLATFORM}" SCOPE INTERFACE)
defold_get_graphics_symbols(symbols "${TARGET_PLATFORM}")
get_target_property(libraries engine INTERFACE_LINK_LIBRARIES)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/adapters.txt" "${symbols}\\n${libraries}")
''')
                    definitions = {'TARGET_PLATFORM': platform}
                    if vulkan is not None:
                        definitions['WITH_VULKAN'] = 'ON' if vulkan else 'OFF'
                    result = self.configure_project(root, **definitions)
                    self.assertEqual(0, result.returncode, result.stdout)
                    symbols, libraries = (root / 'build/adapters.txt').read_text().splitlines()
                    expected_symbols = {gl_symbol}
                    expected_libraries = set(gl_libraries)
                    libraries = set(libraries.split(';'))
                    if vulkan is not False:
                        expected_symbols.add('GraphicsAdapterVulkan')
                        expected_libraries.update({'graphics_vulkan', 'platform_vulkan', 'dl'})
                    else:
                        expected_libraries.add('platform')
                        self.assertNotIn('graphics_vulkan', libraries)
                        self.assertNotIn('platform_vulkan', libraries)
                    self.assertEqual(expected_symbols, set(symbols.split(';')))
                    self.assertTrue(expected_libraries <= libraries, libraries)
                    self.assertNotIn('GLU', libraries)
                    self.assertNotIn('X11-xcb', libraries)
                    self.assertNotIn('vulkan', libraries)

    def configure_consumer(self, root, vulkan, opengl):
        # Generate the real platform/HID/graphics dependency graph with the host compiler.
        # No Android sources are compiled, so an NDK or populated SDK is unnecessary.
        (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(platform_link_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(TARGET_PLATFORM arm64-android)
set(TARGET_PLATFORM_OS android)
set(BUILD_TESTS OFF)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_library(basis_transcoder INTERFACE)
add_custom_target(ddf_sdk_headers)
add_custom_target(dlib_sdk_headers)
file(WRITE "${DEFOLD_SDK_ROOT}/bin/ddfc_cxx" "")
add_subdirectory("${DEFOLD_HOME}/engine/platform" platform)
add_subdirectory("${DEFOLD_HOME}/engine/graphics" graphics)
add_subdirectory("${DEFOLD_HOME}/engine/hid" hid)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" "int main() { return 0; }\\n")
add_executable(platform_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
defold_target_link_libraries(platform_consumer "${TARGET_PLATFORM}" hid graphics)
''')
        consumer = self.configure_target(root, 'platform_consumer', WITH_VULKAN=vulkan, WITH_OPENGL=opengl)
        return ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments']
                        if fragment['role'] == 'libraries')

    # Guard against HID or generic graphics dependencies pulling GLES into Vulkan-only consumers.
    def test_android_consumers_preserve_selected_graphics_backend(self):
        cases = (
            ('ON', 'OFF', False),
            ('OFF', 'ON', True),
            ('ON', 'ON', True),
            ('OFF', 'OFF', True),
        )
        for vulkan, opengl, needs_gles in cases:
            with self.subTest(vulkan=vulkan, opengl=opengl), tempfile.TemporaryDirectory() as directory:
                libraries = self.configure_consumer(Path(directory), vulkan, opengl)
                if needs_gles:
                    self.assertRegex(libraries, r'\b(?:lib)?platform\.(?:a|lib)\b')
                    for library in ('EGL', 'GLESv1_CM', 'GLESv2'):
                        self.assertIn(library, libraries)
                else:
                    self.assertIn('graphics_vulkan', libraries)
                    self.assertNotRegex(libraries, r'\b(?:lib)?graphics\.(?:a|lib)\b')
                    self.assertIn('platform_vulkan', libraries)
                    self.assertNotRegex(libraries, r'\b(?:lib)?platform\.(?:a|lib)\b')
                    for library in ('EGL', 'GLESv1_CM', 'GLESv2'):
                        self.assertNotIn(library, libraries)

    # The service must wait for later graphics headers without selecting a native backend or requiring in-tree graphics.
    def test_platform_service_graphics_headers_preserve_headless_linking(self):
        for in_tree in ('OFF', 'ON'):
            with self.subTest(in_tree=in_tree), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(platform_service_link_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(TARGET_PLATFORM arm64-android)
set(TARGET_PLATFORM_OS android)
set(BUILD_TESTS OFF)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_custom_target(dlib_sdk_headers)
add_subdirectory("${DEFOLD_HOME}/engine/platform" platform)
if(IN_TREE_GRAPHICS)
    add_library(basis_transcoder INTERFACE)
    add_custom_target(ddf_sdk_headers)
    file(WRITE "${DEFOLD_SDK_ROOT}/bin/ddfc_cxx" "")
    add_subdirectory("${DEFOLD_HOME}/engine/graphics" graphics)
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" "int main() { return 0; }\\n")
add_executable(headless_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
defold_target_link_libraries(headless_consumer "${TARGET_PLATFORM}" platform_service graphics_null platform_null)
''')
                service = self.configure_target(root, 'platform_service', IN_TREE_GRAPHICS=in_tree)
                dependencies = [dependency['id'].split('::')[0]
                                for dependency in service.get('dependencies', [])]
                self.assertEqual(in_tree == 'ON', 'graphics_sdk_headers' in dependencies, dependencies)
                consumer = self.read_target(root, 'headless_consumer')
                libraries = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments']
                                     if fragment['role'] == 'libraries')
                for library in ('platform_service', 'graphics_null', 'platform_null'):
                    self.assertIn(library, libraries)
                for library in ('graphics', 'graphics_vulkan', 'platform', 'platform_vulkan'):
                    self.assertNotRegex(libraries, r'\b(?:lib)?' + library + r'\.(?:a|lib)\b')
                for library in ('EGL', 'GLESv1_CM', 'GLESv2'):
                    self.assertNotIn(library, libraries)

    # Mobile extensions must configure standalone while retaining in-tree header build ordering.
    def test_mobile_extension_platform_headers_are_optional(self):
        for platform in ('arm64-android', 'arm64-ios', 'arm64_sim-ios'):
            for in_tree in ('OFF', 'ON'):
                with self.subTest(platform=platform, in_tree=in_tree), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(extension_link_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(BUILD_TESTS OFF)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
include(features)
if(IN_TREE_PLATFORM)
    add_custom_target(platform_sdk_headers)
endif()
add_subdirectory("${DEFOLD_HOME}/engine/extension" extension)
''')
                    extension = self.configure_target(
                        root, 'extension', TARGET_PLATFORM=platform,
                        TARGET_PLATFORM_OS=platform.split('-')[1], IN_TREE_PLATFORM=in_tree)
                    dependencies = [dependency['id'].split('::')[0]
                                    for dependency in extension.get('dependencies', [])]
                    self.assertEqual(in_tree == 'ON', 'platform_sdk_headers' in dependencies)

    # Web consumers need exactly one backend JS library, including links through a static library.
    def test_web_consumers_link_platform_javascript(self):
        for platform in ('wasm-web', 'wasm_pthread-web'):
            for in_tree in ('OFF', 'ON'):
                with self.subTest(platform=platform, in_tree=in_tree), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(web_link_test LANGUAGES C CXX)
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_LANGUAGE_LIST C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(TARGET_PLATFORM_OS web)
set(BUILD_TESTS OFF)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
file(WRITE "${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}/js/library_platform.js" "")
if(IN_TREE_PLATFORM)
    add_subdirectory("${DEFOLD_HOME}/engine/platform" platform)
endif()
add_subdirectory("${DEFOLD_HOME}/engine/hid" hid)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" "int main() { return 0; }\\n")
add_executable(platform_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
target_link_libraries(platform_consumer PRIVATE hid)
''')
                    consumer = self.configure_target(
                        root, 'platform_consumer', TARGET_PLATFORM=platform, IN_TREE_PLATFORM=in_tree)
                    link_command = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments'])
                    if in_tree == 'ON':
                        repository = Path(__file__).resolve().parent.parent
                        library = repository / 'engine/platform/src/native/web/library_platform.js'
                    else:
                        library = root / 'sdk/lib' / platform / 'js/library_platform.js'
                    self.assertIn('--js-library=' + library.as_posix(), link_command)
                    self.assertEqual(1, link_command.count('library_platform.js'))


if __name__ == '__main__':
    unittest.main()
