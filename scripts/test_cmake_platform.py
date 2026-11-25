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
    def configure_project(self, root, **definitions):
        repository = Path(__file__).resolve().parent.parent
        build = root / 'build'
        query = build / '.cmake/api/v1/query'
        query.mkdir(parents=True)
        (query / 'codemodel-v2').touch()
        return subprocess.run([
            'cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
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

    # RandR, Wayland, and the X11-XCB bridge must stay optional for installed Linux archives.
    def test_linux_window_archive_dependencies(self):
        for platform in ('x86_64-linux', 'arm64-linux'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 4.0)
project(linux_window_link LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
foreach(backend IN ITEMS platform platform_vulkan platform_null)
  add_library(consumer_${backend} INTERFACE)
  set(DEFOLD_PLATFORM_WINDOW_LIBRARY ${backend})
  defold_target_link_platform(consumer_${backend} "${TARGET_PLATFORM}" SCOPE INTERFACE)
  get_target_property(libs consumer_${backend} INTERFACE_LINK_LIBRARIES)
  file(WRITE "${CMAKE_BINARY_DIR}/${backend}.txt" "${libs}")
endforeach()
''')
                result = self.configure_project(root, TARGET_PLATFORM=platform)
                self.assertEqual(0, result.returncode, result.stdout)
                for backend in ('platform', 'platform_vulkan', 'platform_null'):
                    libs = set((root / 'build' / f'{backend}.txt').read_text().split(';'))
                    expected = {backend}
                    if backend != 'platform_null':
                        expected.update(('X11', 'dl', 'pthread'))
                    self.assertEqual(expected, libs)

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
    defold_target_link_libraries_web defold_register_test_target)
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
include(functions)
include(features)
add_library(lz4 INTERFACE)
add_subdirectory("${DEFOLD_HOME}/engine/dlib" dlib)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.c" "int main() { return 0; }\\n")
add_executable(mbedtls_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.c")
defold_target_link_libraries(mbedtls_consumer "${TARGET_PLATFORM}"
    dlib dlib_noasan mbedtls mbedtls_noasan)
''')
                consumer = self.configure_target(
                    root, 'mbedtls_consumer', TARGET_PLATFORM=platform,
                    DEFOLD_ENABLE_FEATURES=enabled, DEFOLD_DISABLE_FEATURES=disabled, WITH_MBEDTLS=legacy)
                libraries = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments']
                                     if fragment['role'] == 'libraries')
                for library in ('dmbedtls', 'dmbedtls_noasan'):
                    self.assertEqual(expected, bool(re.search(r'\b(?:lib)?' + library + r'\.(?:a|lib)\b', libraries)),
                                     libraries)

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

    # Graphics-only builds must link native resize tests from SDK archives and resolve DXC without in-tree shaderc.
    def test_focused_graphics_dependencies(self):
        for os_name, in_tree in (('win32', 'OFF'), ('win32', 'ON'), ('linux', 'OFF')):
            with self.subTest(os=os_name, in_tree=in_tree), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 4.0)
project(focused_graphics_test LANGUAGES C CXX)
set(DEFOLD_SDK_ROOT "${DEFOLD_BUILD_HOME}/sdk")
set(DEFOLD_EXT_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include")
file(MAKE_DIRECTORY "${DEFOLD_EXT_INCLUDE_DIR}/directx")
file(WRITE "${DEFOLD_SDK_ROOT}/ext/bin/${TARGET_PLATFORM}/dxcompiler.dll" "runtime")
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
if(IN_TREE_SHADERC)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sibling/CMakeLists.txt"
        "include(shaderc_dxc)\\nshaderc_import_dxc()\\nadd_library(shaderc INTERFACE)\\n")
    add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/sibling" sibling-build)
endif()
if(TARGET_PLATFORM_OS STREQUAL "win32")
    add_library(graphics_dx12 INTERFACE)
