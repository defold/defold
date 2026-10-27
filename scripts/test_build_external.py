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

    # Explicit DXC packaging must retain the executable and runtime DLLs alongside headers and the import library.
    def test_windows_dxc_archive_includes_runtime_files(self):
        configuration = self.configuration
        configuration.target_platform = 'x86_64-win32'
        configuration.external_package = 'dxc'
        configuration.build_options = []
        configuration.verbose = False
        configuration.build_tracker = mock.Mock()
        configuration._form_env = lambda: {}
        source = self.root / 'external' / 'dxc'
        source.mkdir(parents=True)
        (source / 'CMakeLists.txt').touch()
        files = {
            'bin/x86_64-win32/dxc.exe': b'compiler executable',
            'bin/x86_64-win32/dxcompiler.dll': b'compiler runtime',
            'bin/x86_64-win32/dxil.dll': b'validator runtime',
            'lib/x86_64-win32/dxcompiler.lib': b'import library',
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

        package = self.root / 'packages' / 'dxc-1.9.2609-x86_64-win32.tar.gz'
        with tarfile.open(package) as archive:
            contents = {member.name: archive.extractfile(member).read()
                        for member in archive.getmembers() if member.isfile()}
        self.assertEqual(files, contents)

    # Every host needs Windows Bob tools, but installing them must never request nonexistent DXC archives for other platforms.
    def test_install_requests_only_windows_dxc_package(self):
        cases = (
            ('x86_64-win32', 'x86_64-win32'),
            ('x86_64-win32', 'arm64-android'),
            ('x86_64-linux', 'x86_64-linux'),
            ('arm64-linux', 'arm64-android'),
            ('x86_64-macos', 'arm64-android'),
            ('arm64-macos', 'arm64-macos'),
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
                self.assertEqual([('dxc-1.9.2609-x86_64-win32.tar.gz', configuration.ext)], dxc_packages)


if __name__ == '__main__':
    unittest.main()
