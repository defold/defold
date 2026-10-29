"""Compact build progress with complete logs and focused failure output."""

from collections import deque
import json
from pathlib import Path
import re
import shutil
import sys
import time

import run
import build_console
from build_console import Style


NINJA_STATUS = '[defold-build:%f/%t] '
_NINJA_PROGRESS = re.compile(r'^\[defold-build:(\d+)/(\d+)\] ')
_TEST_PROGRESS = re.compile(r'^\[(\d+)/(\d+)\] \S+: (PASSED|FAILED) \([\d.]+s\)$')
_TEST_START = re.compile(r'^Starting \S+ \(group .+\)$')
_ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
_ERROR = re.compile(r'^CMake Error|^Traceback \(most recent call last\):|\b(?:fatal error|error(?: [A-Z]+\d+)?):')
_GRADLE_PROGRESS = re.compile(r'^(?:<[^>]*>|[│|][^│|]*[│|])\s*(\d+)%\s+([A-Z]+)')
_GRADLE_TASK = re.compile(r'^> Task (\S+)(?:\s+(.*))?$')
_GRADLE_TEST = re.compile(r'^(\S.*?) (PASSED|SKIPPED|FAILED|STANDARD_OUT|STANDARD_ERROR)$')
_GRADLE_TEST_RUN_FAILURE = re.compile(r'^Gradle Test (?:Run|Executor) .+ FAILED$')
_GRADLE_SKIPPED = 'DEFOLD_TEST_SKIPPED '


