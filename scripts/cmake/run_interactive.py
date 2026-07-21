#!/usr/bin/env python3
"""
Run a command with streaming output.

When CMake execute_process() runs native tests, stdout/stderr are pipes. Many
C/C++ test binaries then switch from line-buffered to block-buffered output.
On POSIX hosts, run the child behind a pseudo terminal so test output stays
interactive in Xcode's build log.
"""

import errno
import os
import shlex
import subprocess
import sys

if os.name == "posix":
    import pty
    import termios


def should_use_color() -> bool:
    return not (os.environ.get("NOCOLOR") or os.environ.get("NO_COLOR"))


def colorize(text: str, color_code: str) -> str:
    if not should_use_color():
        return text
    return f"\033[{color_code}m{text}\033[0m"


def format_command(argv) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(argv)
    return shlex.join(argv)


def print_failed_command(argv, returncode: int) -> None:
    print(colorize(f"FAILED COMMAND (exit code {returncode}): {format_command(argv)}", "91"), file=sys.stderr)


def run_with_pty(argv) -> int:
    master_fd, slave_fd = pty.openpty()
    devnull_fd = os.open(os.devnull, os.O_RDONLY)

    try:
        attrs = termios.tcgetattr(slave_fd)
        attrs[1] &= ~termios.ONLCR
        termios.tcsetattr(slave_fd, termios.TCSANOW, attrs)
    except Exception:
        pass

    try:
        process = subprocess.Popen(
            argv,
            stdin=devnull_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            close_fds=True)
    except Exception:
        os.close(master_fd)
        os.close(slave_fd)
        os.close(devnull_fd)
        raise

    os.close(slave_fd)
    os.close(devnull_fd)

    try:
        while True:
            try:
                data = os.read(master_fd, 4096)
            except OSError as exc:
                if exc.errno == errno.EIO:
                    break
                raise

            if not data:
                break

            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
    finally:
        os.close(master_fd)

    return process.wait()


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: run_interactive.py <command> [args...]", file=sys.stderr)
        return 2

    argv = sys.argv[1:]
    if os.name != "posix":
        result = subprocess.call(argv)
    else:
        result = run_with_pty(argv)

    if result != 0:
        print_failed_command(argv, result)
    return result


if __name__ == "__main__":
    sys.exit(main())
