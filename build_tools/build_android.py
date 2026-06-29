import argparse
import configparser
import os
import shutil
import subprocess
import sys

ANDROID_TEST_DEVICE_ROOT = '/data/local/tmp/unittest'


def _to_string_list(value):
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        return [str(v) for v in value if v]
    return [str(value)]


def _copy_env(env):
    if env is None:
        return dict(os.environ)
    return dict(env)


def _log(log_fn, message):
    if log_fn:
        log_fn(message)


def _find_adb_candidates(env = None):
    env = _copy_env(env)
    adb_candidates = []

    adb_env = env.get('ADB', None)
    if adb_env:
        adb_candidates.extend(_to_string_list(adb_env))

    adb_path = shutil.which('adb')
    if adb_path:
        adb_candidates.append(adb_path)

    for sdk_var in ('ANDROID_NDK_HOME', 'ANDROID_NDK_ROOT'):
        sdk_root = env.get(sdk_var, None)
        if sdk_root:
            adb_candidates.append(os.path.join(sdk_root, '..', 'platform-tools', 'adb'))

    android_home = env.get('ANDROID_HOME', None)
    if android_home:
        adb_candidates.append(os.path.join(android_home, 'platform-tools', 'adb'))

    adb_candidates = [os.path.normpath(path) for path in adb_candidates if path]
    adb_candidates = [path for path in adb_candidates if os.path.exists(path)]
    return adb_candidates


def get_adb_command(env = None, adb = None):
    adb_cmd = _to_string_list(adb)
    if adb_cmd:
        return adb_cmd

    env = _copy_env(env)
    adb_env = env.get('ADB', None)
    adb_cmd = _to_string_list(adb_env)
    if adb_cmd:
        return adb_cmd

    adb_path = shutil.which('adb')
    if adb_path:
        return [adb_path]

    return ['adb']


def get_device_name(env = None, device = None):
    device_name = _to_string_list(device)
    if device_name:
        return device_name[0]

    env = _copy_env(env)
    device_name = _to_string_list(env.get('ANDROID_SERIAL', None))
    if device_name:
        return device_name[0]

    return None


def get_library_name(cwd):
    library_name = os.path.basename(os.path.normpath(cwd))
    if not library_name:
        return 'test'
    return library_name


def get_reverse_port(cwd, configfile, log_fn = None):
    if not configfile:
        return None

    config = configparser.RawConfigParser()
    config_path = os.path.join(cwd, configfile)
    if not os.path.exists(config_path):
        _log(log_fn, 'android-test: config file %s not found, skipping adb reverse' % config_path)
        return None

    config.read(config_path)
    if not config.has_section('server') or not config.has_option('server', 'socket'):
        _log(log_fn, 'android-test: config file %s has no server.socket, skipping adb reverse' % config_path)
        return None

    port = config.getint('server', 'socket')
    if port <= 0:
        _log(log_fn, 'android-test: config file %s requests no adb reverse (socket=%d)' % (config_path, port))
        return None

    return port


def can_run_tests_android(log_fn = None, env = None, device = None):
    adb_candidates = _find_adb_candidates(env)
    device_name = get_device_name(env, device)

    if len(adb_candidates) == 0:
        _log(log_fn, 'No adb found, skipping Android tests')
        return False

    adb = adb_candidates[0]
    try:
        result = subprocess.run([adb, 'devices'], stdout = subprocess.PIPE, stderr = subprocess.STDOUT, text = True, check = False)
    except OSError as e:
        _log(log_fn, 'Failed to run adb (%s), skipping Android tests' % e)
        return False

    if result.returncode != 0:
        _log(log_fn, 'adb devices failed, skipping Android tests')
        if result.stdout:
            _log(log_fn, result.stdout.strip())
        return False

    devices = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line or line.startswith('List of devices attached'):
            continue

        parts = line.split()
        if len(parts) < 2:
            continue

        device_id = parts[0]
        state = parts[1]
        if state == 'device':
            devices.append(device_id)

    if len(devices) == 0:
        _log(log_fn, 'No connected Android devices found, skipping Android tests')
        return False

    if device_name:
        if device_name not in devices:
            _log(log_fn, 'Requested Android device %s not found, connected device(s): %s' % (device_name, ', '.join(devices)))
            return False

        _log(log_fn, 'Found requested Android device: %s' % device_name)
        return True

    _log(log_fn, 'Found Android device(s): %s' % ', '.join(devices))
    return True


