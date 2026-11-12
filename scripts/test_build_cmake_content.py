import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / 'engine/engine/scripts/build_cmake_content.py'
SPEC = importlib.util.spec_from_file_location('build_cmake_content', SCRIPT)
content = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(content)


class ContentCommandTests(unittest.TestCase):
    # Failed tools must identify their working directory and full arguments for reproduction.
    def test_failed_command_reports_context(self):
        with tempfile.TemporaryDirectory(prefix='content build ') as directory:
            args = [sys.executable, '-c', 'import sys; sys.exit(7)', 'input with spaces']
            output = io.StringIO()
            with contextlib.redirect_stderr(output), self.assertRaises(subprocess.CalledProcessError) as raised:
                content.run(args, Path(directory))
            self.assertEqual(7, raised.exception.returncode)
            self.assertIn('exit code 7 (0x00000007)', output.getvalue())
            self.assertIn('Working directory: ' + directory, output.getvalue())
            self.assertIn('Command: ', output.getvalue())
            for arg in args:
                self.assertIn(arg, output.getvalue())

    # Windows crash statuses must remain readable even when a wrapper later reduces the exit code.
    def test_native_crash_status_is_logged_and_preserved(self):
        for status in (0xC00000FD, -1073741571):
            with self.subTest(status=status):
                error = subprocess.CalledProcessError(status, ['java'])
                output = io.StringIO()
                with mock.patch.object(content.subprocess, 'check_call', side_effect=error), \
                        contextlib.redirect_stderr(output), \
                        self.assertRaises(subprocess.CalledProcessError) as raised:
                    content.run(['java'], Path('content-work'))
                self.assertIs(error, raised.exception)
                self.assertIn('0xC00000FD', output.getvalue())


if __name__ == '__main__':
    unittest.main()
