"""Session-scoped build tool setup, validation, caching, and reporting."""

import contextlib
from dataclasses import dataclass
import io
import json
import os
from os.path import join
import shutil
import sys
import time

from build_console import Style
import run
import sdk
import wasm_runner


MINIMUM_PYTHON_VERSION = (3, 12)


@dataclass
class _Result:
    value: object
    status: str
    path: str = None


class ToolChecks:
    def __init__(self, configuration):
        self.configuration = configuration
        self._checked = {}

    def invalidate_sdk(self):
        for key in list(self._checked):
            if key[1] is not None:
                del self._checked[key]

    def invalidate(self, *names):
        for key in list(self._checked):
            if key[0] in names:
                del self._checked[key]

    def _report(self, name, status, passed=True, path=None, elapsed=None):
        configuration = self.configuration
        message = f'{name}: {status}'
        if configuration.verbose and path:
            message += f' ({path})'
        if elapsed is not None:
            if configuration.verbose:
                message += f' [{elapsed:.2f} s]'
            configuration.build_tracker.record_check_time(name, elapsed)
        mark = '✓' if passed else '✗'
        if sys.stdout.isatty() and 'NO_COLOR' not in os.environ:
            mark = configuration._colorize(mark, Style.SUCCESS if passed else Style.ERROR)
            message = configuration._colorize(message, Style.SUCCESS if passed else Style.FAILED_CHECK)
        encoding = sys.stdout.encoding or 'utf-8'
        try:
            mark.encode(encoding)
        except UnicodeEncodeError:
            mark = '+' if passed else 'x'
        configuration._log(f'  {mark} {message}')

    def _run(self, name, setup):
        started = time.perf_counter()
        output = io.StringIO()
        try:
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                result = setup()
                if result is False:
                    raise sdk.SDKException('SDK validation failed')
            return result, time.perf_counter() - started
        except (Exception, SystemExit) as error:
            self._report(name, 'Failed', passed=False, elapsed=time.perf_counter() - started)
            diagnostic = output.getvalue().strip()
            if diagnostic:
                self.configuration._log(diagnostic)
            if isinstance(error, SystemExit):
                raise
            self.configuration._log(getattr(error, 'output', '') or str(error))
            sys.exit(getattr(error, 'retcode', 1))

    def _complete(self, name, result, elapsed, context=None, silent=False):
        key = (name, context)
        if key not in self._checked:
            self._checked[key] = result
            if silent:
                self.configuration.build_tracker.record_check_time(name, elapsed)
            else:
                self._report(name, result.status, path=result.path, elapsed=elapsed)
        return self._checked[key].value

    def _check(self, name, setup, context=None, silent=False):
        key = (name, context)
        if key in self._checked:
            return self._checked[key].value
        result, elapsed = self._run(name, setup)
        return self._complete(name, result, elapsed, context=context, silent=silent)

    def check_python(self, silent=False):
        return self._check('Python', self._setup_python, silent=silent)

    def _setup_python(self):
        version = '.'.join(str(part) for part in sys.version_info[:3])
        required = '.'.join(str(part) for part in MINIMUM_PYTHON_VERSION)
        if sys.version_info[:2] < MINIMUM_PYTHON_VERSION:
            raise sdk.SDKException(
                f'The build scripts require Python {required}+! Found Python {version}: {sys.executable}')
        return _Result(sys.executable, f'Python {version}', sys.executable)

    def check_tar(self, silent=False):
        return self._check('Tar', self._setup_tar, silent=silent)

    def _setup_tar(self):
        path = shutil.which('tar')
        if not path:
            raise sdk.SDKException('Tar was not found in PATH. Install tar and ensure it is available on PATH.')
        path = os.path.abspath(path)
        run.env_command(os.environ, [path, '--version'], silent=True)
        return _Result(path, 'Found', path)

    def check_cmake(self, silent=False):
        return self._check('CMake', self._setup_cmake, silent=silent)

    def _setup_cmake(self):
        path = shutil.which('cmake')
        if not path:
            raise sdk.SDKException('CMake was not found in PATH. Install CMake and ensure it is available on PATH.')
        path = os.path.abspath(path)
        version = run.env_command(os.environ, [path, '--version'], silent=True)
        return _Result(path, version.splitlines()[0].replace('cmake version', 'CMake'), path)

    def check_python_protobuf(self, silent=False):
        self.check_python(silent=silent)
        context = (self.configuration.ext,)
        return self._check('Python Protobuf', self._setup_python_protobuf, context=context, silent=silent)

    def _setup_python_protobuf(self):
        configuration = self.configuration
        env = configuration._form_env()
        env['PYTHONPATH'] = join(configuration.ext, 'lib', 'python')
        probe = ('import json; import google.protobuf; '
                 'from google.protobuf.compiler import plugin_pb2; '
                 'print(json.dumps({"version": google.protobuf.__version__, "path": google.protobuf.__file__}))')
        try:
            output = run.env_command(env, configuration.get_python() + ['-S', '-c', probe], silent=True)
        except run.ExecException as error:
            raise sdk.SDKException(
                'Could not load the bundled Python Protobuf runtime. Run ./scripts/build.py install_ext.\n' + error.output) from error
        info = json.loads(output)
        return _Result(info, f"Protobuf {info['version']}", info['path'])

    def _setup_sdk(self):
        configuration = self.configuration
        info = sdk.get_sdk_info(join(configuration.ext, 'SDKs'), configuration.target_platform, True)
        if not info:
            raise sdk.SDKException(
                f'No SDK found for {configuration.target_platform}. Check the local SDK setup or run install_sdk.\n'
                'https://github.com/defold/defold/blob/dev/README_BUILD.md#important-prerequisite---platform-sdks')
        target = info.get(configuration.target_platform, {})
        version = target.get('version', info.get('version', ''))
        path = target.get('path') or info.get('sdk') or info.get('emsdk', {}).get('path')
        return _Result(info, f'{configuration.target_platform} {version}'.strip(), path)

    def _setup_compiler(self):
        configuration = self.configuration
        info = configuration.sdk_info
        result = sdk.test_sdk(configuration.target_platform, info, verbose=configuration.verbose)
        if result is False:
            return False
        version = info.get('clang-version')
        path = info.get('clang')
        if not path and info.get('clangname'):
            path = join(info['bintools'], info['clangname'])
        return _Result(result, f'Clang {version}' if version else 'OK', path)

    def _setup_wasm_runner(self):
        runner, errors = self.configuration._find_wasm_test_runner()
        if runner:
            return _Result(runner, runner.name, runner.path)
        if self.configuration.skip_tests:
            return _Result(None, 'Not required (--skip-tests)')
        raise sdk.SDKException(self.configuration._format_wasm_runner_error(errors))

    def _setup_luajit_generator(self):
        configuration = self.configuration
        args = [self.check_cmake(), f'-DDEFOLD_SDK_ROOT:PATH={configuration.dynamo_home}',
                '-DLUAJIT_CHECK_SDK=ON', '-P', join(configuration.defold_root, 'external/luajit/host/emscripten.cmake')]
        run.env_command(os.environ, args, silent=True)
        return _Result(None, 'OK')

    def check_sdk(self, require_protoc=True, silent=False):
        configuration = self.configuration
        checked_before = len(self._checked)
        context = (configuration.host, configuration.target_platform, configuration.dynamo_home, configuration.skip_tests)
        self.check_python(silent=silent)
        configuration.sdk_info = self._check('SDK', self._setup_sdk, context=context, silent=silent)
        self._check('Compiler', self._setup_compiler, context=context, silent=silent)
        if wasm_runner.is_web_platform(configuration.target_platform):
            self._check('Wasm test runner', self._setup_wasm_runner, context=context, silent=silent)
        cmake = self.check_cmake(silent=silent)

        common_needed = any((name, context) not in self._checked for name in ('Java', 'Ninja', 'Toolchain'))
        protoc_needed = require_protoc and ('Protoc', context) not in self._checked
        if common_needed or protoc_needed:
            args = [cmake, '-DDEFOLD_SDK_CHECK=ON', f'-DDEFOLD_SDK_ROOT:PATH={configuration.dynamo_home}',
                    f'-DTARGET_PLATFORM={configuration.target_platform}',
                    '-P', join(configuration.defold_root, 'scripts/cmake/check_install.cmake')]
            if not protoc_needed:
                args.insert(1, '-DDEFOLD_REQUIRE_PROTOC=OFF')
            if not common_needed:
                args.insert(1, '-DDEFOLD_SDK_CHECK_PROTOC_ONLY=ON')
            if configuration.verbose:
                args.insert(1, '-DDEFOLD_VERBOSE=ON')
            self._check_cmake_tools(args, context=context, silent=silent)

        self.check_python_protobuf(silent=silent)
        if configuration.target_platform == 'armv7-android' and configuration.host.endswith('-macos'):
            self._check('LuaJIT generator', self._setup_luajit_generator, context=context, silent=silent)
        if len(self._checked) > checked_before and not silent:
            configuration._log(configuration._colorize(f'All checks passed: {configuration.target_platform}', Style.SUCCESS))
        return configuration.sdk_info

    def _check_cmake_tools(self, args, context=None, silent=False):
        current_check = 'CMake environment'
        diagnostics = []
        started = time.perf_counter()

        def on_output(line):
            nonlocal current_check, started
            if line.startswith('DEFOLD_SDK_CHECK\t'):
                now = time.perf_counter()
                diagnostics.clear()
                fields = line.split('\t', 3)
                current_check = fields[1]
                if len(fields) >= 3:
                    path = fields[3] if len(fields) == 4 else None
                    self._complete(current_check, _Result(path, fields[2], path), now - started,
                                   context=context, silent=silent)
                    current_check = 'CMake environment'
                started = now
            else:
                diagnostics.append(line)

        try:
            # CMake emits machine-readable records; the checker applies terminal styling.
            env = dict(os.environ, CLICOLOR_FORCE='0', CLICOLOR='0')
            run.env_command(env, args, silent=True, on_output=on_output)
        except run.ExecException as error:
            self._report(current_check, self._format_cmake_error(diagnostics), passed=False,
                         elapsed=time.perf_counter() - started)
            if self.configuration.verbose:
                self.configuration._log('\n'.join(diagnostics).strip())
            sys.exit(error.retcode)

    @staticmethod
    def _format_cmake_error(diagnostics):
        for index, line in enumerate(diagnostics):
            if line.startswith('CMake Error:'):
                return line.partition(':')[2].strip()
            if line.startswith('CMake Error'):
                message = []
                for line in diagnostics[index + 1:]:
                    if not line.strip():
                        if message:
                            break
                        continue
                    if not line.startswith((' ', '\t')):
                        break
                    message.extend(line.split())
                if message:
                    return ' '.join(message)
        return 'Failed (use --verbose for details)'
