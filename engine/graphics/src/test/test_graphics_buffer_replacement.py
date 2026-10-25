#!/usr/bin/env python3
"""Validate the native buffer replacement regression runner."""
import contextlib
import io
import subprocess
import unittest
from unittest import mock


class GraphicsBufferReplacementTest(unittest.TestCase):
    # Buffer regressions must fail on crashes, validation errors or missing completion,
    # even if the launcher reports success, and must invoke the intended CLI mode.
    def test_buffer_replacement_runner(self):
        import run_graphics_buffer_replacement as buffers
        for code, log, expected in (
            (0, 'GRAPHICS_BUFFER_REPLACEMENT_RESULT=0\n', 0),
            (1, 'GRAPHICS_BUFFER_REPLACEMENT_RESULT=0\n', 1),
            (0, 'GRAPHICS_BUFFER_REPLACEMENT_RESULT=1\n', 1),
            (0, 'app crashed\n', 1),
            (0, 'GRAPHICS_BUFFER_REPLACEMENT_RESULT=0\nERROR:GRAPHICS: invalid buffer\n', 1),
        ):
            with self.subTest(code=code, log=log):
                with mock.patch.object(buffers.subprocess, 'run', return_value=subprocess.CompletedProcess([], code, log)) as run:
                    with contextlib.redirect_stdout(io.StringIO()):
                        result = buffers.main(['--executable', 'capture', '--target-platform', 'arm64-macos', '--available', 'metal'])
                    self.assertEqual(expected, result)
                    self.assertEqual(['metal', 'buffer-replacement'], run.call_args.args[0][1:])

    # Unsupported builds and the explicit hosted-CI policy must not launch a GPU test.
    def test_buffer_replacement_runner_skip(self):
        import run_graphics_buffer_replacement as buffers
        for extra in (['--available', 'vulkan'], ['--available', 'metal', '--skip-all', 'CI policy']):
            with mock.patch.object(buffers.subprocess, 'run') as run:
                with contextlib.redirect_stdout(io.StringIO()):
                    result = buffers.main(['--executable', 'capture', '--target-platform', 'arm64-macos'] + extra)
                self.assertEqual(0, result)
                run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