class AndroidTestRunner(object):
    def __init__(self, env = None, adb = None, log_fn = None, device = None, device_root = ANDROID_TEST_DEVICE_ROOT):
        self._env = _copy_env(env)
        self._adb = get_adb_command(self._env, adb)
        self._device = get_device_name(self._env, device)
        self._log_fn = log_fn
        self.device_root = device_root
        self._prepared = False
        self._library_name = None
        self._library_root = None
        self._configfile = None
        self._reverse_port = None
        self._reverse_active = False

        if self._adb:
            self._env['ADB'] = self._adb[0]
        if self._device:
            self._env['ANDROID_SERIAL'] = self._device

    def _log(self, message):
        _log(self._log_fn, message)

    def _set_context(self, cwd, configfile = None):
        self._library_name = get_library_name(cwd)
        self._library_root = '%s/%s' % (self.device_root, self._library_name)
        self._configfile = None
        if configfile:
            self._configfile = '%s/%s' % (self._library_root, 'unittest.cfg')

    def _ensure_context(self, cwd = None, configfile = None):
        if cwd:
            self._set_context(cwd, configfile)

        if not self._library_root:
            raise RuntimeError('android-test: missing library root context, provide cwd or call prepare() first')

    def _run(self, args):
        cmd = list(self._adb)
        if self._device:
            cmd.extend(['-s', self._device])
        cmd.extend(args)
        self._log('android-test: %s' % subprocess.list2cmdline(cmd))
        try:
            return subprocess.call(cmd, env = self._env)
        except FileNotFoundError:
            self._log('android-test: adb not found, set ADB or put adb on PATH')
            raise

    def _push_file(self, source, device_path):
        device_parent = os.path.dirname(device_path)
        ret = self._run(['shell', 'mkdir', '-p', device_parent])
        if ret != 0:
            self._log('android-test: failed to create device file parent %s' % device_parent)
            return ret

        ret = self._run(['push', source, device_path])
        if ret != 0:
            self._log('android-test: failed to push file %s' % source)
            return ret

        return 0

    def _push_source_path(self, cwd, source, target):
        if not os.path.isabs(source):
            source = os.path.join(cwd, source)
        source = os.path.normpath(source)

        if not os.path.exists(source):
            self._log('android-test: source %s not found, skipping' % source)
            return 0

        target = (target or '').replace(os.sep, '/').lstrip('/')
        if not target:
            target = os.path.basename(source)

        device_path = '%s/%s' % (self._library_root, target)

        if os.path.isdir(source):
            self._log('android-test: staging folder %s to %s' % (source, device_path))

            ret = self._run(['shell', 'mkdir', '-p', device_path])
            if ret != 0:
                self._log('android-test: failed to create device folder %s' % device_path)
                return ret

            ret = self._run(['push', source + '/.', device_path])
            if ret != 0:
                self._log('android-test: failed to push folder %s' % source)
                return ret
        else:
            self._log('android-test: staging file %s to %s' % (source, device_path))
            ret = self._push_file(source, device_path)
            if ret != 0:
                return ret

        return 0

    def prepare(self, cwd, configfile, folders = None):
        self._prepared = True
        self._log('android-test: prepare cwd=%s configfile=%s' % (cwd, configfile))

        self._set_context(cwd, configfile)
        self._reverse_active = False
        self._reverse_port = get_reverse_port(cwd, configfile, self._log_fn)

        if folders != None and not isinstance(folders, dict):
            raise TypeError('android-test: folders must be a dict or None')

        if self._reverse_port:
            ret = self._run(['reverse', 'tcp:%d' % self._reverse_port, 'tcp:%d' % self._reverse_port])
            if ret != 0:
                self._log('android-test: failed to set up adb reverse for tcp:%d' % self._reverse_port)
                return ret

            self._reverse_active = True
            self._log('android-test: enabled adb reverse tcp:%d tcp:%d' % (self._reverse_port, self._reverse_port))

        ret = self._run(['shell', 'mkdir', '-p', self._library_root])
        if ret != 0:
            self._log('android-test: failed to create device folder')
            return ret

        if folders != None:
            for source, target in folders.items():
                ret = self._push_source_path(cwd, source, target)
                if ret != 0:
                    return ret

        if configfile:
            self._configfile = '%s/%s' % (self._library_root, 'unittest.cfg')
            self._log('android-test: staging config file to %s' % self._configfile)

            ret = self._run(['push', os.path.join(cwd, configfile), self._configfile])
            if ret != 0:
                self._log('android-test: failed to push config file')
                return ret

        self._log('android-test: staged config file %s' % self._configfile)
        return 0

    def stop(self, cwd = None, configfile = None):
        reverse_port = self._reverse_port
        if cwd:
            reverse_port = get_reverse_port(cwd, configfile, self._log_fn)

        self._log('android-test: stop cwd=%s configfile=%s prepared=%s' % (cwd, configfile, self._prepared))
        ret = 0
        if reverse_port:
            ret = self._run(['reverse', '--remove', 'tcp:%d' % reverse_port])
            if ret != 0:
                self._log('android-test: failed to remove adb reverse for tcp:%d' % reverse_port)

        self._prepared = False
        self._configfile = None
        self._reverse_active = False
        self._reverse_port = None
        self._library_name = None
        self._library_root = None
        return ret

    def run_test(self, program, cwd = None, configfile = None):
        self._ensure_context(cwd, configfile)

        device_program = os.path.basename(program)
        device_program_path = '%s/%s' % (self._library_root, device_program)
        self._log('android-test: push %s %s' % (program, device_program_path))

        ret = self._run(['push', program, device_program_path])
        if ret != 0:
            self._log('android-test: failed to push test binary')
            return ret

        ret = self._run(['shell', 'chmod', '755', device_program_path])
        if ret != 0:
            self._log('android-test: failed to chmod test binary')
            return ret

        cmd = ['shell', 'cd', self._library_root, '&&', './%s' % device_program]
        if self._configfile:
            relative_config = './unittest.cfg'
            self._log('android-test: using staged config file %s' % relative_config)
            cmd.append(relative_config)

        return self._run(cmd)


