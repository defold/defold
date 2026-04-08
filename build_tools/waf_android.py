import os
import shutil
import subprocess
import configparser

from waflib import Logs, Utils
from waflib.Configure import conf

from waf_tests import TestHarness, register_test_harness


class AndroidTestHarness(TestHarness):
    def __init__(self):
        self.device_root = '/data/local/tmp/unittest'
        self._prepared = False
        self._library_name = None
        self._library_root = None
        self._configfile = None
        self._reverse_port = None
        self._reverse_active = False

    def _get_library_name(self, cwd):
        library_name = os.path.basename(os.path.normpath(cwd))
        if not library_name:
            return 'test'
        return library_name

    def _get_reverse_port(self, cwd, configfile):
        if not configfile:
            return None

        config = configparser.RawConfigParser()
        config_path = os.path.join(cwd, configfile)
        if not os.path.exists(config_path):
            Logs.info('android-test: config file %s not found, skipping adb reverse', config_path)
            return None

        config.read(config_path)
        if not config.has_section('server') or not config.has_option('server', 'socket'):
            Logs.info('android-test: config file %s has no server.socket, skipping adb reverse', config_path)
            return None

        port = config.getint('server', 'socket')
        if port <= 0:
            Logs.info('android-test: config file %s requests no adb reverse (socket=%d)', config_path, port)
            return None

        return port

    def _adb(self, env):
        if 'ADB' in env:
            return Utils.to_list(env['ADB'])

        adb_path = shutil.which('adb')
        if adb_path:
            return [adb_path]

        return ['adb']

    def _run(self, env, args):
        cmd = self._adb(env) + args
        proc_env = self._proc_env(env)
        if 'ADB' in env:
            adb = Utils.to_list(env['ADB'])
            if adb:
                proc_env['ADB'] = adb[0]
        Logs.info('android-test: %s', subprocess.list2cmdline(cmd))
        try:
            return subprocess.call(cmd, env=proc_env)
        except FileNotFoundError:
            Logs.error('android-test: adb not found, set ADB or put adb on PATH')
            raise

    def _push_file(self, env, source, device_path):
        device_parent = os.path.dirname(device_path)
        ret = self._run(env, ['shell', 'mkdir', '-p', device_parent])
        if ret != 0:
            Logs.info('android-test: failed to create device file parent %s', device_parent)
            return ret

        ret = self._run(env, ['push', source, device_path])
        if ret != 0:
            Logs.info('android-test: failed to push file %s', source)
            return ret

        return 0

    def _push_source_path(self, env, cwd, source, target):
        if not os.path.isabs(source):
            source = os.path.join(cwd, source)
        source = os.path.normpath(source)

        if not os.path.exists(source):
            Logs.info('android-test: source %s not found, skipping', source)
            return 0

        target = (target or '').replace(os.sep, '/').lstrip('/')
        if not target:
            target = os.path.basename(source)

        device_path = '%s/%s' % (self._library_root, target)

        if os.path.isdir(source):
            Logs.info('android-test: staging folder %s to %s', source, device_path)

            ret = self._run(env, ['shell', 'mkdir', '-p', device_path])
            if ret != 0:
                Logs.info('android-test: failed to create device folder %s', device_path)
                return ret

            ret = self._run(env, ['push', source + '/.', device_path])
            if ret != 0:
                Logs.info('android-test: failed to push folder %s', source)
                return ret
        else:
            Logs.info('android-test: staging file %s to %s', source, device_path)
            ret = self._push_file(env, source, device_path)
            if ret != 0:
                return ret

        return 0

    def prepare(self, env, cwd, configfile, folders = None):
        self._prepared = True
        Logs.info('android-test: prepare cwd=%s configfile=%s', cwd, configfile)

        self._library_name = self._get_library_name(cwd)
        self._library_root = '%s/%s' % (self.device_root, self._library_name)
        self._configfile = None
        self._reverse_active = False
        self._reverse_port = self._get_reverse_port(cwd, configfile)

        if folders != None and not isinstance(folders, dict):
            raise TypeError('android-test: folders must be a dict or None')

        if self._reverse_port:
            ret = self._run(env, ['reverse', 'tcp:%d' % self._reverse_port, 'tcp:%d' % self._reverse_port])
            if ret != 0:
                Logs.info('android-test: failed to set up adb reverse for tcp:%d', self._reverse_port)
                return

            self._reverse_active = True
            Logs.info('android-test: enabled adb reverse tcp:%d tcp:%d', self._reverse_port, self._reverse_port)

        ret = self._run(env, ['shell', 'mkdir', '-p', self._library_root])
        if ret != 0:
            Logs.info('android-test: failed to create device folder')
            return

        if folders != None:
            for source, target in folders.items():
                ret = self._push_source_path(env, cwd, source, target)
                if ret != 0:
                    return

        if configfile:
            self._configfile = '%s/%s' % (self._library_root, 'unittest.cfg')
            Logs.info('android-test: staging config file to %s', self._configfile)

            ret = self._run(env, ['push', os.path.join(cwd, configfile), self._configfile])
            if ret != 0:
                Logs.info('android-test: failed to push config file')
                return

        Logs.info('android-test: staged config file %s', self._configfile)

    def stop(self, env, cwd, configfile):
        Logs.info('android-test: stop cwd=%s configfile=%s prepared=%s', cwd, configfile, self._prepared)
        if self._reverse_active and self._reverse_port:
            ret = self._run(env, ['reverse', '--remove', 'tcp:%d' % self._reverse_port])
            if ret != 0:
                Logs.info('android-test: failed to remove adb reverse for tcp:%d', self._reverse_port)
        self._prepared = False
        self._configfile = None
        self._reverse_active = False
        self._reverse_port = None
        self._library_name = None
        self._library_root = None

    def run_test(self, program, configfile, env, argv):
        device_program = os.path.basename(program)
        device_program_path = '%s/%s' % (self._library_root, device_program)
        Logs.info('android-test: push %s %s', program, device_program_path)
        ret = self._run(env, ['push', program, device_program_path])
        if ret != 0:
            Logs.info('android-test: failed to push test binary')
            return ret

        ret = self._run(env, ['shell', 'chmod', '755', device_program_path])
        if ret != 0:
            Logs.info('android-test: failed to chmod test binary')
            return ret

        cmd = ['shell', 'cd', self._library_root, '&&', './%s' % device_program]
        if self._configfile:
            relative_config = './unittest.cfg'

            Logs.info('android-test: using staged config file %s', relative_config)
            cmd.append(relative_config)

        Logs.info('android-test: %s', subprocess.list2cmdline(self._adb(env) + cmd))
        return self._run(env, cmd)


register_test_harness(['armv7-android', 'arm64-android'], AndroidTestHarness())


def options(opt):
    pass


def configure(conf):
    conf.find_program('adb', var='ADB', mandatory=False)