endif()
add_library(graphics_vulkan INTERFACE)
add_custom_target(graphics_dmsdk_sdk_headers)
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
function(defold_register_test_target)
endfunction()
function(defold_get_graphics_symbols)
endfunction()
set(GRAPHICS_SRC_DIR "${DEFOLD_HOME}/engine/graphics/src")
add_subdirectory("${GRAPHICS_SRC_DIR}/test" graphics-tests)
''')
                resize = self.configure_target(root, 'test_graphics_window_resize_vulkan',
                                               TARGET_PLATFORM='x86_64-' + os_name, TARGET_PLATFORM_OS=os_name,
                                               IN_TREE_SHADERC=in_tree, WITH_VULKAN='OFF')
                libraries = ' '.join(item['fragment'] for item in resize['link']['commandFragments'])
                self.assertIn('platform_vulkan', libraries)
                self.assertIn('dmbedtls', libraries)
                if os_name == 'linux':
                    self.assertNotIn('X11-xcb', libraries)
                    continue
                dx12 = self.read_target(root, 'test_graphics_window_resize_dx12')
                imports = ' '.join(item['fragment'] for item in dx12['link']['commandFragments'])
                for name in ('imm32', 'dinput8', 'dxguid', 'dmbedtls'):
                    self.assertIn(name, imports)
                consumer = self.read_target(root, 'test_graphics_dx12')
                sources = [source['path'] for source in consumer['sources']]
                self.assertTrue(any(path.endswith('/shaderc_dxc.cpp') for path in sources), sources)
                libraries = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments'])
                self.assertIn('dxcompiler.dll', libraries)
                self.assertIn('dmbedtls', libraries)
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
                library = self.read_target(root, 'dlib')
                source = next(source for source in library['sources']
                              if source['path'].endswith('/dlib/lz4.cpp'))
                group = library['compileGroups'][source['compileGroupIndex']]
                flags = ' '.join(fragment['fragment'] for fragment in group['compileCommandFragments'])
                self.assertIn('-ffunction-sections', flags)
                self.assertIn('-fdata-sections', flags)
                flags = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments'])
                self.assertIn('-Wl,--gc-sections', flags)

    # Default Linux binaries must start without the Vulkan loader or the X11-XCB bridge.
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
                    self.assertNotIn('X11-xcb', libraries)
                    self.assertNotIn('GLU', libraries)
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
                    adapter = self.read_target(Path(directory), 'graphics_vulkan')
                    dependencies = [item['id'].split('::')[0] for item in adapter.get('dependencies', [])]
                    self.assertIn('platform_sdk_headers', dependencies)
                    self.assertIn('graphics_vulkan', libraries)
                    self.assertNotRegex(libraries, r'\b(?:lib)?graphics\.(?:a|lib)\b')
                    self.assertIn('platform_vulkan', libraries)
                    self.assertNotRegex(libraries, r'\b(?:lib)?platform\.(?:a|lib)\b')
                    for library in ('EGL', 'GLESv1_CM', 'GLESv2'):
                        self.assertNotIn(library, libraries)

    # Both installed macOS window archives need the Cocoa frameworks on the consumer's link line.
    def test_macos_sdk_window_frameworks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'main.c').write_text('int main(void) { return 0; }\n')
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(macos_window_link_test LANGUAGES C)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
foreach(WITH_VULKAN IN ITEMS OFF ON)
    add_executable(consumer_${WITH_VULKAN} main.c)
    defold_target_link_platform(consumer_${WITH_VULKAN} arm64-macos)
endforeach()
''')
            result = self.configure_project(root)
            self.assertEqual(0, result.returncode, result.stdout)
            for name in ('consumer_OFF', 'consumer_ON'):
                consumer = self.read_target(root, name)
                link_command = ' '.join(fragment['fragment'] for fragment in consumer['link']['commandFragments'])
                for framework in ('AppKit', 'CoreGraphics', 'IOKit', 'QuartzCore'):
                    self.assertIn(framework, link_command, name)

    # An installed iOS window archive needs its UIKit, motion and GL dependencies even for a Metal-only simulator.
    def test_ios_sdk_window_frameworks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(ios_window_link_test LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
add_library(consumer INTERFACE)
defold_target_link_platform(consumer arm64_sim-ios SCOPE INTERFACE)
get_target_property(libraries consumer INTERFACE_LINK_LIBRARIES)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libraries" "${libraries}")
''')
            result = self.configure_project(root)
            self.assertEqual(0, result.returncode, result.stdout)
            libraries = (root / 'build/libraries').read_text().split(';')
            for framework in ('UIKit', 'QuartzCore', 'OpenGLES', 'CoreVideo', 'CoreGraphics', 'CoreMotion'):
                self.assertIn('-framework ' + framework, libraries)

    # Installed Win32 archives must bring their system imports, including for OpenGL-only tools.
    def test_win32_sdk_window_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(win32_window_link_test LANGUAGES NONE)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
foreach(backend IN ITEMS opengl vulkan headless)
    set(WITH_VULKAN OFF)
    if(backend STREQUAL "vulkan")
        set(WITH_VULKAN ON)
    elseif(backend STREQUAL "headless")
        set(DEFOLD_PLATFORM_WINDOW_LIBRARY platform_null)
    endif()
    add_library(consumer_${backend} INTERFACE)
    defold_target_link_platform(consumer_${backend} x86_64-win32 SCOPE INTERFACE)
    get_target_property(libs consumer_${backend} INTERFACE_LINK_LIBRARIES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${backend}.txt" "${libs}")
endforeach()
set(DEFOLD_SDK_ROOT "${CMAKE_BINARY_DIR}/sdk")
add_library(standalone_gdc INTERFACE)
defold_target_link_libraries(standalone_gdc x86_64-win32 SCOPE INTERFACE hid mbedtls)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/tools.txt"
    CONTENT "$<TARGET_GENEX_EVAL:standalone_gdc,$<TARGET_PROPERTY:standalone_gdc,INTERFACE_LINK_LIBRARIES>>")
''')
            result = self.configure_project(root)
            self.assertEqual(0, result.returncode, result.stdout)
            imports = {'user32', 'gdi32', 'opengl32', 'imm32', 'dinput8', 'dxguid'}
            for backend, library in [('opengl', 'platform'), ('vulkan', 'platform_vulkan'), ('headless', 'platform_null')]:
                expected = {library} if backend == 'headless' else imports | {library}
                linked = (root / 'build' / f'{backend}.txt').read_text().split(';')
                self.assertEqual(expected, set(linked), backend)
            sdk_lib = root / 'build/sdk/lib/x86_64-win32'
            tools = (root / 'build/tools.txt').read_text().split(';')
            self.assertEqual([(sdk_lib / name).as_posix() for name in ('hid.lib', 'dmbedtls.lib')], tools)

    # The service uses private headers and must work with headless and standalone graphics builds.
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
    file(WRITE "${DEFOLD_SDK_ROOT}/bin/ddfc_cxx" "")
    add_subdirectory("${DEFOLD_HOME}/engine/graphics" graphics)
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" "int main() { return 0; }\\n")
add_executable(headless_consumer "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
defold_target_link_libraries(headless_consumer "${TARGET_PLATFORM}" platform_service graphics_null platform_null)
''')
                service = self.configure_target(root, 'platform_service', IN_TREE_GRAPHICS=in_tree)
                platform_headers = root / 'build/platform/include/platform'
                self.assertTrue((platform_headers / 'platform_app.h').is_file())
                self.assertEqual([], list(platform_headers.glob('platform_service*.h')))
                dependencies = [dependency['id'].split('::')[0]
                                for dependency in service.get('dependencies', [])]
                self.assertEqual(in_tree == 'ON', 'graphics_sdk_headers' in dependencies, dependencies)
                if in_tree == 'ON':
                    headers = subprocess.check_output(
                        ['ninja', '-C', str(root / 'build'), '-t', 'graph', 'graphics_sdk_headers'], text=True)
                    self.assertIn('dlib_sdk_headers', headers)
                    self.assertNotIn('ddf_sdk_headers', headers)
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
