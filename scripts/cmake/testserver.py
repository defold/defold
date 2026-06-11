#!/usr/bin/env python3
"""
Run a test executable with the Defold HTTP test server running.

Args:
  1: path to test executable
  2: workdir (optional, empty string to skip)
  3: server IP (default: localhost)
  4: server port (default: 9001)
  5: path to config file to write (default: unittest.cfg in CWD)
  6+: Python import directories for test_script_server.py and optional plugins
"""

import sys
import os
import subprocess
import configparser
import argparse


def write_config(path: str, ip: str, port: int) -> None:
    cfg = configparser.RawConfigParser()
    cfg.add_section('server')
    cfg.set('server', 'ip', ip)
    cfg.set('server', 'socket', str(port))
    with open(path, 'w') as f:
        cfg.write(f)
        print('Wrote test config file:', path)


def run_tests(executables, cfgpath: str) -> int:
    for exe in executables:
        print('Running test with shared Defold test server:', exe, flush=True)
        rc = subprocess.call([exe, cfgpath])
        if rc != 0:
            return rc
    return 0


def run_with_server(executables, workdir, ip: str, port: int, cfgpath: str, server_dirs) -> int:
    for server_dir in reversed(server_dirs):
        sys.path.insert(0, os.path.normpath(server_dir))

    try:
        import test_script_server  # type: ignore
    except Exception as e:
        print('Failed to import test_script_server from', server_dirs, '\n', e)
        return 2

    if workdir:
        os.chdir(workdir)

    write_config(cfgpath, ip, port)

    server = test_script_server.Server(port=port, ip=ip)
    server.start()
    try:
        return run_tests(executables, cfgpath)
    finally:
        try:
            server.stop()
        except Exception:
            pass
        try:
            os.remove(cfgpath)
        except OSError:
            pass


def main_argparse() -> int:
    parser = argparse.ArgumentParser(description='Run test executables with the Defold HTTP test server running.')
    parser.add_argument('--workdir', default=None)
    parser.add_argument('--ip', default='localhost')
    parser.add_argument('--port', type=int, default=9001)
    parser.add_argument('--config', default='unittest.cfg')
    parser.add_argument('--server-dir', action='append', default=[])
    parser.add_argument('executables', nargs='+')
    args = parser.parse_args()
    return run_with_server(args.executables, args.workdir, args.ip, args.port, args.config, args.server_dir)


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1].startswith('--'):
        return main_argparse()

    if len(sys.argv) < 2:
        print('Usage: testserver.py <exe> [workdir] [ip] [port] [cfgpath] [server_dir]')
        return 2

    exe = sys.argv[1]
    workdir = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
    ip = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else 'localhost'
    port = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 9001
    cfgpath = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else 'unittest.cfg'
    server_dirs = [p for p in sys.argv[6:] if p]

    return run_with_server([exe], workdir, ip, port, cfgpath, server_dirs)


if __name__ == '__main__':
    sys.exit(main())
