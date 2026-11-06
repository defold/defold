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

    # GNU tar must treat drive-letter paths as local, with discovery and version checks cached for the session.
    def test_gnu_tar_uses_local_mode_and_is_checked_once(self):
        path = os.path.abspath('tools/tar')
        with mock.patch.object(build.shutil, 'which', return_value='tools/tar') as which, \
                mock.patch.object(build.run, 'env_command', return_value='tar (GNU tar) 1.35') as command:
            self.assertEqual([path, '--force-local'], self.configuration.get_tar())
            self.assertEqual([path, '--force-local'], self.configuration.get_tar())
        which.assert_called_once_with('tar')
        command.assert_called_once_with(os.environ, [path, '--version'], silent=True)
        self.assertEqual(1, self.output.getvalue().count('Tar: Found'))

    # BSD tar must remain usable without the unsupported GNU-specific local-mode flag.
    def test_bsd_tar_does_not_receive_gnu_options(self):
        path = os.path.abspath('tools/tar')
        with mock.patch.object(build.shutil, 'which', return_value='tools/tar'), \
                mock.patch.object(build.run, 'env_command', return_value='bsdtar 3.7.9 - libarchive 3.7.9'):
            self.assertEqual([path], self.configuration.get_tar())

    # Both extraction paths must open colon-containing local archives, including native Windows drive-letter paths.
    @unittest.skipUnless(build.shutil.which('tar'), 'Tar is required')
    def test_extracts_local_archives_containing_colons(self):
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='tar extraction '))
        root = Path(directory).resolve()
        payload = root / 'payload.txt'
        payload.write_text('installed')
        archive = root / ('SDK.tar.gz' if os.name == 'nt' else 'C:SDK.tar.gz')
        with tarfile.open(archive, 'w:gz') as package:
            package.add(payload, arcname='sdk/installed.txt')
        source = str(archive) if os.name == 'nt' else archive.name

        self.configuration._extract_tgz(source, str(root))
        self.assertEqual('installed', (root / 'sdk' / 'installed.txt').read_text())
        with contextlib.chdir(root):
            self.configuration._extract_tgz_rename_folder(source, str(root / 'renamed'))
        self.assertEqual('installed', (root / 'renamed' / 'installed.txt').read_text())


if __name__ == '__main__':
    unittest.main()
