from log import log
import codecs
import os
from queue import Empty, Full, Queue
import re
import signal
import subprocess
import sys
from threading import Event, Thread
import build_console

_ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')

class ExecException(Exception):
    def __init__(self, retcode, output):
        self.retcode = retcode
        self.output = output
    def __init__(self, retcode):
        self.retcode = retcode
        self.output = ''

def _to_str(x):
    if x is None:
        return ''
    elif isinstance(x, (bytes, bytearray)):
        x = str(x, encoding='utf-8')
    return x

def _to_output_str(x):
    if x is None:
        return ''
    elif isinstance(x, (bytes, bytearray)):
        x = str(x, encoding='utf-8', errors='replace')
    return x

def _to_str_set(values):
    if values is None:
        return set()
    if isinstance(values, (list, tuple, set)):
        iterable = values
    else:
        iterable = [values]
    out = set()
    for v in iterable:
        s = _to_str(v)
        if s:
            out.add(s)
    return out

def _sanitize_text(text, secrets):
    text = _to_str(text)
    for secret in secrets:
        if secret:
            text = text.replace(secret, '***')
    # Styling can split a secret into pieces which become contiguous in plain logs.
    # Drop styling on these sensitive lines; leave other native diagnostic colors intact.
    if secrets and '\x1b' in text:
        plain = _ANSI.sub('', text)
        for secret in secrets:
            if secret and secret in plain:
                text = plain = plain.replace(secret, '***')
    return text

def _redact_args(arg_list, redact_flags, secrets):
    redacted = []
    redact_next = False

    for arg in arg_list:
        value = _to_str(arg)
        if redact_next:
            if value:
                secrets.add(value)
            redacted.append('***')
            redact_next = False
            continue

        is_redacted = False
        for flag in redact_flags:
            prefix = flag + '='
            if value == flag:
                redacted.append(value)
                redact_next = True
                is_redacted = True
                break
            elif value.startswith(prefix):
                secret = value[len(prefix):]
                if secret:
                    secrets.add(secret)
                redacted.append(prefix + '***')
                is_redacted = True
                break

        if not is_redacted:
            redacted.append(value)

    return redacted, secrets

def _pipe_bytes_available(fd):
    if os.name == 'nt':
        import msvcrt
        import _winapi
        handle = msvcrt.get_osfhandle(fd)
        try:
            return _winapi.PeekNamedPipe(handle)[0]
        except OSError as error:
            if error.winerror == _winapi.ERROR_BROKEN_PIPE:
                return 0
            raise
    import array
    import fcntl
    import termios
    available = array.array('i', [0])
    fcntl.ioctl(fd, termios.FIONREAD, available, True)
    return available[0]

def _terminal_lines(stream, stopped=None, process_done=None):
    # Gradle redraws rows with cursor movements, often without a newline or carriage return.
    separator = re.compile(r'\r\n|[\r\n]|\x1b\[[0-9;]*[AB]')
    decoder = codecs.getincrementaldecoder('utf-8')(errors='replace')
    pending = ''
    fd = stream.fileno() if stopped is not None else None
    if fd is not None:
        # Python 3.12 supports non-blocking pipes on Windows too.
        os.set_blocking(fd, False)
    remaining = None
    while stopped is None or not stopped.is_set():
        if remaining is None and process_done is not None and process_done.is_set():
            # Snapshot after observing exit to retain final writes without following future worker output.
            remaining = _pipe_bytes_available(fd)
        if remaining == 0:
            break
        try:
            size = 65536 if remaining is None else min(65536, remaining)
            chunk = os.read(fd, size) if fd is not None else stream.read1(size)
        except BlockingIOError:
            if remaining is not None:
                break
            stopped.wait(0.05)
            continue
        if not chunk:
            break
        if remaining is not None:
            remaining -= len(chunk)
        pending += decoder.decode(chunk)
        records = separator.split(pending)
        pending = records.pop()
        for record in records:
            if record:
                yield record + '\n'
    pending += decoder.decode(b'', final=True)
    if pending:
        yield pending + '\n'

def _stop_process(process, kill_process_tree=False):
    if kill_process_tree and os.name == 'nt' and process.poll() is None:
        try:
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
        except (OSError, subprocess.TimeoutExpired):
            pass
    isolated = kill_process_tree and os.name != 'nt'
    if isolated:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    if process.poll() is None:
        if not isolated:
            try:
                process.terminate()
            except ProcessLookupError:
                pass
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    if isolated:
        # A worker can outlive the launcher and keep the output readers blocked.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass

