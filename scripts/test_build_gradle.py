import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from threading import Event, Thread
import unittest
from unittest import mock

import build
import build_output
import run


class Terminal(io.StringIO):
    def isatty(self):
        return True


class GradleOutputTests(unittest.TestCase):
    def reporter(self, terminal=False):
        log = io.StringIO()
        log.name = 'gradle.log'
        stream = Terminal() if terminal else io.StringIO()
        self.now = 0
        output = build_output.GradleOutput('Gradle bob', log, stream=stream, clock=lambda: self.now)
        return output, stream, log

    # Native percentages produce sparse CI snapshots across phases without task spam or terminal escapes.
    def test_ci_progress_uses_gradle_percentages(self):
        output, stream, log = self.reporter()
        for phase in ('CONFIGURING', 'EXECUTING'):
            for percent in range(101):
                output.on_output(f'\x1b[1m│████····│ {percent}% {phase} [1s]\x1b[m\x1b[40D')
                output.on_output('> IDLE')
                output.on_output('successful tool output')
        output.on_output('│········│ 0% WAITING')
        output.finish(True)
        text = stream.getvalue()
        self.assertLessEqual(len(text.splitlines()), 26)
        self.assertIn('100% configuring', text)
        self.assertIn('100% executing', text)
        self.assertNotIn('waiting', text)
        self.assertNotIn('successful tool output', text)
        self.assertNotIn('\x1b', text)
        self.assertNotIn('\r', text)
        self.assertIn('successful tool output', log.getvalue())

    # Terminals redraw a bounded line for both older ASCII bars and current Unicode Gradle bars.
    def test_terminal_progress_reuses_one_line(self):
        output, stream, _ = self.reporter(terminal=True)
        with mock.patch.object(build_output.shutil, 'get_terminal_size', return_value=os.terminal_size((65, 24))):
            for percent in range(101):
                self.now += 0.02
                output.on_output(f'<====---------> {percent}% EXECUTING [1s]')
            output.finish(True)
        text = stream.getvalue()
        self.assertEqual(1, text.count('\n'))
        self.assertIn('100% executing', text)
        self.assertLess(text.count('\r'), 30)
        visible = build_output._ANSI.sub('', text)
        self.assertLessEqual(max(len(frame.rstrip('\n')) for frame in visible.split('\r')), 64)

    # Failure summaries may arrive before stdout task output; only failed task diagnostics belong in the console.
    def test_interleaved_failure_summary_and_task_output(self):
        output, stream, log = self.reporter()
        for line in ('FAILURE: Build failed with an exception.', '* Where:', "Build file 'build.gradle' line: 10",
                     '* What went wrong:', "Execution failed for task ':broken'.", '> actual failure',
                     '* Try:', '> Run with --scan', '> Get more help at https://help.gradle.org.'):
            output.on_stderr(line)
        for line in ('> Task :successful', 'unrelated successful output', '> Task :broken FAILED',
                     'failing task context', '> relevant tool diagnostic', '1 actionable task: 1 executed',
                     '> IDLE', '> Task :other', 'unrelated parallel output'):
            output.on_output(line)
        output.finish(False)
        text = stream.getvalue()
        self.assertIn('> Task :broken FAILED', text)
        self.assertIn('failing task context', text)
        self.assertIn('> relevant tool diagnostic', text)
        self.assertIn("Build file 'build.gradle' line: 10", text)
        self.assertIn('> actual failure', text)
        for omitted in ('unrelated', '* Try:', '--scan', '> IDLE', 'actionable task'):
            self.assertNotIn(omitted, text)
        self.assertIn('unrelated successful output', log.getvalue())
        self.assertIn('--scan', log.getvalue())

    # Compiler errors printed before a FAILED task record must retain source locations and error details.
    def test_compiler_diagnostics_before_failed_task_record(self):
        for header_first in (True, False):
            with self.subTest(header_first=header_first):
                output, stream, _ = self.reporter()
                output.on_output('Daemon will be stopped at the end of the build')
                if header_first:
                    output.on_output('> Task :compileJava')
                callback = output.on_output if header_first else output.on_stderr
                for line in ('Foo.java:3: error: cannot find symbol', '    missingSymbol();', '    ^'):
                    callback(line)
                output.on_output('> Task :compileJava FAILED')
                output.finish(False)
                self.assertIn('Foo.java:3: error: cannot find symbol', stream.getvalue())
                self.assertIn('    missingSymbol();', stream.getvalue())
                self.assertIn('> Task :compileJava FAILED', stream.getvalue())
                self.assertNotIn('Daemon', stream.getvalue())

    # A FAILED task header must retain that task's earlier tool output without replaying successful tasks.
    def test_task_diagnostics_before_failed_task_record(self):
        for stderr in (False, True):
            with self.subTest(stderr=stderr):
                output, stream, _ = self.reporter()
                output.on_output('> Task :successful')
                output.on_output('unrelated successful output')
                output.on_output('> Task :broken')
                callback = output.on_stderr if stderr else output.on_output
                callback('Missing required native library: libshaderc_shared.dylib')
                output.on_output('> Task :broken FAILED')
                for line in ('FAILURE: Build failed with an exception.', '* What went wrong:',
                             "Execution failed for task ':broken'.", '> Process exited with code 1'):
                    output.on_stderr(line)
                for line in ('[Incubating] Problems report is available at: build/reports/problems.html',
                             'Deprecated Gradle features were used in this build, making it incompatible with Gradle 10.',
                             "You can use '--warning-mode all' to show the individual deprecation warnings."):
                    output.on_output(line)
                output.on_output('> Task :other')
                output.on_output('unrelated parallel output')
                output.finish(False)
                text = stream.getvalue()
                self.assertIn('Missing required native library: libshaderc_shared.dylib', text)
                self.assertIn('> Task :broken FAILED', text)
                self.assertIn('> Process exited with code 1', text)
                self.assertNotIn('unrelated', text)
                self.assertNotIn('> Task :successful', text)
                self.assertNotIn('Problems report', text)
                self.assertNotIn('Deprecated Gradle', text)
                self.assertNotIn('--warning-mode', text)

    # Separate pipe readers can deliver stderr before Gradle attributes output to its first task.
    def test_stderr_diagnostics_before_first_task_header(self):
        for task_started in (False, True):
            for summary_first in (False, True):
                with self.subTest(task_started=task_started, summary_first=summary_first):
                    output, stream, log = self.reporter()
                    output.on_output('Daemon will be stopped at the end of the build')
                    for line in ('REVIEW_ROOT_CAUSE: missing native library', '    searched /sdk/lib'):
                        output.on_stderr(line)
                    if task_started:
                        output.on_output('> Task :broken')
                    summary = ('FAILURE: Build failed with an exception.', '* What went wrong:',
                               "Execution failed for task ':broken'.", '> subprocess exited with code 1')
                    if summary_first:
                        for line in summary:
                            output.on_stderr(line)
                    output.on_output('> Task :broken FAILED')
                    if not summary_first:
                        for line in summary:
                            output.on_stderr(line)
                    output.on_output('> Task :other')
                    output.on_output('unrelated successful output')
                    output.finish(False)
                    text = stream.getvalue()
                    for diagnostic in ('REVIEW_ROOT_CAUSE: missing native library', '    searched /sdk/lib'):
                        self.assertEqual(1, text.count(diagnostic))
                        self.assertIn(diagnostic, log.getvalue())
                    self.assertIn('> Task :broken FAILED', text)
                    self.assertIn('> subprocess exited with code 1', text)
                    self.assertNotIn('Daemon', text)
                    self.assertNotIn('unrelated successful output', text)

    # A later task's stderr may precede its stdout header even with sequential Gradle execution.
    def test_stderr_diagnostics_survive_task_boundaries(self):
        for task_started in (False, True):
            for summary_first in (False, True):
                with self.subTest(task_started=task_started, summary_first=summary_first):
                    output, stream, log = self.reporter()
                    output.on_output('> Task :preceding')
                    output.on_output('unrelated successful output')
                    output.on_stderr('REVIEW_ROOT_CAUSE: missing native library')
                    if task_started:
                        output.on_output('> Task :broken')
                    output.on_stderr('    searched /sdk/lib')
                    summary = ('FAILURE: Build failed with an exception.', '* What went wrong:',
                               "Execution failed for task ':broken'.", '> subprocess exited with code 1')
                    if summary_first:
                        for line in summary:
                            output.on_stderr(line)
                    output.on_output('> Task :broken FAILED')
                    if not summary_first:
                        for line in summary:
                            output.on_stderr(line)
                    output.on_output('> Task :other')
                    output.on_output('unrelated parallel output')
                    output.finish(False)
                    text = stream.getvalue()
                    for diagnostic in ('REVIEW_ROOT_CAUSE: missing native library', '    searched /sdk/lib'):
                        self.assertEqual(1, text.count(diagnostic))
                        self.assertIn(diagnostic, log.getvalue())
                    self.assertLess(text.index('REVIEW_ROOT_CAUSE:'), text.index('    searched /sdk/lib'))
                    self.assertIn('> Task :broken FAILED', text)
                    self.assertIn('> subprocess exited with code 1', text)
                    self.assertNotIn('unrelated', text)
                    self.assertNotIn('> Task :preceding', text)

    # A stdout task header between stderr lines must not discard or reorder a compiler diagnostic.
    def test_stderr_compiler_diagnostic_crosses_task_header(self):
        output, stream, _ = self.reporter()
        output.on_output('> Task :preceding')
        output.on_stderr('Foo.java:3: error: cannot find symbol')
        output.on_output('> Task :compileJava')
        output.on_stderr('    missingSymbol();')
        output.on_stderr('    ^')
        output.on_output('> Task :compileJava FAILED')
        output.finish(False)
        text = stream.getvalue()
        self.assertIn('Foo.java:3: error: cannot find symbol', text)
        self.assertIn('    missingSymbol();', text)
        self.assertIn('    ^', text)
        self.assertLess(text.index('Foo.java:3: error:'), text.index('    missingSymbol();'))
        self.assertLess(text.index('    missingSymbol();'), text.index('    ^'))

    # Bounded unattributed stderr must not truncate the first recognized compiler error in a long diagnostic.
    def test_long_stderr_compiler_diagnostic_keeps_first_error(self):
        for failed_header in (False, True):
            with self.subTest(failed_header=failed_header):
                output, stream, _ = self.reporter()
                output.on_output('> Task :preceding')
                output.on_stderr('Foo.java:3: error: cannot find symbol')
                for index in range(150):
                    output.on_stderr(f'    compiler context {index}')
                if failed_header:
                    output.on_output('> Task :compileJava FAILED')
                output.finish(False)
                self.assertIn('Foo.java:3: error: cannot find symbol', stream.getvalue())
                self.assertIn('    compiler context 149', stream.getvalue())

    # Unattributed stderr remains available in the log without adding noise to successful compact builds.
    def test_stderr_before_first_task_stays_quiet_on_success(self):
        output, stream, log = self.reporter()
        output.on_stderr('early tool diagnostic')
        output.on_output('> Task :successful')
        output.on_stderr('successful tool warning')
        output.on_output('│████████│ 100% EXECUTING')
        output.finish(True)
        self.assertNotIn('early tool diagnostic', stream.getvalue())
        self.assertNotIn('successful tool warning', stream.getvalue())
        self.assertIn('early tool diagnostic', log.getvalue())
        self.assertIn('successful tool warning', log.getvalue())
        self.assertIn('100% executing', stream.getvalue())

    # Bob's passed-test and standard-output logging must not bury failed tests and their assertions.
    def test_failed_tests_exclude_successful_test_logs(self):
        output, stream, log = self.reporter()
        for line in ('> Task :testJar', 'ExampleTest > succeeds STANDARD_OUT', '    unrelated test output',
                     'ExampleTest > succeeds PASSED', 'ExampleTest > fails FAILED',
                     '    java.lang.AssertionError: expected value', '        at ExampleTest.java:12',
                     'ExampleTest > another PASSED', '> Task :testJar FAILED'):
            output.on_output(line)
        output.finish(False)
        text = stream.getvalue()
        self.assertIn('ExampleTest > fails FAILED', text)
        self.assertIn('java.lang.AssertionError: expected value', text)
        self.assertIn('at ExampleTest.java:12', text)
        self.assertNotIn('PASSED', text)
        self.assertNotIn('unrelated test output', text)
        self.assertIn('unrelated test output', log.getvalue())

    # --continue failures must retain all failure reasons while hiding generic Gradle help sections.
    def test_multiple_failure_explanations_are_retained(self):
        output, stream, _ = self.reporter()
        for line in ('FAILURE: Build completed with 2 failures.', '1: Task failed with an exception.',
                     '* What went wrong:', '> first actual failure', '* Try:', '> Run with --scan',
                     '2: Task failed with an exception.', '* What went wrong:', '> second actual failure'):
            output.on_stderr(line)
        output.finish(False)
        self.assertIn('> first actual failure', stream.getvalue())
        self.assertIn('> second actual failure', stream.getvalue())
        self.assertNotIn('--scan', stream.getvalue())

    # A launcher or daemon failure without standard Gradle headings still gets a bounded useful diagnostic.
    def test_unrecognized_failure_uses_bounded_tail(self):
        output, stream, _ = self.reporter()
        output.on_stderr('early JVM warning')
        for index in range(250):
            output.on_output(f'bootstrap output {index}')
        output.on_stderr('Could not reserve enough space for the JVM')
        output.on_output('Gradle launcher failed before task execution')
        output.finish(False)
        self.assertIn('Could not reserve enough space for the JVM', stream.getvalue())
        self.assertIn('Gradle launcher failed before task execution', stream.getvalue())
        self.assertNotIn('early JVM warning', stream.getvalue())
        self.assertNotIn('bootstrap output 0\n', stream.getvalue())
        self.assertLess(len(stream.getvalue().splitlines()), 110)

    # Rich progress must reach callbacks before any newline or process exit, with separate intact stderr records.
    def test_cursor_redraws_stream_before_process_exit(self):
        read_fd, write_fd = os.pipe()
        self.addCleanup(os.close, read_fd)
        self.addCleanup(os.close, write_fd)
        progress = Event()
        lines, errors, failures = [], [], []
        script = ('import sys; '
                  'sys.stdout.buffer.write("│····│ 5% EXECUTING\\x1b[20D\\x1b[1B".encode()); sys.stdout.flush(); '
                  'sys.stdin.readline(); '
                  'sys.stderr.write("intact stderr diagnostic\\n"); sys.stderr.flush()')

        def on_output(line):
            lines.append(line)
            if '5% EXECUTING' in line:
                progress.set()

        def execute():
            try:
                run.env_command(os.environ, [sys.executable, '-c', script], stdin=read_fd,
                                silent=True, terminal_output=True, on_output=on_output, on_stderr=errors.append)
            except BaseException as error:
                failures.append(error)

        worker = Thread(target=execute, daemon=True)
        worker.start()
        try:
            received = progress.wait(timeout=10)
        finally:
            os.write(write_fd, b'continue\n')
            worker.join(timeout=10)
        self.assertTrue(received, 'Progress was buffered until process exit')
        self.assertFalse(worker.is_alive())
        self.assertEqual([], failures)
        self.assertEqual(['intact stderr diagnostic'], errors)
        self.assertIn('│····│ 5% EXECUTING', lines[0])

    # Rich-console frame and UTF-8 boundaries may fall between pipe reads without corrupting output.
    def test_fragmented_cursor_and_unicode_records(self):
        data = '│··│ 5% EXECUTING\x1b[20D\x1b[1B雪 diagnostic\nlast record'.encode('utf-8')
        chunks = iter([data[index:index + 1] for index in range(len(data))] + [b''])
        stream = mock.Mock()
        stream.read1.side_effect = lambda size: next(chunks)
        lines = list(run._terminal_lines(stream))
        self.assertEqual(['│··│ 5% EXECUTING\x1b[20D\n', '雪 diagnostic\n', 'last record\n'], lines)

    # Both rich-console streams must redact credentials in saved logs, console errors, and raised diagnostics.
    def test_rich_console_redacts_both_streams(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'gradle.log'
            stream = io.StringIO()
            script = ('import sys; '
                      'print("stdout test-private-value"); '
                      'sys.stderr.write("FAILURE: Build failed with an exception.\\n* What went wrong:\\nstderr test-pri\\x1b[1mvate-value\\x1b[0m\\n"); '
                      'sys.exit(7)')
            with self.assertRaises(run.ExecException) as raised:
                build_output.command({**os.environ, 'BUILD_PASSWORD': 'test-private-value'},
                                     [sys.executable, '-c', script], 'Gradle fixture', path,
                                     stream=stream, gradle=True)
            self.assertEqual(7, raised.exception.retcode)
            for text in (path.read_text(), stream.getvalue(), raised.exception.output):
                self.assertNotIn('test-private-value', text)
            self.assertIn('stdout ***', path.read_text())
            self.assertIn('stderr ***', stream.getvalue())


class GradleConfigurationTests(unittest.TestCase):
    def setUp(self):
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='gradle build '))
        self.root = Path(directory)
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(self.root)
        self.configuration.host = self.configuration.target_platform = 'arm64-macos'
        self.configuration.verbose = False
        self.configuration.build_options = []
        self.configuration.channel = 'test-channel'
        self.configuration.keep_bob_uncompressed = True
        self.configuration.build_tracker = mock.Mock()
        self.configuration._form_env = lambda: os.environ.copy()

    # Every Bob build/test and private plugin entry point must use compact output without changing its task graph.
    def test_bob_commands_use_shared_runner(self):
        configuration = self.configuration
        bob = self.root / 'com.dynamo.cr/com.dynamo.cr.bob'
        (bob / 'dist').mkdir(parents=True)
        (bob / 'dist/bob.jar').touch()
        for plugin in ('xbox', 'switch', 'playstation'):
            (self.root / f'com.dynamo.cr/com.dynamo.cr.{plugin}').mkdir()
        with mock.patch.object(configuration, '_run_gradle') as run_gradle:
            configuration.build_bob_light()
            self.assertIn('installBobLight', run_gradle.call_args_list[0].args[0])
            self.assertIn('-Pchannel=test-channel', run_gradle.call_args_list[0].args[0])
            self.assertEqual(4, run_gradle.call_count)
            self.assertEqual({'bob-plugin-xbox', 'bob-plugin-switch', 'bob-plugin-playstation'},
                             {call.args[2] for call in run_gradle.call_args_list[1:]})
            for skip_tests in (True, False):
                run_gradle.reset_mock()
                configuration.skip_tests = skip_tests
                configuration.build_bob()
                args = run_gradle.call_args_list[0].args[0]
                self.assertIn('clean', args)
                self.assertIn('install', args)
                self.assertIn('-Pkeep-bob-uncompressed', args)
                self.assertEqual(not skip_tests, 'testJar' in args)
            run_gradle.reset_mock()
            configuration.test_bob()
            self.assertEqual(['testJar', '-x', 'distBob'], run_gradle.call_args.args[0][-3:])

    # Compact and verbose commands preserve argument quoting, useful logs, and the original failure exit status.
    def test_runner_flags_logs_and_exit_status(self):
        script = self.root / 'fake gradle.py'
        script.write_text('import json, sys\n'
                          'print(json.dumps(sys.argv[1:]))\n'
                          'print("FAILURE: Build failed with an exception.", file=sys.stderr)\n'
                          'print("* What went wrong:\\nactual tool error", file=sys.stderr)\n'
                          'sys.exit(9 if "fail" in sys.argv else 0)\n')
        configuration = self.configuration
        for verbose in (False, True):
            configuration.verbose = verbose
            with contextlib.redirect_stdout(io.StringIO()) as stream:
                configuration._run_gradle([sys.executable, str(script), '-Pvalue=with spaces'], str(self.root), 'fixture', os.environ)
            log = self.root / 'logs/fixture-arm64-macos.log'
            lines = log.read_text().splitlines()
            args = json.loads(next(line for line in lines if line.startswith('[')))
            self.assertIn('-Pvalue=with spaces', args)
            self.assertIn('--console=plain' if verbose else '--console=rich', args)
            self.assertEqual(verbose, '--info' in args)
            self.assertEqual(verbose, '-Pvalue=with spaces' in stream.getvalue())
        configuration.verbose = False
        with contextlib.redirect_stdout(io.StringIO()) as stream, self.assertRaises(SystemExit) as error:
            configuration._run_gradle([sys.executable, str(script), 'fail'], str(self.root), 'fixture', os.environ)
        self.assertEqual(9, error.exception.code)
        self.assertIn('actual tool error', stream.getvalue())
        self.assertNotIn('Traceback', stream.getvalue())

    # Gradle clean must not unlink the active log or fail on an open log file on Windows.
    def test_logs_survive_cleaning_build_outputs(self):
        build_directory = self.root / 'build'
        build_directory.mkdir()
        (build_directory / 'old-output').touch()
        script = self.root / 'clean.py'
        script.write_text('import shutil\nshutil.rmtree("build")\nprint("complete output after clean")\n')
        with contextlib.redirect_stdout(io.StringIO()):
            self.configuration._run_gradle([sys.executable, str(script)], str(self.root), 'clean', os.environ)
        self.assertFalse(build_directory.exists())
        log = self.root / 'logs/clean-arm64-macos.log'
        self.assertIn('complete output after clean', log.read_text())

    # Bob, Bob Light, test, and plugin timings must appear in the console and JSON build summaries exactly once.
    def test_gradle_commands_are_included_in_build_summary(self):
        configuration = self.configuration
        path = self.root / 'build_times.json'
        messages = []
        tracker = build.BuildTimeTracker(output_file=str(path), logger=messages.append)
        configuration.build_tracker = tracker
        names = ('bob', 'bob-light', 'bob-build-tests', 'bob-tests', 'bob-plugin-xbox')
        script = self.root / 'gradle fixture.py'
        script.write_text('print("<=============> 100% EXECUTING [1s]")\n')
        with mock.patch.object(build.time, 'time', side_effect=range(100, 100 + 2 * len(names) + 2)), \
                contextlib.redirect_stdout(io.StringIO()):
            tracker.start_command('build_bob')
            for name in names:
                configuration._run_gradle([sys.executable, str(script)], str(self.root), name, os.environ)
            tracker.end_command('build_bob')
            tracker.print_summary()
            tracker.save_times()
        summary = json.loads(path.read_text())
        commands = summary['commands']
        self.assertEqual({'build_bob'} | {f'Gradle {name}' for name in names}, set(commands))
        self.assertEqual(2 * len(names) + 1, summary['build_session']['total_duration'])
        output = '\n'.join(messages)
        for name in names:
            command = f'Gradle {name}'
            self.assertEqual(1, commands[command]['duration'])
            self.assertRegex(output, rf'(?m)^  {command}\s+1\.00 s$')

    # A failed Gradle subprocess must close its timer while preserving its original exit code.
    def test_gradle_failure_closes_command_timer(self):
        configuration = self.configuration
        configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
        with mock.patch.object(build.time, 'time', side_effect=[10, 10.25]), \
                mock.patch.object(build_output, 'command', side_effect=run.ExecException(9)), \
                self.assertRaises(SystemExit) as error:
            configuration._run_gradle(['gradlew', 'build'], str(self.root), 'failure', os.environ)
        self.assertEqual(9, error.exception.code)
        self.assertEqual(0.25, configuration.build_tracker.command_times['Gradle failure']['duration'])
        self.assertEqual(10.25, configuration.build_tracker.end_times['Gradle failure'])


if __name__ == '__main__':
    unittest.main()
