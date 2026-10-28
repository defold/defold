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

        def broken_lines(stream):
            if stream is self.children[-1].stdout:
                raise OSError(errno.EIO, 'simulated pipe read failure')
            yield from read_lines(stream)

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


if __name__ == '__main__':
    unittest.main()