def _separate_output_lines(process, kill_process_tree=False):
    # Keep each stream's records intact; merging rich-console fragments corrupts diagnostics.
    records = Queue(maxsize=128)
    stopped = Event()
    process_done = Event()

    def put(record):
        while not stopped.is_set():
            try:
                records.put(record, timeout=0.1)
                return True
            except Full:
                pass
        return False

    def read_stream(stream, is_stderr):
        try:
            with stream:
                for line in _terminal_lines(stream, stopped, process_done):
                    if not put((line, is_stderr)):
                        return
        except BaseException as error:
            put(error)
        finally:
            put(None)

    readers = [Thread(target=read_stream, args=(stream, is_stderr), daemon=True)
               for stream, is_stderr in ((process.stdout, False), (process.stderr, True))]
    finished = 0
    started = []
    try:
        for reader in readers:
            reader.start()
            started.append(reader)
        while finished < len(readers):
            if process.poll() is not None:
                process_done.set()
            try:
                record = records.get(timeout=0.1)
            except Empty:
                continue
            if record is None:
                finished += 1
            elif isinstance(record, BaseException):
                raise record
            else:
                yield record
    finally:
        stopped.set()
        if finished < len(readers):
            _stop_process(process, kill_process_tree)
        for reader in started:
            reader.join()

def _exec_command(arg_list, **kwargs):
    # Report streamed lines even when command logging is disabled.
    on_output = kwargs.pop('on_output', None)
    on_stderr = kwargs.pop('on_stderr', None)
    capture_output = kwargs.pop('capture_output', True)
    terminal_output = kwargs.pop('terminal_output', False)
    kill_process_tree = kwargs.pop('kill_process_tree', False)
    silent = False
    if 'silent' in kwargs:
        silent = True
        del kwargs['silent']

    redact_flags = kwargs.pop('redact_flags', ['--password', '--passphrase', '--token', '--secret', '--api-key', '--client-secret'])
    if isinstance(redact_flags, str):
        redact_flags = [redact_flags]
    secrets = _to_str_set(kwargs.pop('redacted_values', None))

    arg_str = arg_list
    if not isinstance(arg_str, str):
        arg_list = [_to_str(x) for x in arg_list]
        redacted_args, secrets = _redact_args(arg_list, redact_flags, secrets)
        redacted_args = [_sanitize_text(arg, secrets) for arg in redacted_args]
        arg_str = ' '.join(redacted_args)
    else:
        arg_str = _sanitize_text(arg_str, secrets)
    if not silent:
        display = arg_str
        if not isinstance(arg_list, str) and build_console.use_colors(env=kwargs.get('env')):
            paint = lambda text, code: build_console.colorize(text, code, enabled=True)
            display = build_console.format_shell_command(redacted_args, colorize_fn=paint)
        log('[exec] %s' % display)

    # Keep stdout and stderr in their original order. In particular, Ninja may
    # print the failed command on stdout while the compiler or linker writes the
    # useful diagnostic to stderr.
    stream_output = 'stdout' not in kwargs and 'stderr' not in kwargs
    if stream_output:
        kwargs['stdout'] = subprocess.PIPE
        kwargs['stderr'] = subprocess.PIPE if terminal_output else subprocess.STDOUT
    if kill_process_tree and os.name != 'nt':
        # Isolate build workers so cancellation cannot signal this Python process.
        kwargs['start_new_session'] = True

    process = subprocess.Popen(arg_list, **kwargs)
    output = ''
    lines = None
    try:
        if stream_output:
            lines = (_separate_output_lines(process, kill_process_tree) if terminal_output else
                     ((line, False) for line in iter(lambda: _to_output_str(process.stdout.readline()), '')))
            for line, is_stderr in lines:
                line = _to_output_str(line)
                if capture_output:
                    output += line
                callback = on_stderr if is_stderr and on_stderr else on_output
                sanitized = _sanitize_text(line, secrets).rstrip('\r\n')
                if callback:
                    callback(sanitized)
                if not silent:
                    log(sanitized)
        else:
            captured_stdout, captured_stderr = process.communicate()
            if capture_output:
                output = _to_output_str(captured_stdout) + _to_output_str(captured_stderr)
        retcode = process.wait()
    except BaseException:
        _stop_process(process, kill_process_tree)
        raise
    finally:
        if lines is not None:
            lines.close()
        for pipe in (process.stdin, process.stdout, process.stderr):
            if pipe is not None:
                pipe.close()

    output = _sanitize_text(output, secrets)
    if retcode != 0:
        e = ExecException(retcode)
        e.output = output
        if not silent:
            log('[exec] failed command: %s' % arg_str)
            log('[exec] command failed with exit code %s' % retcode)
        raise e

    output = _to_str(output)
    return output.strip()

def command(args, **kwargs):
    if kwargs.get("shell") is None:
        kwargs["shell"] = False
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, **kwargs)
    except ExecException as e:
        sys.exit(e.retcode)

def shell_command(args, **kwargs):
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, shell = True, **kwargs)
    except ExecException as e:
        sys.exit(e.retcode)


def env_command(env, args, **kwargs):
    return _exec_command(args, shell = False, env = env, **kwargs)

def env_shell_command(env, args, **kwargs):
    return _exec_command(args, shell = True, env = env, **kwargs)