class BuildOutput:
    def __init__(self, title, log_file, stream=None, verbose=False, clock=None, env=None):
        self.title = title
        self.log_file = log_file
        self.stream = stream if stream is not None else sys.stdout
        self.verbose = verbose
        self.clock = clock if clock is not None else time.monotonic
        self.terminal = self.stream.isatty()
        self.color_enabled = build_console.use_colors(self.stream, env)
        self.started = self.clock()
        self.last_update = self.started
        self.last_bucket = -1
        self.line_width = 0
        self.completed = 0
        self.total = 0
        self.progress_detail = None
        self.ninja = False
        self.tests = False
        self.no_work = False
        self.in_error = False
        self.errors = []
        self.edge_output = deque(maxlen=100)
        self.tail = deque(maxlen=100)
        if not self.verbose:
            self._write_progress(f'{self.title}: running...')

    def _write_progress(self, message, color=None):
        message = self._printable(message)
        paint = lambda text, code: build_console.colorize(text, code, self.color_enabled)
        if self.terminal:
            width = max(1, shutil.get_terminal_size(fallback=(100, 24)).columns - 1)
            line_width = min(width, len(build_console.STEP_PREFIX) + len(message))
            self.stream.write('\r' + build_console.format_step(message, color, paint, width)
                              + ' ' * max(0, min(self.line_width, width) - line_width))
            self.line_width = line_width
        else:
            self.stream.write(build_console.format_step(message, color, paint) + '\n')
        self.stream.flush()

    def _printable(self, message):
        encoding = self.stream.encoding or 'utf-8'
        return message.encode(encoding, errors='backslashreplace').decode(encoding)

    def _progress(self, completed, total, detail=None):
        if total != self.total or completed < self.completed:
            self.last_bucket = -1
        self.completed, self.total = completed, total
        self.progress_detail = detail
        if not total:
            return
        percent = min(100, completed * 100 // total)
        now = self.clock()
        interval = 0.1 if self.terminal else 30
        if self.last_bucket >= 0 and completed != total and now - self.last_update < interval:
            if self.terminal or percent // 10 <= self.last_bucket:
                return
        self.last_update = now
        self.last_bucket = percent // 10
        filled = percent * 20 // 100
        suffix = detail if detail else f'({completed}/{total})'
        self._write_progress(f'{self.title} [{"#" * filled}{"-" * (20 - filled)}] {percent:3d}% {suffix}')

    def _start_error(self, line, context=False):
        if not self.in_error:
            if self.errors:
                self.errors.append('')
            if context:
                self.errors.extend(self.edge_output)
            self.edge_output.clear()
        self.in_error = True
        self.errors.append(line)

    def on_output(self, line):
        self.log_file.write(_ANSI.sub('', line) + '\n')
        if self.verbose:
            self.tail.append(line)
            return
        plain = _ANSI.sub('', line)
        progress = _NINJA_PROGRESS.match(plain)
        test = _TEST_PROGRESS.match(plain)
        if progress or test:
            self.ninja |= progress is not None
            self.tests |= test is not None
            self.in_error = False
            self.edge_output.clear()
            if not self.errors:
                match = progress if progress else test
                self._progress(int(match[1]), int(match[2]))
            if test and test[3] == 'FAILED':
                self._start_error(line)
            return
        if _TEST_START.match(plain):
            self.in_error = False
            self.edge_output.clear()
            return
        self.tail.append(line)
        if plain.startswith('FAILED:'):
            self._start_error(line, context=not self.tests)
        elif plain.startswith(('ninja: error:', 'ninja: build stopped:')):
            self._start_error(line, context=not self.errors)
        elif not self.ninja and _ERROR.search(plain):
            self._start_error(line)
        elif self.in_error:
            if not self.ninja and plain.startswith('-- '):
                self.in_error = False
            else:
                self.errors.append(line)
        else:
            self.edge_output.append(line)
        self.no_work |= plain == 'ninja: no work to do.'

    def failure_output(self):
        diagnostic = '\n'.join(self.errors or self.tail).strip()
        return diagnostic if self.color_enabled else _ANSI.sub('', diagnostic)

    def finish(self, success):
        if self.verbose:
            return
        elapsed = self.clock() - self.started
        if success:
            if self.total and self.completed != self.total:
                self._progress(self.total, self.total, self.progress_detail)
            status = 'up to date' if self.no_work else 'done'
        else:
            status = 'FAILED'
        self._write_progress(f'{self.title}: {status} [{elapsed:.2f} s]', Style.SUCCESS if success else Style.ERROR)
        if self.terminal:
            self.stream.write('\n')
        if not success:
            diagnostic = self.failure_output()
            if diagnostic:
                self.stream.write(self._printable(diagnostic) + '\n')
            self.stream.write(self._printable(f'Full log: {self.log_file.name}\n'))
        self.stream.flush()


class _DiagnosticBuffer:
    """Bound unstructured context while retaining a recognized error's complete diagnostic."""

    def __init__(self):
        self.context = deque(maxlen=100)
        self.errors = []

    def append(self, line):
        if self.errors or _ERROR.search(line):
            if not self.errors:
                self.errors.extend(self.context)
                self.context.clear()
            self.errors.append(line)
        else:
            self.context.append(line)

    def lines(self):
        return self.errors or list(self.context)


class GradleOutput(BuildOutput):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.phase = None
        self.summary_stream = None
        self.summary_section = False
        self.summary = []
        self.current_task = None
        self.task_output = {}
        self.test_tasks = set()
        self.current_tests = {}
        self.test_output = {}
        self.skipped_test_output = {}
        self.ordinary_skipped_tests = set()
        self.completed_skips = set()
        self.stderr_output = _DiagnosticBuffer()

    def on_output(self, line):
        self._on_output(line, False)

    def on_stderr(self, line):
        self._on_output(line, True)

    def _on_output(self, line, is_stderr):
        plain = _ANSI.sub('', line).rstrip()
        if not plain.strip():
            return
        self.log_file.write(plain + '\n')
        if plain.startswith(_GRADLE_SKIPPED):
            result = json.loads(plain[len(_GRADLE_SKIPPED):])
            key = (result['task'], result['test'])
            self.test_tasks.add(result['task'])
            if key not in self.completed_skips:
                self.ordinary_skipped_tests.add(key)
            self.skipped_test_output.pop(key, None)
            return
        progress = _GRADLE_PROGRESS.match(plain)
        if progress:
            phase = progress[2].lower()
            if phase != 'waiting' and not self.errors and not self.summary:
                if phase != self.phase:
                    self.phase = phase
                    self.last_bucket = -1
                self._progress(int(progress[1]), 100, phase)
            return
        # Worker rows are transient status, rather than task diagnostics.
        if plain == '> IDLE' or plain.startswith('> :'):
            return
        if plain.startswith(('BUILD FAILED', 'BUILD SUCCESSFUL', '[Incubating] Problems report is available at:',
                             'Deprecated Gradle features were used in this build,')):
            self.in_error = False
            if not is_stderr:
                self.current_tests.pop(self.current_task, None)
            return
        self.tail.append(plain)
        task = _GRADLE_TASK.match(plain)
        if task:
            same_task = self.current_task == task[1]
            self.current_task = task[1]
            self.edge_output = self.task_output.setdefault(self.current_task, deque(maxlen=100))
            if self.phase != 'executing' and not self.errors and not self.summary:
                self.phase = 'executing'
                self.last_bucket = -1
                self._progress(0, 100, self.phase)
            if task[2] == 'FAILED':
                if not same_task:
                    self.in_error = False
                self._start_error(plain, context=True)
                self.task_output.pop(self.current_task)
                self.current_tests.pop(self.current_task, None)
            else:
                self.in_error = False
                self.edge_output.append(plain)
            return
        if _GRADLE_TEST_RUN_FAILURE.match(plain):
            self.in_error = False
            self._start_error(plain, context=True)
            for key in list(self.skipped_test_output):
                if key[0] == self.current_task:
                    self.errors.extend(self.skipped_test_output.pop(key).lines())
            return
        test = _GRADLE_TEST.match(plain)
        # Method-only displays omit the class separator; stream events still identify a test task.
        if test and (' > ' in test[1] or self.current_task in self.test_tasks
                     or test[2] in ('STANDARD_OUT', 'STANDARD_ERROR')):
            self.test_tasks.add(self.current_task)
            self.in_error = False
            key = (self.current_task, test[1])
            status = test[2]
            if status in ('PASSED', 'SKIPPED'):
                current = self.current_tests.get(self.current_task)
                if current is not None and current[0] == test[1]:
                    self.current_tests.pop(self.current_task)
                previous = self.test_output.pop(key, None)
                ordinary_skip = key in self.ordinary_skipped_tests
                self.ordinary_skipped_tests.discard(key)
                if status == 'SKIPPED':
                    self.completed_skips.add(key)
                if status == 'SKIPPED' and previous is not None and not ordinary_skip:
                    # Worker crashes also produce SKIPPED results; retain errors and bounded context for a run failure.
                    previous.append(plain)
                    self.skipped_test_output[key] = previous
            else:
                # A new execution cannot inherit a completed invocation's skip confirmation.
                self.completed_skips.discard(key)
                self.current_tests[self.current_task] = (test[1], status)
                if status == 'FAILED':
                    self._start_error(plain, context=True)
                    previous = self.test_output.pop(key, None)
                    if previous is not None:
                        self.errors.extend(previous.lines())
                else:
                    self.test_output.setdefault(key, _DiagnosticBuffer()).append(plain)
            return
        if plain.startswith('FAILURE:'):
            self.summary_stream = is_stderr
            self.summary_section = True
            self.summary.append(plain)
        elif self.summary_stream == is_stderr and plain.startswith('* '):
            self.summary_section = plain.startswith(('* Where:', '* What went wrong:', '* Exception is:'))
            if self.summary_section:
                self.summary.append(plain)
        elif self.summary_stream == is_stderr:
            if re.match(r'^\d+: .+failed with an exception\.$', plain):
                self.summary_section = True
            if self.summary_section and not plain.startswith(('BUILD FAILED', 'BUILD SUCCESSFUL')):
                self.summary.append(plain)
        elif is_stderr:
            # Task headers arrive on stdout and cannot delimit stderr diagnostics.
            self.stderr_output.append(plain)
        elif self.current_task in self.current_tests:
            name, status = self.current_tests[self.current_task]
            if status == 'FAILED':
                if not re.match(r'^\d+ actionable tasks?:', plain):
                    self.errors.append(plain)
            else:
                self.test_output[(self.current_task, name)].append(plain)
        elif _ERROR.search(plain):
            self._start_error(plain, context=self.current_task is not None)
        elif self.in_error:
            if not re.match(r'^\d+ actionable tasks?:', plain):
                self.errors.append(plain)
        else:
            self.edge_output.append(plain)

    def failure_output(self):
        pending_tests = [line for output in self.test_output.values() for line in output.lines()]
        diagnostics = pending_tests + self.stderr_output.errors + self.errors + self.summary
        context = list(self.stderr_output.context)
        if not diagnostics:
            diagnostics = [line for line in self.tail if line not in context]
        return '\n'.join(context + diagnostics).strip()


def command(env, args, title, log_path, cwd=None, verbose=False, stream=None, gradle=False):
    env = env.copy()
    if not verbose:
        env['NINJA_STATUS'] = NINJA_STATUS
    secrets = [value for key, value in env.items()
               if value and re.search(r'TOKEN|PASSWORD|SECRET|PRIVATE_KEY|SERVICE_KEY|API_KEY|WEBHOOK', key.upper())]
    log_path = Path(log_path)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open('w', encoding='utf-8') as log_file:
        reporter = GradleOutput if gradle and not verbose else BuildOutput
        output = reporter(title, log_file, stream=stream, verbose=verbose, env=env)
        if not gradle:
            # The compiler already emits colors, but Ninja strips them when its
            # output is piped to this runner unless CLICOLOR_FORCE is enabled.
            env['CLICOLOR_FORCE'] = '1' if output.color_enabled else '0'
            if not output.color_enabled:
                env['CLICOLOR'] = '0'
        options = {} if verbose else {'silent': True}
        if gradle:
            options['terminal_output'] = True
            if not verbose:
                options['on_stderr'] = output.on_stderr
        try:
            run.env_command(env, args, cwd=cwd, on_output=output.on_output,
                            capture_output=False, redacted_values=secrets, kill_process_tree=True, **options)
        except run.ExecException as error:
            output.finish(False)
            error.output = output.failure_output()
            raise
        except BaseException:
            if output.terminal and not verbose:
                output.stream.write('\n')
                output.stream.flush()
            raise
        else:
            output.finish(True)
