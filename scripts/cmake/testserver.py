#!/usr/bin/env python3
"""
Run test executables with the Defold HTTP test server running.

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
import socket
import ipaddress


def write_config(path: str, ip: str, port: int) -> None:
    cfg = configparser.RawConfigParser()
    cfg.add_section('server')
    cfg.set('server', 'ip', ip)
    cfg.set('server', 'socket', str(port))
    with open(path, 'w') as f:
        cfg.write(f)
        print('Wrote test config file:', path)


def _local_ip_from_udp_route() -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.connect(('8.8.8.8', 80))
        return sock.getsockname()[0]
    finally:
        sock.close()


def _local_ip_from_hostname() -> str:
    hostname = socket.gethostname()
    try:
        return socket.gethostbyname(hostname)
    except socket.gaierror as e:
        print(e)
        print("Hostname was '%s', trying empty hostname instead" % hostname)
        return socket.gethostbyname("")


def _is_publishable_ip(value: str) -> bool:
    try:
        ip = ipaddress.ip_address(value)
    except ValueError:
        return False
    return ip.version == 4 and not ip.is_loopback and not ip.is_unspecified and not ip.is_link_local


def _is_explicit_publish_ip(value: str) -> bool:
    try:
        ip = ipaddress.ip_address(value)
    except ValueError:
        return False
    return ip.version == 4 and not ip.is_loopback and not ip.is_unspecified


def _default_route_interface() -> str:
    try:
        result = subprocess.run(
            ['/sbin/route', '-n', 'get', 'default'],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            check=False)
    except OSError:
        return ''

    for line in result.stdout.splitlines():
        if 'interface:' in line:
            return line.split(':', 1)[1].strip()
    return ''


def _local_ip_from_ipconfig() -> str:
    interfaces = [_default_route_interface(), 'en0', 'en1', 'en2', 'en3', 'en4', 'en5']
    seen = set()
    for interface in interfaces:
        if not interface or interface in seen:
            continue
        seen.add(interface)
        try:
            result = subprocess.run(
                ['/usr/sbin/ipconfig', 'getifaddr', interface],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
                check=False)
        except OSError:
            continue
        ip = result.stdout.strip()
        if _is_publishable_ip(ip):
            return ip
    return ''


def _local_ip_from_ifconfig() -> str:
    try:
        result = subprocess.run(
            ['/sbin/ifconfig'],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            check=False)
    except OSError:
        return ''

    blocks = []
    current = []
    for line in result.stdout.splitlines():
        if line and not line[0].isspace():
            if current:
                blocks.append(current)
            current = [line]
        elif current:
            current.append(line)
    if current:
        blocks.append(current)

    ignored_prefixes = ('lo', 'awdl', 'llw', 'utun', 'bridge', 'gif', 'stf', 'anpi')
    fallback = ''
    for block in blocks:
        name = block[0].split(':', 1)[0]
        if name.startswith(ignored_prefixes):
            continue
        is_active = any(line.strip() == 'status: active' for line in block)
        for line in block:
            fields = line.strip().split()
            if len(fields) >= 2 and fields[0] == 'inet' and _is_publishable_ip(fields[1]):
                if is_active:
                    return fields[1]
                fallback = fallback or fields[1]
    return fallback


def resolve_publish_ip(ip: str) -> str:
    if ip != 'auto':
        return ip

    env_ip = os.environ.get('DEFOLD_TESTSERVER_IP')
    if env_ip and _is_explicit_publish_ip(env_ip):
        return env_ip
    if env_ip:
        raise RuntimeError('DEFOLD_TESTSERVER_IP is not a usable device-reachable IPv4 address: %s' % env_ip)

    for resolver in (_local_ip_from_ipconfig, _local_ip_from_ifconfig, _local_ip_from_udp_route, _local_ip_from_hostname):
        try:
            resolved = resolver()
        except OSError as e:
            print('Could not resolve local test server IP:', e)
            continue
        if resolved and _is_publishable_ip(resolved):
            return resolved

    raise RuntimeError('Could not resolve a non-loopback host IP for iOS device tests; set DEFOLD_TESTSERVER_IP')


def make_runner_command(runner_args, exe: str, cfgpath: str):
    if not runner_args:
        return [exe, cfgpath]

    command = []
    has_exe = False
    has_config = False
    for arg in runner_args:
        if '{exe}' in arg:
            has_exe = True
        if '{config}' in arg:
            has_config = True
        command.append(arg.replace('{exe}', exe).replace('{config}', cfgpath))

    if not has_exe:
        command.append(exe)
    if not has_config:
        command.append(cfgpath)
    return command


def run_tests(executables, cfgpath: str, runner_args=None) -> int:
    runner_args = runner_args or []
    for exe in executables:
        print('Running test with shared Defold test server:', exe, flush=True)
        rc = subprocess.call(make_runner_command(runner_args, exe, cfgpath))
        if rc != 0:
            return rc
    return 0


def run_with_server(executables, workdir, ip: str, port: int, cfgpath: str, server_dirs, runner_args=None, bind_ip=None) -> int:
    for server_dir in reversed(server_dirs):
        sys.path.insert(0, os.path.normpath(server_dir))

    try:
        import test_script_server  # type: ignore
    except Exception as e:
        print('Failed to import test_script_server from', server_dirs, '\n', e)
        return 2

    if workdir:
        os.chdir(workdir)

    publish_ip = resolve_publish_ip(ip)
    bind_ip = bind_ip or publish_ip
    print('Starting Defold test server on %s:%d, publishing %s:%d' % (bind_ip, port, publish_ip, port))
    write_config(cfgpath, publish_ip, port)

    server = test_script_server.Server(port=port, ip=bind_ip)
    server.start()
    try:
        return run_tests(executables, cfgpath, runner_args=runner_args)
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
    parser.add_argument('--bind-ip', default=None)
    parser.add_argument('--port', type=int, default=9001)
    parser.add_argument('--config', default='unittest.cfg')
    parser.add_argument('--server-dir', action='append', default=[])
    parser.add_argument('--runner-arg', action='append', default=[],
        help='Argument for a custom per-test runner. Use {exe} and {config} placeholders.')
    parser.add_argument('executables', nargs='+')
    args = parser.parse_args()
    return run_with_server(args.executables, args.workdir, args.ip, args.port, args.config, args.server_dir, args.runner_arg, args.bind_ip)


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
