# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

"""Serialize tests that share resources; Ninja bounds the total worker count."""

import argparse
import errno
import os
import subprocess
import time
from contextlib import contextmanager
from pathlib import Path


@contextmanager
def resource_lock(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, 'a+b') as lock:
        if os.name == 'nt':
            import msvcrt
            if path.stat().st_size == 0:
                lock.write(b'\0')
                lock.flush()
            while True:
                lock.seek(0)
                try:
                    msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                    break
                except OSError as error:
                    if error.errno not in (errno.EACCES, errno.EAGAIN, errno.EDEADLK):
                        raise
                    time.sleep(0.05)
        else:
            import fcntl
            fcntl.flock(lock, fcntl.LOCK_EX)
        try:
            yield
        finally:
            if os.name == 'nt':
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock, fcntl.LOCK_UN)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', required=True, type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command[:1] == ['--']:
        command = command[1:]
    if not command:
        parser.error('a test command is required after --')
    with resource_lock(args.lock):
        return subprocess.call(command)


if __name__ == '__main__':
    raise SystemExit(main())
