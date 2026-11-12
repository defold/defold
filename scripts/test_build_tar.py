import contextlib
import io
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

import build


class TarTests(unittest.TestCase):
    def setUp(self):
        self.output = io.StringIO()
        self.enterContext(contextlib.redirect_stdout(self.output))
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.verbose = False
        self.configuration.no_colors = True
        self.configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
        self.configuration.tool_checks = build.tool_checks.ToolChecks(self.configuration)
        self.configuration._form_env = lambda: os.environ.copy()

    # Windows must keep using its system tar even when Git's incompatible GNU tar appears first on PATH.
    def test_windows_prefers_system_tar_and_checks_it_once(self):
        system_root = os.path.abspath('Windows')
        path = os.path.join(system_root, 'System32', 'tar.exe')
        with mock.patch.object(build.os, 'name', 'nt'), \
                mock.patch.dict(os.environ, {'SystemRoot': system_root}), \
                mock.patch.object(build.shutil, 'which',
                                  side_effect=lambda name: 'git/tar.exe' if name == 'tar' else path) as which, \
                mock.patch.object(build.run, 'env_command', return_value='bsdtar 3.7.9') as command:
            self.assertEqual(path, self.configuration.get_tar())
            self.assertEqual(path, self.configuration.get_tar())
        which.assert_called_once_with(path)
        command.assert_called_once_with(os.environ, [path, '--version'], silent=True)
        self.assertEqual(1, self.output.getvalue().count('Tar: Found'))

    # Windows installations without a bundled tar must retain the existing PATH fallback.
    def test_windows_falls_back_to_path(self):
        system_root = os.path.abspath('Windows')
        path = os.path.abspath('tools/tar')
        with mock.patch.object(build.os, 'name', 'nt'), \
                mock.patch.dict(os.environ, {'SystemRoot': system_root}), \
                mock.patch.object(build.shutil, 'which', side_effect=[None, path]) as which, \
                mock.patch.object(build.run, 'env_command', return_value='tar (GNU tar) 1.35'):
            self.assertEqual(path, self.configuration.get_tar())
        self.assertEqual([mock.call(os.path.join(system_root, 'System32', 'tar.exe')), mock.call('tar')],
                         which.call_args_list)

    # A relative PATH entry must be resolved before extraction changes the working directory, without repeated checks.
    def test_posix_resolves_and_caches_path_tar(self):
        path = os.path.abspath('tools/tar')
        with mock.patch.object(build.os, 'name', 'posix'), \
                mock.patch.object(build.shutil, 'which', return_value='tools/tar') as which, \
                mock.patch.object(build.run, 'env_command', return_value='tar (GNU tar) 1.35') as command:
            self.assertEqual(path, self.configuration.get_tar())
            self.assertEqual(path, self.configuration.get_tar())
        which.assert_called_once_with('tar')
        command.assert_called_once_with(os.environ, [path, '--version'], silent=True)

    # Both extraction paths must merge package directories and accept native Windows drive-letter archive paths.
    @unittest.skipUnless(build.shutil.which('tar'), 'Tar is required')
    def test_extracts_packages_with_shared_directories(self):
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='tar extraction '))
        root = Path(directory).resolve()
        for name in ('first', 'second'):
            source = root / name / 'sdk'
            source.mkdir(parents=True)
            (source / f'{name}.txt').write_text(name)
            archive = root / f'{name}.tar.gz'
            with tarfile.open(archive, 'w:gz') as package:
                package.add(source, arcname='sdk')
            self.configuration._extract_tgz(str(archive), str(root / 'installed'))
            with contextlib.chdir(root):
                self.configuration._extract_tgz_rename_folder(str(archive), str(root / 'renamed'))
        for name in ('first', 'second'):
            self.assertEqual(name, (root / 'installed' / 'sdk' / f'{name}.txt').read_text())
            self.assertEqual(name, (root / 'renamed' / f'{name}.txt').read_text())


if __name__ == '__main__':
    unittest.main()
