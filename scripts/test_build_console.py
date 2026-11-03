import contextlib
import io
import json
import os
from pathlib import Path
import shlex
import sys
import tempfile
import unittest
from unittest import mock

import build
import build_console
import build_output


class Terminal(io.StringIO):
    def isatty(self):
        return True


class BuildConsoleTests(unittest.TestCase):
    # Build-owned headings, completions, warnings, and summaries share TTY colors and every color opt-out.
    def test_build_steps_and_summary_share_color_policy(self):
        cases = ((True, {}, False, True), (False, {}, False, False),
                 (True, {}, True, False), (True, {'NO_COLOR': ''}, False, False),
                 (True, {'NOCOLOR': '1'}, False, False), (True, {'CLICOLOR': '0'}, False, False))
        for terminal, overrides, no_colors, expect_colors in cases:
            with self.subTest(terminal=terminal, overrides=overrides, no_colors=no_colors), \
                    tempfile.TemporaryDirectory() as directory, mock.patch.dict(os.environ):
                for key in ('NO_COLOR', 'NOCOLOR', 'CLICOLOR', 'CLICOLOR_FORCE'):
                    os.environ.pop(key, None)
                os.environ.update(overrides)
                stream = Terminal() if terminal else io.StringIO()
                configuration = build.Configuration.__new__(build.Configuration)
                configuration.no_colors = no_colors
                path = Path(directory) / 'build_times.json'
                tracker = build.BuildTimeTracker(output_file=str(path), logger=configuration._log,
                                                colorize=configuration._colorize)
                with contextlib.redirect_stdout(stream):
                    tracker.start_component('cmake_engine_libs', 'arm64-macos')
                    tracker.start_command('CMake build engine_libs')
                    configuration._warn('example warning')
                    configuration._log_command([], environment={'DM_BOB_ROOTFOLDER': '/tmp/bob light'})
                    tracker.end_command('CMake build engine_libs')
                    tracker.end_component('cmake_engine_libs', 'arm64-macos')
                    tracker.start_command('Gradle bob-light', quiet=True)
                    tracker.end_command('Gradle bob-light', quiet=True)
                    tracker.print_summary()
                    tracker.save_times()
                    configuration._log_step('Done', '1;32')
                text = stream.getvalue()
                self.assertRegex(text, r'(?m)^  CMake build engine_libs +[\d.]+ s$')
                self.assertRegex(text, r'(?m)^  Gradle bob-light +[\d.]+ s$')
                if expect_colors:
                    self.assertIn('\x1b[36mDEFOLD - \x1b[0m\x1b[1;36mStarting', text)
                    self.assertIn('\x1b[1;38;5;130mBuilding cmake_engine_libs for arm64-macos\x1b[0m', text)
                    self.assertIn("\x1b[1;36mStarting 'CMake build engine_libs'", text)
                    self.assertIn("\x1b[32m'CMake build engine_libs' completed", text)
                    self.assertIn('\x1b[33mexample warning\x1b[0m', text)
                    self.assertIn('\x1b[1;38;5;130menv\x1b[0m \x1b[1;38;5;29mDM_BOB_ROOTFOLDER\x1b[0m=', text)
                    self.assertIn('\x1b[1;36mBUILD TIME SUMMARY\x1b[0m', text)
                    self.assertIn('\x1b[1;32mDone\x1b[0m', text)
                else:
                    self.assertNotIn('\x1b', text)
                plain = build_output._ANSI.sub('', text)
                self.assertIn("DEFOLD - Starting 'CMake build engine_libs'", plain)
                self.assertIn('DEFOLD - Building cmake_engine_libs for arm64-macos', plain)
                self.assertIn('DEFOLD - example warning', plain)
                self.assertIn('DEFOLD - env DM_BOB_ROOTFOLDER=', plain)
                self.assertIn('DEFOLD - BUILD TIME SUMMARY', plain)
                self.assertIn('DEFOLD - Done', plain)
                self.assertIn('COMMAND TIMES:', plain)
                header = next(line for line in plain.splitlines() if line.startswith('COMPONENTS:'))
                self.assertEqual(len('cmake_engine_libs '), header.index('arm64-macos'))
                summary = json.loads(path.read_text())
                self.assertIn('CMake build engine_libs', summary['commands'])
                self.assertIn('Gradle bob-light', summary['commands'])
                self.assertIn('cmake_engine_libs', summary['components'])
                self.assertNotIn('\\u001b', path.read_text())

    # Shell-style logging must preserve argument boundaries and literal environment values containing shell syntax.
    @unittest.skipIf(os.name == 'nt', 'This checks POSIX shell quoting')
    def test_shell_command_formatting_preserves_quoting(self):
        environment = {'ROOT': "/tmp/bob light; literal $HOME=it's", 'EMPTY': ''}
        args = ['cmake', '--build', '/tmp/build path', '-DNAME=with spaces', '--', 'target name']
        paint = lambda text, code: build_console.colorize(text, code, enabled=True)
        text = build_console.format_shell_command(args, environment, paint)
        plain = build_output._ANSI.sub('', text)
        self.assertEqual(['env', *(f'{name}={value}' for name, value in environment.items()), *args],
                         shlex.split(plain))
        self.assertIn('\x1b[1;38;5;130menv\x1b[0m', text)
        self.assertIn('\x1b[1;38;5;29mROOT\x1b[0m=', text)
        self.assertIn('\x1b[36m--build\x1b[0m', text)
        self.assertIn('\x1b[36m-DNAME\x1b[0m=', text)

    # Verbose command highlighting must use redacted arguments and must never change the subprocess arguments.
    def test_command_echo_highlighting_preserves_redaction(self):
        stream = Terminal()
        env = os.environ.copy()
        for key in ('NO_COLOR', 'NOCOLOR', 'CLICOLOR', 'CLICOLOR_FORCE'):
            env.pop(key, None)
        args = [sys.executable, '-c', 'import sys; print(" ".join(sys.argv[1:]))',
                '--token', 'private-test-token', '--output=/tmp/output path']
        with contextlib.redirect_stdout(stream):
            result = build.run.env_command(env, args)
        text = stream.getvalue()
        self.assertNotIn('private-test-token', text)
        self.assertNotIn('private-test-token', result)
        self.assertIn('--token *** --output=/tmp/output path', result)
        self.assertIn('\x1b[1;38;5;130m', text)
        self.assertIn('\x1b[36m--token\x1b[0m', text)
        self.assertIn('\x1b[36m--output\x1b[0m=', text)

    # Progress colors only the ownership prefix; completion statuses retain their own colors and fit the terminal.
    def test_colored_progress_stays_within_terminal_width(self):
        with mock.patch.object(build_output.shutil, 'get_terminal_size', return_value=os.terminal_size((50, 24))):
            for success in (True, False):
                with self.subTest(success=success):
                    stream = Terminal()
                    log = io.StringIO()
                    log.name = 'build.log'
                    output = build_output.BuildOutput('CMake build engine_libs', log, stream=stream, env={})
                    output.on_output('[defold-build:1/2] Build object')
                    output.finish(success)
                    text = stream.getvalue()
                    self.assertIn('\x1b[36mDEFOLD - \x1b[0m', text)
                    self.assertIn('\x1b[36mDEFOLD - \x1b[0mCMake build engine_libs', text)
                    self.assertNotIn('\x1b[36mCMake build engine_libs', text)
                    color = '32' if success else '31'
                    status = 'done' if success else 'FAILED'
                    self.assertIn(f'\x1b[{color}mCMake build engine_libs: {status}', text)
                    self.assertEqual(1 if success else 2, text.count('\n'))
                    plain = build_output._ANSI.sub('', text)
                    self.assertLessEqual(max(len(frame) for frame in plain.splitlines()), 49)


if __name__ == '__main__':
    unittest.main()
