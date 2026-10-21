import json
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class PlatformLinkTests(unittest.TestCase):
    def configure_target(self, root, target_name, **definitions):
        repository = Path(__file__).resolve().parent.parent
        build = root / 'build'
        query = build / '.cmake/api/v1/query'
        query.mkdir(parents=True)
        (query / 'codemodel-v2').touch()
        result = subprocess.run([
            'cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
            '-DDEFOLD_HOME=' + repository.as_posix(),
            '-DDEFOLD_BUILD_HOME=' + root.as_posix(),
            *[f'-D{name}={value}' for name, value in definitions.items()],
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.assertEqual(0, result.returncode, result.stdout)
        reply = build / '.cmake/api/v1/reply'
        codemodel = json.loads(next(reply.glob('codemodel-v2-*.json')).read_text())
        target = next(target for target in codemodel['configurations'][0]['targets']
                      if target['name'] == target_name)
        return json.loads((reply / target['jsonFile']).read_text())

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

    # Desktop cross-builds need a target DXC library as well as the host copy used by build tools.
    def test_macos_cross_build_includes_dxc(self):
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
# Exercise the real dependency selection and DXC wrapper without configuring
# unrelated packages or downloading/compiling LLVM in this configure-only test.
function(add_subdirectory directory)
    if(directory STREQUAL "dxc")
        _add_subdirectory("${DEFOLD_HOME}/external/dxc" "${CMAKE_BINARY_DIR}/dxc")
    endif()
endfunction()
include("${DEFOLD_HOME}/external/CMakeLists.txt")
''')
                self.configure_target(root, 'dxc_build_external', TARGET_PLATFORM=platform)
                command = (root / 'build/dxc/dxc_build_external-prefix/tmp/dxc_build_external-cfgcmd.txt').read_text()
                architecture = platform.split('-')[0]
                self.assertIn('-DCMAKE_OSX_ARCHITECTURES=' + architecture, command)
                install = (root / 'build/dxc/cmake_install.cmake').read_text()
                self.assertIn('/lib/' + platform, install)
                self.assertIn('dxcompiler', install)

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
