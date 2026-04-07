import os
import subprocess

from waflib import Utils

TEST_HARNESSES = {}


def register_test_harness(platforms, harness):
    if isinstance(platforms, (list, tuple, set)):
        for platform in platforms:
            TEST_HARNESSES[platform] = harness
    else:
        TEST_HARNESSES[platforms] = harness


class TestHarness(object):
    def prepare(self, env, cwd, configfile):
        pass

    def stop(self, env, cwd, configfile):
        pass

    def _proc_env(self, env):
        proc_env = dict(os.environ)
        for key, value in env.items():
            if isinstance(value, str):
                proc_env[key] = value
        return proc_env

    def run_test(self, program, configfile, env, argv):
        proc = subprocess.Popen(argv, env=self._proc_env(env))
        return proc.wait()


class DefaultTestHarness(TestHarness):
    def run_jar_test(self, task, env, configfile):
        mainclass = getattr(task, 'mainclass', '')
        classpath = Utils.to_list(getattr(task, 'classpath', []))
        java_library_paths = Utils.to_list(getattr(task, 'java_library_paths', []))
        jar_path = task.outputs[0].abspath()
        jar_dir = os.path.dirname(jar_path)
        java_library_paths.append(jar_dir)
        classpath.append(jar_path)

        argv = ['java',
                '-Djava.library.path=%s' % os.pathsep.join(java_library_paths),
                '-Djni.library.path=%s' % os.pathsep.join(java_library_paths),
                '-cp', os.pathsep.join(classpath),
                mainclass,
                '-verbose:class']

        proc = subprocess.Popen(argv, env=self._proc_env(env))
        return proc.wait()


class Html5TestHarness(DefaultTestHarness):
    def run_test(self, program, configfile, env, argv):
        argv = [env['NODEJS'][0]] + argv
        return super(Html5TestHarness, self).run_test(program, configfile, env, argv)


class LinuxTestHarness(DefaultTestHarness):
    def run_test(self, program, configfile, env, argv):
        valgrind = env.get('VALGRIND', None)
        if valgrind:
            valgrind = Utils.to_list(valgrind)
            dynamo_home = os.getenv('DYNAMO_HOME')
            argv = valgrind + ['-q', '--leak-check=full',
                    '--suppressions=%s/share/valgrind-python.supp' % dynamo_home,
                    '--suppressions=%s/share/valgrind-libasound.supp' % dynamo_home,
                    '--suppressions=%s/share/valgrind-libdlib.supp' % dynamo_home,
                    '--suppressions=%s/ext/share/luajit/lj.supp' % dynamo_home,
                    '--error-exitcode=1'] + argv
        return super(LinuxTestHarness, self).run_test(program, configfile, env, argv)


DEFAULT_TEST_HARNESS = DefaultTestHarness()

register_test_harness(['js-web', 'wasm-web', 'wasm_pthread-web'], Html5TestHarness())
register_test_harness(['x86_64-linux', 'arm64-linux'], LinuxTestHarness())


def get_test_harness(platform):
    return TEST_HARNESSES.get(platform, DEFAULT_TEST_HARNESS)
