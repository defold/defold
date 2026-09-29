# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class ReleaseNotesTests(unittest.TestCase):
    # Verify the workflow's three downloaded scripts start without pip packages or other build tools.
    def test_standalone_cli(self):
        repository_root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for source in ('scripts/check_release_notes.py',
                           'scripts/releasenotes_github_projectv2.py',
                           'build_tools/github.py'):
                target = root / source
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(repository_root / source, target)
            result = subprocess.run(
                [sys.executable, '-E', '-S', '-B', str(root / 'scripts/check_release_notes.py'), '--help'],
                cwd=root, capture_output=True, text=True, timeout=30)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('--pull-request', result.stdout)


if __name__ == '__main__':
    unittest.main()
