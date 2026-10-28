import contextlib
import io
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

import build
import build_output
import run


class Terminal(io.StringIO):
    def isatty(self):
        return True


class AsciiStream(io.StringIO):
    @property
    def encoding(self):
        return 'ascii'


class BuildOutputTests(unittest.TestCase):
    def reporter(self, terminal=False):
        log = io.StringIO()
        log.name = 'build.log'
        stream = Terminal() if terminal else io.StringIO()
        self.now = 0
        output = build_output.BuildOutput('Build engine', log, stream=stream, clock=lambda: self.now)
        return output, stream, log

    # Thousands of successful Ninja edges produce a few CI progress snapshots and a complete file log.
    def test_ci_progress_is_sparse_and_keeps_full_log(self):
        output, stream, log = self.reporter()
        for finished in range(1, 1001):
            output.on_output(f'[defold-build:{finished}/1000] Building object {finished}')
            output.on_output(f'successful tool output {finished}')
        output.finish(True)
        text = stream.getvalue()
        self.assertLessEqual(len(text.splitlines()), 13)
        self.assertIn('100% (1000/1000)', text)
        self.assertNotIn('successful tool output', text)
        self.assertNotIn('Building object', text)
        self.assertNotIn('\r', text)
        self.assertNotIn('\x1b', text)
        self.assertEqual(2000, len(log.getvalue().splitlines()))

    # A terminal redraws one bounded line and finishes it before subsequent output.
    def test_terminal_progress_reuses_one_line(self):
        output, stream, _ = self.reporter(terminal=True)
        with mock.patch.object(build_output.shutil, 'get_terminal_size', return_value=os.terminal_size((70, 24))):
            for finished in range(1, 101):
                self.now += 0.02
                output.on_output(f'[defold-build:{finished}/100] Building object')
            output.finish(True)
        text = stream.getvalue()
        self.assertIn('\r', text)
        self.assertEqual(1, text.count('\n'))
        self.assertIn('100% (100/100)', text)
        self.assertLess(text.count('\r'), 30)
        visible = build_output._ANSI.sub('', text)
        self.assertLessEqual(max(len(frame.rstrip('\n')) for frame in visible.split('\r')), 69)

    # Progress continues after CMake regeneration or dynamic dependencies change Ninja's total.
    def test_progress_handles_total_changes(self):
        output, stream, _ = self.reporter()
        for line in ('[defold-build:1/1] Regenerating', '[defold-build:1/100] Compiling',
                     '[defold-build:10/100] Compiling'):
            output.on_output(line)
        output.finish(True)
        self.assertIn('  1% (1/100)', stream.getvalue())
        self.assertIn(' 10% (10/100)', stream.getvalue())

    # Long-running phases can emit a heartbeat without flooding CI while their percentage is unchanged.
    def test_ci_progress_reports_liveness(self):
        output, stream, _ = self.reporter()
        output.on_output('[defold-build:1/1000] Compiling')
        self.now = 31
        output.on_output('[defold-build:2/1000] Compiling')
        self.assertIn('(2/1000)', stream.getvalue())

    # Failures retain both output streams and every failed edge while excluding parallel successful edges.
    def test_failures_exclude_successful_command_output(self):
        output, stream, log = self.reporter()
        lines = (
            '[defold-build:1/4] First successful edge', 'unrelated warning from successful edge',
            '[defold-build:2/4] First failed edge', 'FAILED: first.o', 'compiler first.cpp',
            'first stdout diagnostic', 'first.cpp:7: error: missing semicolon',
            '[defold-build:3/4] Another successful edge', 'unrelated successful output',
            '[defold-build:4/4] Second failed edge', 'FAILED: second.o', 'compiler second.cpp',
            'second.cpp(3): error C2001: invalid input', 'ninja: build stopped: subcommand failed.',
        )
        for line in lines:
            output.on_output(line)
        output.finish(False)
        text = stream.getvalue()
        self.assertIn('FAILED: first.o', text)
        self.assertIn('FAILED: second.o', text)
        self.assertLess(text.index('first stdout diagnostic'), text.index('first.cpp:7: error:'))
        self.assertNotIn('unrelated', text)
        self.assertIn('Full log: build.log', text)
        self.assertIn('unrelated successful output', log.getvalue())

    # Ninja console-pool diagnostics preceding FAILED must remain visible, including unstructured errors.
    def test_console_failure_preserves_output_before_failed_marker(self):
        output, stream, _ = self.reporter()
        for line in ('unrelated startup output', '[defold-build:0/1] Console task',
                     'The signing service is unavailable', 'FAILED: sign', 'sign binary',
                     'ninja: build stopped: subcommand failed.'):
            output.on_output(line)
        output.finish(False)
        self.assertIn('The signing service is unavailable', stream.getvalue())
        self.assertNotIn('unrelated startup output', stream.getvalue())

    # A later console failure keeps its own preamble without pulling in successful output after failures.
    def test_multiple_console_failures_keep_their_own_output(self):
        output, stream, _ = self.reporter()
        for line in ('[defold-build:0/3] First console task', 'first console diagnostic',
                     'FAILED: first', 'first command', '[defold-build:1/3] Second console task',
                     'second console diagnostic', 'FAILED: second', 'second command',
                     '[defold-build:3/3] Successful task', 'unrelated successful warning',
                     'ninja: build stopped: subcommand failed.'):
            output.on_output(line)
        output.finish(False)
        text = stream.getvalue()
        self.assertIn('first console diagnostic', text)
        self.assertIn('second console diagnostic', text)
        self.assertNotIn('unrelated successful warning', text)

    # Test-suite progress hides passing test logs while preserving failed test output before Ninja fails.
    def test_test_failures_exclude_passing_test_logs(self):
        output, stream, _ = self.reporter()
        for line in ('[defold-build:0/1] Run tests', 'Starting run_first (group first)',
                     '[1/3] run_first: PASSED (0.10s)', 'expected error: from a passing test',
                     '[2/3] run_second: FAILED (0.20s)', 'assertion failed in second test',
                     '[3/3] run_third: PASSED (0.30s)', 'passing third test output',
                     'FAILED: run_tests', 'run_tests.py manifest.json', 'ninja: build stopped: subcommand failed.'):
            output.on_output(line)
        output.finish(False)
        text = stream.getvalue()
        self.assertIn('run_second: FAILED', text)
        self.assertIn('assertion failed in second test', text)
        self.assertNotIn('expected error', text)
        self.assertNotIn('passing third test output', text)

    # Configure/install failures show their CMake diagnostic without replaying earlier successful checks.
    def test_cmake_error_is_scoped_to_its_diagnostic(self):
        output, stream, _ = self.reporter()
        for line in ('-- Found Java: /tools/java', 'CMake Error at tool.cmake:7 (message):',
                     '  Missing required library', '', 'Call Stack (most recent call first):',
                     '  CMakeLists.txt:3 (include)', '-- Configuring incomplete, errors occurred!'):
            output.on_output(line)
        output.finish(False)
        self.assertIn('Missing required library', stream.getvalue())
        self.assertNotIn('Found Java', stream.getvalue())
        self.assertNotIn('Configuring incomplete', stream.getvalue())

    # An unrecognized tool failure gets a bounded tail instead of an empty error or its entire transcript.
    def test_unrecognized_failure_uses_bounded_tail(self):
        output, stream, _ = self.reporter()
        for index in range(250):
            output.on_output(f'tool output {index}')
        output.on_output('license server unavailable')
        output.finish(False)
        self.assertIn('license server unavailable', stream.getvalue())
        self.assertNotIn('tool output 0\n', stream.getvalue())
        self.assertLess(len(stream.getvalue().splitlines()), 110)

    # An incremental no-op reports that it is up to date without inventing a job count.
    def test_no_work_is_reported(self):
        output, stream, _ = self.reporter()
        output.on_output('ninja: no work to do.')
        output.finish(True)
        self.assertIn('up to date', stream.getvalue())
        self.assertNotIn('100%', stream.getvalue())

    # Legacy Windows or redirected encodings must not replace a real build failure with a Unicode exception.
    def test_non_unicode_stream_keeps_error_readable(self):
        log = io.StringIO()
        log.name = 'build.log'
        stream = AsciiStream()
        output = build_output.BuildOutput('Build', log, stream=stream)
        output.on_output('CMake Error: could not load 雪.lib')
        output.finish(False)
        self.assertIn('could not load \\u96ea.lib', stream.getvalue())
        self.assertIn('could not load 雪.lib', log.getvalue())

    # Saving complete build logs must not expose credential values in files, error text, or callbacks.
    def test_log_files_and_errors_are_redacted(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'build.log'
            stream = io.StringIO()
            with self.assertRaises(run.ExecException) as raised:
                build_output.command({**os.environ, 'BUILD_PASSWORD': 'test-private-value', 'BUILD_API_KEY': 'test-api-value'},
                                     [sys.executable, '-c', 'import sys; print("test-private-value test-api-value"); sys.exit(7)'],
                                     'Build', log, stream=stream)
            self.assertEqual(7, raised.exception.retcode)
            self.assertIn('***', log.read_text())
            for text in (log.read_text(), stream.getvalue(), raised.exception.output):
                self.assertNotIn('test-private-value', text)
                self.assertNotIn('test-api-value', text)

    # ANSI styling inside a credential must not reconstruct it when logs or failure diagnostics strip colors.
    def test_ansi_split_credentials_are_redacted(self):
        secret = 'test-private-value'
        diagnostic = 'CMake Error: test-private-\x1b[1mvalue\x1b[0m\n'
        script = f'import sys; sys.stdout.write({diagnostic!r}); sys.exit(7)'
        env = {key: value for key, value in os.environ.items()
               if key not in ('NO_COLOR', 'NOCOLOR', 'CLICOLOR', 'CLICOLOR_FORCE')}
        env['BUILD_API_KEY'] = secret
        for gradle in (False, True):
            for verbose in (False, True):
                with self.subTest(gradle=gradle, verbose=verbose), tempfile.TemporaryDirectory() as directory:
                    log = Path(directory) / 'build.log'
                    stream = Terminal()
                    with contextlib.redirect_stdout(stream), self.assertRaises(run.ExecException) as raised:
                        build_output.command(env, [sys.executable, '-c', script], 'Build', log,
                                             stream=stream, gradle=gradle, verbose=verbose)
                    for text in (log.read_text(), stream.getvalue(), raised.exception.output):
                        self.assertNotIn(secret, build_output._ANSI.sub('', text))
                        self.assertIn('CMake Error: ***', text)
                    self.assertNotIn('\x1b', log.read_text())

    # Streaming a large build to a file must not retain another copy in the generic command runner.
    def test_streaming_without_capture_preserves_callbacks_and_exit_status(self):
        lines = []
        with self.assertRaises(run.ExecException) as raised:
            run.env_command(os.environ, [sys.executable, '-c',
                            'import sys; [print(i) for i in range(1000)]; sys.exit(3)'],
                            silent=True, capture_output=False, on_output=lines.append)
        self.assertEqual(3, raised.exception.retcode)
        self.assertEqual('', raised.exception.output)
        self.assertEqual(1000, len(lines))

    # build.py exposes the actual exit status after the focused diagnostic, without a Python traceback.
    def test_configuration_runner_preserves_failure_status(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()) as stream:
            configuration = build.Configuration.__new__(build.Configuration)
            configuration.verbose = False
            configuration.build_options = []
            configuration.defold_root = directory
            configuration._form_env = lambda: os.environ.copy()
            with self.assertRaises(SystemExit) as raised:
                configuration._run_cmake([sys.executable, '-c',
                                          'import sys; print("tool failed"); sys.exit(9)'],
                                         directory, 'fixture', 'build')
            self.assertEqual(9, raised.exception.code)
            self.assertIn('tool failed', stream.getvalue())
            self.assertNotIn('Traceback', stream.getvalue())

    # CI cache hits must also use compact output when reinstalling shared SDK headers.
    def test_cached_external_headers_use_compact_runner(self):
        with mock.patch.object(sys, 'path', sys.path + [str(Path(__file__).resolve().parents[1] / 'ci')]):
            import ext_cache
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.dict(os.environ, {'GITHUB_ACTIONS': 'true',
                                             'DEFOLD_EXT_HOST_CACHE': str(Path(directory) / 'cache'),
                                             'DEFOLD_EXT_HOST_CACHE_HIT': 'true'}), \
                mock.patch.object(ext_cache.shutil, 'copytree'):
            configuration = mock.Mock(host='arm64-macos', target_platform='arm64-macos',
                                      defold_root=directory, ext=str(Path(directory) / 'ext'))
            configuration._find_cmake_build_type.return_value = 'RelWithDebInfo'
            ext_cache.build_dependencies(configuration)
            builddir = str(Path(directory) / 'external/build/arm64-macos')
            configuration._run_cmake.assert_called_once_with(
                ['cmake', '--install', builddir, '--config', 'RelWithDebInfo',
                 '--component', 'defold_sdk_headers'], builddir, 'ext', 'headers')
            configuration._build_ext_platform.assert_not_called()


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class CmakeBuildOutputTests(unittest.TestCase):
    def setUp(self):
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='cmake output '))
        self.root = Path(directory)
        self.builddir = self.root / 'build'
        (self.root / 'task.py').write_text('''
from pathlib import Path
import sys
if sys.argv[2] in ('fail', 'color-fail'):
    print('stdout from failing tool', flush=True)
    error = 'fixture.cpp:7: error: missing semicolon'
    if sys.argv[2] == 'color-fail':
        error = '\\x1b[31m' + error + '\\x1b[0m'
    print(error, file=sys.stderr, flush=True)
    sys.exit(7)
print('unrelated successful tool output')
Path(sys.argv[1]).write_text('built')
''')
        (self.root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(compact_build_test LANGUAGES NONE)
if(COLOR_CONFIG_ERROR)
  message(FATAL_ERROR "configuration color fixture")
endif()
set(outputs)
foreach(index RANGE 1 24)
  set(output "${CMAKE_BINARY_DIR}/step${index}.stamp")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/task.py" "${output}" pass VERBATIM)
  list(APPEND outputs "${output}")
endforeach()
add_custom_target(build_tests DEPENDS ${outputs})
add_custom_target(success ALL DEPENDS ${outputs})
add_custom_target(failure
  COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/task.py" ignored fail VERBATIM)
add_dependencies(failure success)
add_custom_target(console_failure
  COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/task.py" ignored fail USES_TERMINAL VERBATIM)
add_dependencies(console_failure success)
add_custom_target(color_failure
  COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/task.py" ignored color-fail VERBATIM)
add_custom_target(run_tests COMMAND "${CMAKE_COMMAND}" -E echo "test suite completed")
install(FILES "${CMAKE_BINARY_DIR}/step1.stamp" DESTINATION share)
''')
        self.env = os.environ.copy()
        self.env.pop('NINJA_STATUS', None)
        self.cmake = shutil.which('cmake')
        self.configure = [self.cmake, '-S', str(self.root), '-B', str(self.builddir), '-GNinja',
                          f'-DPYTHON_EXECUTABLE={sys.executable}']

    def command(self, args, phase, verbose=False):
        stream = io.StringIO()
        log = self.builddir / 'logs' / f'{phase}.log'
        with contextlib.redirect_stdout(stream):
            build_output.command(self.env, args, phase, log, verbose=verbose, stream=stream)
        return stream.getvalue(), log.read_text()

    # Ninja must preserve compiler colors through the internal pipe only for terminals, including verbose builds.
    def test_compiler_colors_follow_terminal_and_color_opt_outs(self):
        self.command(self.configure, 'configure')
        cases = ((True, {}, True), (False, {}, False), (False, {'CLICOLOR_FORCE': '1'}, False),
                 (True, {'NO_COLOR': ''}, False), (True, {'NO_COLOR': '1'}, False),
                 (True, {'NOCOLOR': '1'}, False), (True, {'CLICOLOR': '0'}, False))
        for verbose in (False, True):
            for terminal, overrides, expect_color in cases:
                with self.subTest(verbose=verbose, terminal=terminal, overrides=overrides):
                    env = self.env.copy()
                    for key in ('NO_COLOR', 'NOCOLOR', 'CLICOLOR', 'CLICOLOR_FORCE'):
                        env.pop(key, None)
                    env.update(overrides)
                    original_env = env.copy()
                    stream = Terminal() if terminal else io.StringIO()
                    path = self.builddir / 'logs' / 'colors.log'
                    args = [self.cmake, '--build', str(self.builddir), '--target', 'color_failure']
                    if verbose:
                        args.append('--verbose')
                    with contextlib.redirect_stdout(stream), self.assertRaises(run.ExecException) as raised:
                        build_output.command(env, args, 'Compiler colors', path, verbose=verbose, stream=stream)
                    text = stream.getvalue()
                    self.assertEqual(expect_color, '\x1b[31mfixture.cpp:7: error:' in text)
                    self.assertIn('fixture.cpp:7: error: missing semicolon', text)
                    if not expect_color:
                        self.assertNotIn('\x1b[', text)
                    self.assertNotIn('\x1b[', path.read_text())
                    self.assertEqual(original_env, env)
                    self.assertEqual(1, raised.exception.retcode)

    # CMake's own configure errors must also retain their native colors through output capture on a terminal.
    def test_cmake_configuration_errors_keep_terminal_colors(self):
        env = self.env.copy()
        for key in ('NO_COLOR', 'NOCOLOR', 'CLICOLOR', 'CLICOLOR_FORCE'):
            env.pop(key, None)
        for terminal in (False, True):
            with self.subTest(terminal=terminal):
                stream = Terminal() if terminal else io.StringIO()
                path = self.builddir / 'logs' / 'configure-colors.log'
                with self.assertRaises(run.ExecException):
                    build_output.command(env, self.configure + ['-DCOLOR_CONFIG_ERROR=ON'],
                                         'Configure colors', path, stream=stream)
                self.assertEqual(terminal, '\x1b[' in stream.getvalue())
                self.assertIn('configuration color fixture', stream.getvalue())
                self.assertNotIn('\x1b[', path.read_text())

    # Real Ninja failures, including console-pool jobs, retain compiler stderr and omit successful build noise.
    def test_real_ninja_failures_keep_only_failed_command_output(self):
        self.command(self.configure, 'configure')
        for target in ('failure', 'console_failure'):
            with self.subTest(target=target):
                stream = io.StringIO()
                log = self.builddir / 'logs' / f'{target}.log'
                with self.assertRaises(run.ExecException) as raised:
                    build_output.command(self.env, [self.cmake, '--build', str(self.builddir), '--target', target],
                                         target, log, stream=stream)
                self.assertEqual(1, raised.exception.retcode)
                text = stream.getvalue()
                self.assertIn('fixture.cpp:7: error: missing semicolon', text)
                self.assertIn('stdout from failing tool', text)
                self.assertNotIn('unrelated successful tool output', text)
                self.assertLess(text.index('stdout from failing tool'), text.index('fixture.cpp:7: error:'))
                self.assertIn('Full log:', text)

    # Successful and incremental real builds stay compact, while --verbose restores commands and tool output.
    def test_success_incremental_and_verbose_builds(self):
        text, log = self.command(self.configure, 'configure')
        self.assertNotIn('Configuring done', text)
        self.assertIn('Configuring done', log)
        args = [self.cmake, '--build', str(self.builddir)]
        text, log = self.command(args, 'build')
        self.assertNotIn('unrelated successful tool output', text)
        self.assertEqual(24, log.count('unrelated successful tool output'))
        self.assertLessEqual(len(text.splitlines()), 13)
        text, _ = self.command(args, 'incremental')
        self.assertIn('up to date', text)
        self.command([self.cmake, '--build', str(self.builddir), '--target', 'clean'], 'clean')
        text, log = self.command(args + ['--verbose'], 'verbose', verbose=True)
        self.assertIn('task.py', text)
        self.assertIn('unrelated successful tool output', text)
        self.assertIn('unrelated successful tool output', log)

    # The engine build path routes configure, build, install, and tests through the compact runner.
    def test_engine_cmake_phases_use_compact_output(self):
        configuration = build.Configuration(dynamo_home=str(self.root / 'sdk'),
                                            target_platform=build.sdk.get_host_platform(), no_colors=True)
        configuration.defold_root = str(self.root)
        configuration._platform_build_home = lambda platform: str(self.root)
        configuration._cmake_top_build_dir = lambda platform: str(self.builddir)
        configuration._form_env = lambda: self.env.copy()
        configuration._can_run_tests = lambda: True
        configuration._log = lambda message: None
        configuration.build_tracker = mock.Mock()
        # The fixture does not use the engine's feature flags; pass its interpreter through the environment cache.
        configuration._cmake_feature_defines = lambda: [f'-DPYTHON_EXECUTABLE={sys.executable}']
        with contextlib.redirect_stdout(io.StringIO()) as stream:
            configuration._build_engine_libs_cmake('engine_libs', 'all', configuration.target_platform)
        self.assertNotIn('unrelated successful tool output', stream.getvalue())
        self.assertNotIn('test suite completed', stream.getvalue())
        for phase in ('configure', 'build', 'install', 'tests'):
            self.assertTrue((self.builddir / 'logs' / f'engine_libs-{phase}.log').is_file(), phase)
        self.assertTrue((self.root / 'sdk/share/step1.stamp').is_file())


if __name__ == '__main__':
    unittest.main()
