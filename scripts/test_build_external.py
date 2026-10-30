import contextlib
import io
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build


class ExternalPackageTests(unittest.TestCase):
    def setUp(self):
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        self.root = Path(temporary_directory.name)
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(self.root)
        self.configuration.dynamo_home = str(self.root / 'sdk')
        self.configuration.ext = str(self.root / 'sdk' / 'ext')
        self.configuration.dmsdk = str(self.root / 'sdk' / 'sdk')
        self.configuration._log = lambda message: None

    def check_dxc_archive(self, platform, platform_files):
        configuration = self.configuration
        configuration.target_platform = platform
        configuration.external_package = 'dxc'
        configuration.build_options = []
        configuration.verbose = False
        configuration.build_tracker = mock.Mock()
        configuration._form_env = lambda: {}
        source = self.root / 'external' / 'dxc'
        source.mkdir(parents=True, exist_ok=True)
        (source / 'CMakeLists.txt').touch()
        files = {
            **platform_files,
            'include/dxc/dxcapi.h': b'compiler header',
            'share/licenses/dxc/LICENSE.TXT': b'upstream license',
        }
        install_directory = None

        def run_cmake(env, args, **kwargs):
            nonlocal install_directory
            if '--build' not in args:
                prefix = '-DDEFOLD_EXTERNAL_INSTALL_PREFIX='
                install_directory = Path(next(arg.removeprefix(prefix) for arg in args if arg.startswith(prefix)))
            else:
                for relative_path, contents in files.items():
                    path = install_directory / relative_path
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(contents)

        with contextlib.redirect_stdout(io.StringIO()), \
                mock.patch.object(build.run, 'env_command', side_effect=run_cmake), \
                mock.patch.object(build.build_private, 'get_external_package_name',
                                  side_effect=lambda platform, library, default: default):
            configuration.build_external()

        package = self.root / 'packages' / ('dxc-1.9.2609-' + platform + '.tar.gz')
        with tarfile.open(package) as archive:
            contents = {member.name: archive.extractfile(member).read()
                        for member in archive.getmembers() if member.isfile()}
        self.assertEqual(files, contents)

    # Explicit Windows packaging must retain runtime DLLs alongside the executable, headers and import library.
    def test_windows_dxc_archive_includes_runtime_files(self):
        self.check_dxc_archive('x86_64-win32', {
            'bin/x86_64-win32/dxc.exe': b'compiler executable',
            'bin/x86_64-win32/dxcompiler.dll': b'compiler runtime',
            'bin/x86_64-win32/dxil.dll': b'validator runtime',
            'lib/x86_64-win32/dxcompiler.lib': b'import library',
        })

    # Both macOS package architectures must retain the executable and shared library needed by Bob and shaderc.
    def test_macos_dxc_archives_include_runtime_files(self):
        for platform in ('arm64-macos', 'x86_64-macos'):
            with self.subTest(platform=platform):
                self.check_dxc_archive(platform, {
                    'bin/' + platform + '/dxc': b'compiler executable',
                    'lib/' + platform + '/libdxcompiler.dylib': b'compiler library',
                })

    # Linux archives must include the compiler and shared library in both architecture-specific install paths.
    def test_linux_dxc_archives_include_runtime_files(self):
        for platform in ('arm64-linux', 'x86_64-linux'):
            with self.subTest(platform=platform):
                self.check_dxc_archive(platform, {
                    'bin/' + platform + '/dxc': b'compiler executable',
                    'lib/' + platform + '/libdxcompiler.so': b'compiler library',
                })

    # Default package builds include DXC on every desktop platform while skipping mobile and web targets.
    def test_default_build_selects_dxc_only_for_packaged_platforms(self):
        configuration = self.configuration
        configuration.external_package = None
        for platform in ('arm64-macos', 'x86_64-macos', 'x86_64-win32',
                         'arm64-linux', 'x86_64-linux', 'arm64-android', 'wasm-web'):
            with self.subTest(platform=platform):
                configuration.target_platform = platform
                with mock.patch.object(configuration, '_build_external_lib_cmake') as build_library:
                    configuration.build_external()
                dxc_calls = [call for call in build_library.call_args_list if call.args[0] == 'dxc']
                expected = [mock.call('dxc', platform)] if platform.endswith(('macos', 'linux', 'win32')) else []
                self.assertEqual(expected, dxc_calls)

    # Every host needs each desktop Bob tool archive exactly once, including Linux hosts and cross-builds.
    def test_install_requests_only_packaged_dxc_platforms(self):
        cases = (
            ('x86_64-win32', 'x86_64-win32'),
            ('x86_64-win32', 'arm64-android'),
            ('x86_64-linux', 'x86_64-linux'),
            ('arm64-linux', 'arm64-linux'),
            ('x86_64-linux', 'arm64-linux'),
            ('arm64-linux', 'x86_64-linux'),
            ('arm64-linux', 'arm64-android'),
            ('x86_64-macos', 'arm64-android'),
            ('arm64-macos', 'arm64-macos'),
            ('arm64-macos', 'x86_64-macos'),
            ('x86_64-macos', 'arm64-macos'),
        )
        configuration = self.configuration
        configuration._install_python_packages = mock.Mock()
        configuration._copy = mock.Mock()
        for host, target in cases:
            with self.subTest(host=host, target=target):
                configuration.host = host
                configuration.target_platform = target
                with contextlib.redirect_stdout(io.StringIO()), \
                        mock.patch.object(configuration, '_extract_tgz') as extract, \
                        mock.patch.object(build.build_private, 'get_install_host_packages', return_value=[]), \
                        mock.patch.object(build.build_private, 'get_install_target_packages', return_value=[]):
                    configuration.install_ext_packages()
                dxc_packages = [(Path(call.args[0]).name, call.args[1])
                                for call in extract.call_args_list
                                if Path(call.args[0]).name.startswith('dxc-')]
                self.assertCountEqual([
                    ('dxc-1.9.2609-' + platform + '.tar.gz', configuration.ext)
                    for platform in ('arm64-macos', 'x86_64-macos', 'arm64-linux', 'x86_64-linux', 'x86_64-win32')
                ], dxc_packages)


if __name__ == '__main__':
    unittest.main()