def _parse_stage_args(stage_args):
    if not stage_args:
        return None

    folders = {}
    for source, target in stage_args:
        folders[source] = target
    return folders


def _create_argument_parser():
    parser = argparse.ArgumentParser(description = 'Android test helper utilities')
    subparsers = parser.add_subparsers(dest = 'command', required = True)

    parser_can_run = subparsers.add_parser('can-run-tests', help = 'Check if Android tests can run')
    parser_can_run.add_argument('--adb', help = 'Path to adb executable')
    parser_can_run.add_argument('--device', help = 'Android device serial to require and target')

    parser_prepare = subparsers.add_parser('prepare', help = 'Prepare Android device test environment')
    parser_prepare.add_argument('--cwd', required = True, help = 'Library working directory')
    parser_prepare.add_argument('--config', help = 'Optional test config file path relative to cwd')
    parser_prepare.add_argument('--adb', help = 'Path to adb executable')
    parser_prepare.add_argument('--device', help = 'Android device serial to target')
    parser_prepare.add_argument('--device-root', default = ANDROID_TEST_DEVICE_ROOT, help = 'Root folder on device for staged tests')
    parser_prepare.add_argument('--stage', action = 'append', nargs = 2, metavar = ('SOURCE', 'TARGET'),
        help = 'Stage SOURCE from cwd to TARGET under the library root on device')

    parser_run_test = subparsers.add_parser('run-test', help = 'Run one Android test binary on device')
    parser_run_test.add_argument('--cwd', required = True, help = 'Library working directory')
    parser_run_test.add_argument('--program', required = True, help = 'Path to the built test program')
    parser_run_test.add_argument('--config', help = 'Optional test config file path relative to cwd')
    parser_run_test.add_argument('--adb', help = 'Path to adb executable')
    parser_run_test.add_argument('--device', help = 'Android device serial to target')
    parser_run_test.add_argument('--device-root', default = ANDROID_TEST_DEVICE_ROOT, help = 'Root folder on device for staged tests')

    parser_stop = subparsers.add_parser('stop', help = 'Tear down Android device test environment')
    parser_stop.add_argument('--cwd', required = True, help = 'Library working directory')
    parser_stop.add_argument('--config', help = 'Optional test config file path relative to cwd')
    parser_stop.add_argument('--adb', help = 'Path to adb executable')
    parser_stop.add_argument('--device', help = 'Android device serial to target')
    parser_stop.add_argument('--device-root', default = ANDROID_TEST_DEVICE_ROOT, help = 'Root folder on device for staged tests')

    return parser


def main(argv = None):
    parser = _create_argument_parser()
    args = parser.parse_args(argv)

    if args.command == 'can-run-tests':
        env = None
        if args.adb:
            env = dict(os.environ)
            env['ADB'] = args.adb
        return 0 if can_run_tests_android(print, env = env, device = args.device) else 1

    env = dict(os.environ)
    if getattr(args, 'adb', None):
        env['ADB'] = args.adb

    runner = AndroidTestRunner(env = env, log_fn = print, device = args.device, device_root = args.device_root)

    if args.command == 'prepare':
        folders = _parse_stage_args(args.stage)
        return runner.prepare(args.cwd, args.config, folders = folders)

    if args.command == 'run-test':
        return runner.run_test(args.program, cwd = args.cwd, configfile = args.config)

    if args.command == 'stop':
        return runner.stop(args.cwd, args.config)

    parser.error('Unknown command: %s' % args.command)
    return 2


if __name__ == '__main__':
    sys.exit(main())
