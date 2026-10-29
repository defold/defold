import errno
import io
import os
from pathlib import Path
from queue import Queue
import subprocess
import sys
import tempfile
from threading import Event, Thread
import unittest
from unittest import mock

import build
import build_output
import run


class RunCommandTests(unittest.TestCase):
    def setUp(self):
        self.children, self.readers = [], []
        popen = subprocess.Popen

        def spawn(*args, **kwargs):
            kwargs['stdin'] = subprocess.PIPE
            child = popen(*args, **kwargs)
            self.children.append(child)
            return child

        def start_reader(*args, **kwargs):
            reader = Thread(*args, **kwargs)
            self.readers.append(reader)
            return reader

        self.addCleanup(self.cleanup_processes)
        self.enterContext(mock.patch.object(run.subprocess, 'Popen', side_effect=spawn))
        self.enterContext(mock.patch.object(run, 'Thread', side_effect=start_reader))

    def cleanup_processes(self):
        for child in self.children:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
            for pipe in (child.stdin, child.stdout, child.stderr):
                if pipe is not None:
                    pipe.close()

    def assert_processes_closed(self):
        for child in self.children:
            self.assertIsNotNone(child.poll(), 'Child process was left running')
            for pipe in (child.stdin, child.stdout, child.stderr):
                if pipe is not None:
                    self.assertTrue(pipe.closed)
        self.assertTrue(all(not reader.is_alive() for reader in self.readers))

    # Log write failures must stop and reap the child and join rich-console readers, even when their queue is full.
    def test_log_failure_cleans_up_process_and_readers(self):
        for terminal_output in (False, True):
            with self.subTest(terminal_output=terminal_output), tempfile.TemporaryDirectory() as directory:
                full = Event()

                class OutputQueue(Queue):
                    def put(self, item, *args, **kwargs):
                        super().put(item, *args, **kwargs)
                        if self.full():
                            full.set()

                class FullLog(io.StringIO):
                    name = 'full.log'

                    def write(self, text):
                        if terminal_output and not full.wait(timeout=10):
                            raise AssertionError('Reader queue never filled')
                        raise OSError(errno.ENOSPC, 'simulated log disk full')

                script = ('import sys; print("ready", flush=True); '
                          'sys.stdout.write("task output\\n" * 512); sys.stdout.flush(); '
                          'sys.stdin.readline()')
                with mock.patch.object(run, 'Queue', OutputQueue), \
                        mock.patch.object(Path, 'open', return_value=FullLog()), \
                        self.assertRaises(OSError) as raised:
                    build_output.command(os.environ, [sys.executable, '-c', script], 'Build',
                                         Path(directory) / 'full.log', stream=io.StringIO(), gradle=terminal_output)
                self.assertEqual(errno.ENOSPC, raised.exception.errno)
                self.assert_processes_closed()

    # A failed pipe reader must propagate its error and stop the child instead of hanging on the other stream.
    def test_reader_failure_stops_process_and_other_reader(self):
        read_lines = run._terminal_lines

        def broken_lines(stream, *args):
            if stream is self.children[-1].stdout:
                raise OSError(errno.EIO, 'simulated pipe read failure')
            yield from read_lines(stream, *args)

        script = 'import sys; print("ready", flush=True); sys.stdin.readline()'
        with mock.patch.object(run, '_terminal_lines', side_effect=broken_lines), self.assertRaises(OSError) as raised:
            run.env_command(os.environ, [sys.executable, '-c', script], silent=True, terminal_output=True)
        self.assertEqual(errno.EIO, raised.exception.errno)
        self.assert_processes_closed()

    # Interrupts from output callbacks must retain their identity while closing every subprocess resource.
    def test_callback_interrupt_cleans_up_process(self):
        interrupt = KeyboardInterrupt()

        def on_output(line):
            raise interrupt

        for terminal_output in (False, True):
            with self.subTest(terminal_output=terminal_output), self.assertRaises(KeyboardInterrupt) as raised:
                run.env_command(os.environ, [sys.executable, '-c',
                                'import sys; print("ready", flush=True); sys.stdin.readline()'],
                                silent=True, terminal_output=terminal_output, on_output=on_output)
            self.assertIs(interrupt, raised.exception)
            self.assert_processes_closed()

    # Cancelling a build must also stop workers that inherit the launcher's pipes, so reader cleanup can finish.
    def test_log_failure_stops_workers_holding_output_pipes(self):
        done = Event()
        errors = []

        class FullLog(io.StringIO):
            name = 'full.log'

            def write(self, text):
                raise OSError(errno.ENOSPC, 'simulated log disk full')

        script = ('import subprocess, sys; '
                  'subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.readline()"]); '
                  'print("ready", flush=True); sys.stdin.readline()')

        def execute(path, terminal_output):
            try:
                build_output.command(os.environ, [sys.executable, '-c', script], 'Build', path,
                                     stream=io.StringIO(), gradle=terminal_output)
            except BaseException as error:
                errors.append(error)
            finally:
                done.set()

        for terminal_output in (False, True):
            with self.subTest(terminal_output=terminal_output), tempfile.TemporaryDirectory() as directory, \
                    mock.patch.object(Path, 'open', return_value=FullLog()):
                done.clear()
                errors.clear()
                worker = Thread(target=execute, args=(Path(directory) / 'full.log', terminal_output), daemon=True)
                worker.start()
                try:
                    completed = done.wait(timeout=10)
                finally:
                    for child in self.children:
                        if child.stdin is not None and not child.stdin.closed:
                            child.stdin.close()
                    worker.join(timeout=10)
                self.assertTrue(completed, 'Worker pipes blocked cleanup after the launcher stopped')
                self.assertFalse(worker.is_alive())
                self.assertEqual(1, len(errors))
                self.assertIsInstance(errors[0], OSError)
                self.assertEqual(errno.ENOSPC, errors[0].errno)
                self.assert_processes_closed()

    # Launcher exit must drain both streams and preserve its status without waiting for an inherited worker pipe.
    def test_launcher_exit_does_not_wait_for_inherited_pipes(self):
        for retcode in (0, 7):
            with self.subTest(retcode=retcode):
                done = Event()
                results, errors, output, stderr = [], [], [], []
                script = ('import subprocess, sys; '
                          'worker = subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.readline()"]); '
                          'sys.stdout.write("".join("stdout %d\\n" % i for i in range(1024)) + "final stdout 雪"); '
                          'sys.stdout.flush(); sys.stderr.write("final stderr 雪"); sys.stderr.flush(); '
                          f'sys.exit({retcode})')

                def execute():
                    try:
                        results.append(run.env_command(
                            {**os.environ, 'PYTHONIOENCODING': 'utf-8'}, [sys.executable, '-c', script],
                            silent=True, terminal_output=True, kill_process_tree=True,
                            on_output=output.append, on_stderr=stderr.append))
                    except BaseException as error:
                        errors.append(error)
                    finally:
                        done.set()

                worker = Thread(target=execute, daemon=True)
                worker.start()
                try:
                    completed = done.wait(timeout=10)
                finally:
                    for child in self.children:
                        if child.stdin is not None and not child.stdin.closed:
                            child.stdin.close()
                    worker.join(timeout=10)
                self.assertTrue(completed, 'Launcher exit was blocked on a worker holding the output pipes')
                self.assertFalse(worker.is_alive())
                self.assertEqual([*(f'stdout {i}' for i in range(1024)), 'final stdout 雪'], output)
                self.assertEqual(['final stderr 雪'], stderr)
                if retcode:
                    self.assertEqual(1, len(errors))
                    self.assertIsInstance(errors[0], run.ExecException)
                    self.assertEqual(retcode, errors[0].retcode)
                    captured = errors[0].output
                else:
                    self.assertEqual([], errors)
                    self.assertEqual(1, len(results))
                    captured = results[0]
                self.assertIn('final stdout 雪', captured)
                self.assertIn('final stderr 雪', captured)
                self.assert_processes_closed()

    # Output written after an empty read but before exit is observed must reach the console and full log.
    def test_final_output_after_empty_read_is_drained(self):
        read = os.read
        for stream_name in ('stdout', 'stderr'):
            with self.subTest(stream=stream_name), tempfile.TemporaryDirectory() as directory:
                empty_read, resume = Event(), Event()
                stopped, process_done = Event(), Event()
                errors = []
                stream = io.StringIO()
                path = Path(directory) / 'gradle.log'
                diagnostic = 'FINAL_ROOT_CAUSE: final diagnostic 雪'
                script = ('import sys; sys.stdin.readline(); '
                          f'sys.{stream_name}.write({diagnostic!r}); sys.{stream_name}.flush(); '
                          'sys.exit(7)')

                def pause_after_empty_read(fd, size):
                    try:
                        return read(fd, size)
                    except BlockingIOError:
                        if not empty_read.is_set() and fd == getattr(self.children[-1], stream_name).fileno():
                            empty_read.set()
                            if not resume.wait(timeout=10):
                                raise AssertionError('Empty pipe read was never resumed')
                        raise

                def execute():
                    try:
                        build_output.command({**os.environ, 'PYTHONIOENCODING': 'utf-8'},
                                             [sys.executable, '-c', script], 'Gradle final output', path,
                                             stream=stream, gradle=True)
                    except BaseException as error:
                        errors.append(error)

                with mock.patch.object(run.os, 'read', side_effect=pause_after_empty_read), \
                        mock.patch.object(run, 'Event', side_effect=[stopped, process_done]):
                    worker = Thread(target=execute, daemon=True)
                    worker.start()
                    try:
                        self.assertTrue(empty_read.wait(timeout=10), 'Reader never reached an empty pipe')
                        child = self.children[-1]
                        child.stdin.write(b'write final diagnostic\n')
                        child.stdin.flush()
                        self.assertEqual(7, child.wait(timeout=10))
                        self.assertTrue(process_done.wait(timeout=10), 'Launcher exit was never observed')
                    finally:
                        resume.set()
                        for child in self.children:
                            if child.poll() is None:
                                child.kill()
                        worker.join(timeout=10)
                self.assertFalse(worker.is_alive())
                self.assertEqual(1, len(errors))
                self.assertIsInstance(errors[0], run.ExecException)
                self.assertEqual(7, errors[0].retcode)
                for text in (stream.getvalue(), path.read_text(), errors[0].output):
                    self.assertIn(diagnostic, text)
                self.assert_processes_closed()

    # Callback failure after launcher exit must cancel pipe readers even when process cleanup cannot stop the worker.
    def test_callback_failure_after_launcher_exit_cancels_readers(self):
        done = Event()
        errors = []
        script = ('import subprocess, sys; '
                  'worker = subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.readline()"]); '
                  'print("launcher output", flush=True)')

        def on_output(line):
            self.children[-1].wait(timeout=10)
            raise OSError(errno.ENOSPC, 'simulated log disk full')

        def execute():
            try:
                run.env_command(os.environ, [sys.executable, '-c', script], silent=True,
                                terminal_output=True, kill_process_tree=True, on_output=on_output)
            except BaseException as error:
                errors.append(error)
            finally:
                done.set()

        with mock.patch.object(run, '_stop_process', side_effect=lambda child, kill_process_tree: child.wait(timeout=10)):
            worker = Thread(target=execute, daemon=True)
            worker.start()
            try:
                completed = done.wait(timeout=10)
            finally:
                for child in self.children:
                    if child.stdin is not None and not child.stdin.closed:
                        child.stdin.close()
                worker.join(timeout=10)
        self.assertTrue(completed, 'Reader cancellation waited for a worker after its launcher exited')
        self.assertFalse(worker.is_alive())
        self.assertEqual(1, len(errors))
        self.assertIsInstance(errors[0], OSError)
        self.assertEqual(errno.ENOSPC, errors[0].errno)
        self.assert_processes_closed()


if __name__ == '__main__':
    unittest.main()
