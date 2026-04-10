import build_android

from waflib import Logs, Utils
from waflib.Configure import conf

from waf_tests import TestHarness, register_test_harness


class AndroidTestHarness(TestHarness):
    def __init__(self):
        self._runner = None

    def _create_runner(self, env):
        proc_env = self._proc_env(env)
        adb = Utils.to_list(env['ADB']) if 'ADB' in env else None
        return build_android.AndroidTestRunner(env = proc_env, adb = adb, log_fn = Logs.info)

    def _get_runner(self, env):
        if self._runner is None:
            self._runner = self._create_runner(env)
        return self._runner

    def prepare(self, env, cwd, configfile, folders = None):
        self._runner = self._create_runner(env)
        self._runner.prepare(cwd, configfile, folders = folders)

    def stop(self, env, cwd, configfile):
        runner = self._get_runner(env)
        runner.stop(cwd, configfile)
        self._runner = None

    def run_test(self, program, configfile, env, argv):
        runner = self._get_runner(env)
        return runner.run_test(program)


register_test_harness(['armv7-android', 'arm64-android'], AndroidTestHarness())


def options(opt):
    pass


def configure(conf):
    conf.find_program('adb', var='ADB', mandatory=False)
