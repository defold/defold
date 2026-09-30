import contextlib
import io
import os
import shutil
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build
from BuildTimeTracker import BuildTimeTracker


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class ExternalPackageTests(unittest.TestCase):
    def setUp(self):
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        self.root = Path(temporary_directory.name)
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(self.root)
        self.configuration.dynamo_home = str(self.root / 'sdk')
        self.configuration.ext = str(self.root / 'sdk/ext')
        self.configuration.host = build.get_host_platform()
        self.configuration.build_options = []
        self.configuration.verbose = False
        self.configuration.build_tracker = BuildTimeTracker(logger=lambda message: None)
        self.configuration._form_env = os.environ.copy

        protoc = 'protoc.exe' if self.configuration.host.endswith('-win32') else 'protoc'
        protoc_path = Path(self.configuration.ext) / 'bin' / self.configuration.host / protoc
        protoc_path.parent.mkdir(parents=True)
        protoc_path.touch()

    def build_package(self, package, platform, files):
        source_dir = self.root / 'external' / package
        source_dir.mkdir(parents=True, exist_ok=True)
        cmake_lines = [
            'cmake_minimum_required(VERSION 4.0)',
            'project(package_fixture LANGUAGES NONE)',
            'set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}")',
        ]
        for name, contents in files.items():
            source = source_dir / name
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text(contents)
            cmake_lines.append('install(FILES "%s" DESTINATION "%s")' % (name, Path(name).parent.as_posix()))
        (source_dir / 'CMakeLists.txt').write_text('\n'.join(cmake_lines) + '\n')
        self.configuration.target_platform = platform
        self.configuration.external_package = package
        with contextlib.redirect_stdout(io.StringIO()):
            self.configuration.build_external()

    def read_package(self, filename):
        with tarfile.open(self.root / 'packages' / filename) as archive:
            return {member.name: archive.extractfile(member).read().decode()
                    for member in archive
                    if member.isfile() and not Path(member.name).name.startswith('._')}

    # Verifies install_ext supplies Android GLFW libraries and Java classes without a prebuilt archive.
    def test_install_ext_builds_android_glfw_from_source(self):
        source_dir = self.root / 'external'
        source_dir.mkdir()
        repository_root = Path(build.__file__).resolve().parent.parent
        shutil.copyfile(repository_root / 'external/CMakeLists.txt', source_dir / 'CMakeLists.txt')

        cmake_modules = self.root / 'scripts/cmake'
        cmake_modules.mkdir(parents=True)
        (cmake_modules / 'defold.cmake').write_text('set(DEFOLD_LANGUAGE_LIST NONE)\n')
        for library in ('bullet3d', 'basisu', 'lz4'):
            library_dir = source_dir / library
            library_dir.mkdir()
            (library_dir / 'CMakeLists.txt').touch()

        glfw_dir = source_dir / 'glfw'
        glfw_dir.mkdir()
        (glfw_dir / 'CMakeLists.txt').write_text('''
foreach(library IN ITEMS dmglfw dmglfw_vulkan)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lib${library}.a" "${TARGET_PLATFORM}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lib${library}.a" DESTINATION "lib/${TARGET_PLATFORM}")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/glfw_android.jar" "Java classes")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/glfw_android.jar" DESTINATION share/java)
''')

        self.configuration.dmsdk = str(self.root / 'sdk/sdk')
        self.configuration.get_base_platforms = lambda: [self.configuration.host]
        self.configuration.check_sdk = lambda: None
        self.configuration._extract_tgz = mock.Mock()
        self.configuration._install_python_packages = lambda packages: None
        self.configuration._copy = lambda *args: None
        for platform in ('armv7-android', 'arm64-android', 'x86_64-android'):
            with self.subTest(platform=platform):
                self.configuration.target_platform = platform
                self.configuration._extract_tgz.reset_mock()
                with contextlib.redirect_stdout(io.StringIO()):
                    self.configuration.install_ext()
                installed = Path(self.configuration.ext)
                for library in ('dmglfw', 'dmglfw_vulkan'):
                    self.assertEqual(platform, (installed / 'lib' / platform / ('lib' + library + '.a')).read_text())
                self.assertEqual('Java classes', (installed / 'share/java/glfw_android.jar').read_text())
                extracted_archives = [Path(call.args[0]).name for call in self.configuration._extract_tgz.call_args_list]
                self.assertFalse(any(name.startswith('glfw-2.7.1-') and name.endswith('-android.tar.gz')
                                     for name in extracted_archives))

    def test_read_package_ignores_appledouble_metadata(self):
        (self.root / 'packages').mkdir()
        files = {
            'include/fixture.h': b'header',
            'include/._fixture.h': b'\x00\x05\x16\x07\xff',
            '._include': b'\x00\x05\x16\x07\xff',
        }
        with tarfile.open(self.root / 'packages/metadata.tar.gz', 'w:gz') as archive:
            for name, contents in files.items():
                member = tarfile.TarInfo(name)
                member.size = len(contents)
                archive.addfile(member, io.BytesIO(contents))
        self.assertEqual({'include/fixture.h': 'header'}, self.read_package('metadata.tar.gz'))

    def test_common_headers_are_separate_from_platform_libraries(self):
        cases = (
            ('opus', 'opus-1.5.2', 'include/opus/opus.h', 'libopus.a'),
            ('harfbuzz', 'harfbuzz-13.2.1', 'include/harfbuzz/hb.h', 'libharfbuzz.a'),
            ('sheenbidi', 'SheenBidi-2.9.0', 'include/SheenBidi/SheenBidi.h', 'libsheenbidi.a'),
            ('libunibreak', 'libunibreak-6.1', 'include/libunibreak/linebreak.h', 'libunibreak.a'),
            ('box2d', 'box2d-3.1.0', 'include/box2d/box2d.h', 'libbox2d.a'),
        )
        for package, archive_name, header, library in cases:
            with self.subTest(package=package):
                headers = {header: 'common header'}
                libraries = {'lib/arm64-macos/' + library: 'platform archive'}
                self.build_package(package, 'arm64-macos', headers | libraries)
                self.assertEqual(headers, self.read_package(archive_name + '-common.tar.gz'))
                self.assertEqual(libraries, self.read_package(archive_name + '-arm64-macos.tar.gz'))

    def test_glfw_keeps_headers_and_support_files_in_platform_archive(self):
        cases = (
            ('arm64-android', {
                'lib/arm64-android/libdmglfw_vulkan.a': 'Vulkan archive',
                'share/java/glfw_android.jar': 'Android Java classes',
            }),
            ('wasm-web', {'lib/wasm-web/js/library_glfw.js': 'JavaScript glue'}),
        )
        for platform, support_files in cases:
            with self.subTest(platform=platform):
                files = {
                    'include/glfw/glfw.h': 'GLFW header',
                    'lib/' + platform + '/libdmglfw.a': 'GLFW archive',
                } | support_files
                self.build_package('glfw', platform, files)
                self.assertEqual(files, self.read_package('glfw-2.7.1-' + platform + '.tar.gz'))
                self.assertFalse((self.root / 'packages/glfw-2.7.1-common.tar.gz').exists())


if __name__ == '__main__':
    unittest.main()
