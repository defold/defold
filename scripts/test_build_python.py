import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from threading import Event
import unittest
from unittest import mock
import zipfile

import build


REPOSITORY = Path(__file__).resolve().parents[1]


class PythonProtobufTests(unittest.TestCase):
    def setUp(self):
        self.output = io.StringIO()
        self.enterContext(contextlib.redirect_stdout(self.output))
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='protobuf setup '))
        self.root = Path(directory).resolve()
        self.ext = self.root / 'sdk' / 'ext'
        self.packages = self.ext / 'lib' / 'python'
        self.protobuf_version = '7.35.1'
        self.wheel = REPOSITORY / 'packages' / 'python' / f'protobuf-{self.protobuf_version}-py3-none-any.whl'
        with zipfile.ZipFile(self.wheel) as wheel:
            wheel.extractall(self.packages)

        # Model an incompatible host extension discovered through Google's namespace package.
        host = self.root / 'host site-packages'
        extension = host / 'google' / '_upb'
        extension.mkdir(parents=True)
        (extension / '__init__.py').touch()
        (extension / '_message.py').write_text(
            "raise AttributeError(\"module 'google.protobuf.message' has no attribute 'FrozenInstanceError'\")\n")
        customization = self.root / 'customization'
        customization.mkdir()
        (customization / 'sitecustomize.py').write_text(f'import sys; sys.path.insert(0, {str(host)!r})\n')
        self.env = os.environ.copy()
        self.env['PYTHONPATH'] = os.pathsep.join((str(self.packages), str(customization)))
        self.env.pop('PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION', None)
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(REPOSITORY)
        self.configuration.ext = str(self.ext)
        self.configuration._form_env = lambda: self.env.copy()
        self.configuration.verbose = False
        self.configuration.no_colors = True
        self.configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
        self.configuration.tool_checks = build.tool_checks.ToolChecks(self.configuration)

    def run_python(self, *args):
        return subprocess.run([sys.executable, *args], env=self.env, capture_output=True, text=True)

    # The bundled namespace package must not import an incompatible native extension from host site-packages.
    def test_bundled_runtime_isolated_from_host_extension(self):
        probe = 'from google.protobuf.descriptor import FieldDescriptor'
        normal = self.run_python('-c', probe)
        self.assertNotEqual(0, normal.returncode)
        self.assertIn('FrozenInstanceError', normal.stderr)
        isolated = self.run_python('-S', '-c', probe)
        self.assertEqual(0, isolated.returncode, isolated.stderr)

    # Both DDF plugin launchers must isolate imports and preserve paths and arguments containing spaces.
    def test_ddf_launchers_use_bundled_dependencies(self):
        plugins = self.root / 'DDF plugins'
        plugins.mkdir()
        (plugins / 'ddfc.py').write_text(
            'import json, sys\n'
            'from google.protobuf.descriptor import FieldDescriptor\n'
            'print(json.dumps(sys.argv[1:]))\n')
        suffix = '.bat' if os.name == 'nt' else ''
        for mode in ('cxx', 'java'):
            with self.subTest(mode=mode):
                launcher = plugins / f'ddfc_{mode}{suffix}'
                shutil.copy2(REPOSITORY / 'engine' / 'ddf' / 'src' / launcher.name, launcher)
                args = [str(launcher), 'proto with spaces.proto', 'semi;colon']
                if os.name == 'nt':
                    args = f'cmd /d /s /c "{subprocess.list2cmdline(args)}"'
                result = subprocess.run(args, env=self.env, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual([f'--{mode}', 'proto with spaces.proto', 'semi;colon'], json.loads(result.stdout))

    # SDK preflight must import the actual bundled runtime without being affected by host packages.
    def test_preflight_loads_bundled_runtime(self):
        info = self.configuration.tool_checks.check_python_protobuf()
        self.assertEqual(self.protobuf_version, info['version'])
        self.assertEqual(self.packages / 'google' / 'protobuf' / '__init__.py', Path(info['path']))

    # Missing or broken runtimes fail preflight with a repair command even when successful checks are silent.
    def test_preflight_rejects_missing_or_broken_runtime(self):
        init = self.packages / 'google' / 'protobuf' / '__init__.py'
        init.write_text("raise ImportError('broken bundled runtime')\n")
        for state in ('broken', 'missing'):
            with self.subTest(state=state):
                if state == 'missing':
                    shutil.rmtree(self.packages)
                self.output.seek(0)
                self.output.truncate(0)
                with self.assertRaises(SystemExit) as raised:
                    self.configuration.tool_checks.check_python_protobuf(silent=True)
                self.assertEqual(1, raised.exception.code)
                message = self.output.getvalue()
                self.assertIn('Python Protobuf: Failed', message)
                self.assertIn('Could not load the bundled Python Protobuf runtime', message)
                self.assertIn('./scripts/build.py install_ext', message)
                self.assertIn('broken bundled runtime' if state == 'broken' else 'No module named', message)

    # Installing a wheel invalidates its cached runtime check and leaves validation to the next SDK preflight.
    @unittest.skipUnless(importlib.util.find_spec('pip'), 'pip is required for installation')
    def test_setup_defers_validation_and_invalidates_cached_runtime(self):
        with mock.patch.object(build.run, 'env_command', wraps=build.run.env_command) as command:
            self.configuration.tool_checks.check_python_protobuf(silent=True)
            shutil.rmtree(self.packages)
            self.configuration._install_python_packages([f'protobuf=={self.protobuf_version}'])
            self.assertEqual(1, sum('-S' in call.args[1] for call in command.call_args_list))
            info = self.configuration.tool_checks.check_python_protobuf()
            self.assertEqual(2, sum('-S' in call.args[1] for call in command.call_args_list))
        self.assertEqual(self.protobuf_version, info['version'])
        self.assertTrue(Path(info['path']).is_relative_to(self.packages))


class ExternalPackagesTests(unittest.TestCase):
    def setUp(self):
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='external packages '))
        self.configuration = build.Configuration(dynamo_home=directory, target_platform=build.get_host_platform(),
                                                 no_colors=True)
        self.python_packages = Path(self.configuration.ext) / 'lib/python'
        self.configuration._form_env = lambda: os.environ.copy()
        self.configuration._log = lambda message: None
        self.configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
        self.install_sdk = self.enterContext(mock.patch.object(self.configuration, 'install_sdk'))
        self.install_files = self.enterContext(mock.patch.object(self.configuration, '_install_files'))
        self.has_build_hook = self.enterContext(mock.patch.object(build, 'has_hook_module', return_value=False))
        self.enterContext(mock.patch.object(self.configuration, 'check_tar'))

    # Default package setup overlaps pip with archive extraction and any requested SDK install.
    def test_setup_overlaps_wheels_with_sdk_and_archives(self):
        wheels_started = Event()
        archives_extracted = Event()
        wheels_finished = Event()

        def install_wheels(env, command, *, cwd):
            self.assertEqual(self.configuration.defold_root, cwd)
            self.assertIn('--no-index', command)
            self.assertEqual(build.EXT_PYTHON_PACKAGES, command[-len(build.EXT_PYTHON_PACKAGES):])
            self.assertEqual(self.python_packages, Path(command[command.index('-t') + 1]))
            (self.python_packages / 'example.py').write_text('wheel')
            wheels_started.set()
            self.assertTrue(archives_extracted.wait(10))
            wheels_finished.set()

        def install_files(title, files, *, extract=False):
            if extract:
                self.assertTrue(wheels_started.wait(10))
                self.assertFalse(wheels_finished.is_set())
                self.assertTrue((self.python_packages / 'example.py').is_file())
                (Path(self.configuration.ext) / 'lib/archive.a').write_text('archive')
                archives_extracted.set()

        self.install_sdk.side_effect = lambda: self.assertTrue(wheels_started.wait(10))
        self.install_files.side_effect = install_files
        tracker = self.configuration.build_tracker
        tracker.start_command('install_ext_packages')
        with mock.patch.object(build.run, 'env_command', side_effect=install_wheels) as run:
            self.configuration.install_ext_packages(install_sdk=True)
        tracker.end_command('install_ext_packages')

        run.assert_called_once()
        self.install_sdk.assert_called_once_with()
        self.assertTrue(wheels_finished.is_set())
        self.assertEqual('wheel', (self.python_packages / 'example.py').read_text())
        self.assertTrue((Path(self.configuration.ext) / 'lib/archive.a').is_file())
        steps = tracker.command_times['install_ext_packages']['steps']
        self.assertTrue({'SDK installation', 'Package extraction', 'Python wheels'} <= steps.keys())

    # A private archive may own the same package. Pip replaces that package after SDK and archive setup.
    def test_private_archives_are_installed_before_wheels(self):
        self.has_build_hook.return_value = True
        package = self.python_packages / 'markdown'
        other = self.python_packages / 'private_module.py'

        def install_files(title, files, *, extract=False):
            if extract:
                self.install_sdk.assert_called_once_with()
                package.mkdir()
                (package / 'archive_only.py').touch()
                other.write_text('private')

        self.install_files.side_effect = install_files
        with mock.patch.object(build, 'EXT_PYTHON_PACKAGES', ['Markdown==3.3.7']):
            self.configuration.install_ext_packages(install_sdk=True)
        self.has_build_hook.assert_called_once_with('build', self.configuration.target_platform)
        self.assertTrue((package / '__init__.py').is_file())
        self.assertFalse((package / 'archive_only.py').exists())
        self.assertEqual('private', other.read_text())

    # Pip must report an unavailable wheel, preserve installed packages and allow a subsequent retry.
    def test_setup_propagates_wheel_failure(self):
        existing = self.python_packages / 'existing.py'
        existing.write_text('installed')
        with mock.patch.object(build, 'EXT_PYTHON_PACKAGES', ['Markdown==3.3.7', 'missing-defold-wheel==0']):
            with self.assertRaises(build.run.ExecException) as raised:
                self.configuration.install_ext_packages()
        self.assertNotEqual(0, raised.exception.retcode)
        self.install_sdk.assert_not_called()
        self.assertEqual([existing], list(self.python_packages.iterdir()))
        with mock.patch.object(build, 'EXT_PYTHON_PACKAGES', ['Markdown==3.3.7']):
            self.configuration.install_ext_packages()
        self.assertTrue((self.python_packages / 'markdown/__init__.py').is_file())
        self.assertEqual('installed', existing.read_text())


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required')
@unittest.skipIf(os.name == 'nt', 'The protoc fixture uses POSIX launchers')
class ProtocSelectionTests(unittest.TestCase):
    def setUp(self):
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='protoc selection '))
        self.root = Path(directory).resolve()
        self.sdk = self.root / 'sdk'
        self.sdk.mkdir()
        self.programs = self.root / 'bin'
        self.programs.mkdir()
        self.project = self.root / 'project'
        self.project.mkdir()
        (self.project / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(protoc_selection LANGUAGES NONE)
function(defold_log)
endfunction()
set(HOST_PLATFORM test-host)
set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH OFF)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
set(CMAKE_FIND_USE_INSTALL_PREFIX OFF)
include("${DEFOLD_HOME}/scripts/cmake/tools_protoc.cmake")
file(WRITE "${CMAKE_BINARY_DIR}/selected_protoc.txt" "${DEFOLD_PROTOC_EXECUTABLE}")
''')

    def protoc(self, directory, version):
        directory.mkdir(parents=True, exist_ok=True)
        program = directory / 'protoc'
        program.write_text(f"#!/bin/sh\nprintf 'libprotoc {version}\\n'\n")
        program.chmod(0o755)
        return program

    def configure(self, *options):
        env = os.environ.copy()
        env['PATH'] = str(self.programs)
        env.pop('CMAKE_GENERATOR', None)
        return subprocess.run([
            shutil.which('cmake'), '-S', str(self.project), '-B', str(self.root / 'build'),
            '-G', 'Unix Makefiles', f'-DCMAKE_MAKE_PROGRAM={sys.executable}',
            f'-DDEFOLD_HOME={REPOSITORY.as_posix()}', f'-DDEFOLD_SDK_ROOT={self.sdk.as_posix()}',
            *options,
        ], env=env, capture_output=True, text=True)

    # Installing the SDK compiler must replace a previously cached PATH compiler during reconfiguration.
    def test_reconfiguration_replaces_cached_path_compiler(self):
        path_compiler = self.protoc(self.programs, '36.2')
        initial = self.configure()
        self.assertEqual(0, initial.returncode, initial.stdout + initial.stderr)
        selected = self.root / 'build' / 'selected_protoc.txt'
        self.assertEqual(str(path_compiler), selected.read_text())
        sdk_compiler = self.protoc(self.sdk / 'ext' / 'bin' / 'test-host', '35.1')
        updated = self.configure(f'-D_DEFOLD_PROTOC_EXECUTABLE:FILEPATH={path_compiler}')
        self.assertEqual(0, updated.returncode, updated.stdout + updated.stderr)
        self.assertEqual(str(sdk_compiler), selected.read_text())
        cache = (self.root / 'build' / 'CMakeCache.txt').read_text()
        self.assertIn(f'DEFOLD_PROTOC_EXECUTABLE:FILEPATH={sdk_compiler}', cache)

    # An installed SDK compiler must take priority over a different local compiler on the first configure.
    def test_sdk_compiler_is_preferred_over_path(self):
        self.protoc(self.programs, '36.2')
        sdk_compiler = self.protoc(self.sdk / 'ext' / 'bin' / 'test-host', '35.1')
        result = self.configure(f'-DCMAKE_PROGRAM_PATH={self.programs}')
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(str(sdk_compiler), (self.root / 'build' / 'selected_protoc.txt').read_text())

    # PATH remains available when no SDK host compiler is installed, without a Python-derived version restriction.
    def test_path_fallback_when_sdk_compiler_is_missing(self):
        path_compiler = self.protoc(self.programs, '36.2')
        result = self.configure()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(str(path_compiler), (self.root / 'build' / 'selected_protoc.txt').read_text())

    # A compiler installed for another platform must not hide a runnable host compiler on PATH.
    def test_other_platform_compiler_does_not_hide_path_compiler(self):
        self.protoc(self.sdk / 'ext' / 'bin' / 'other-host', '35.1')
        path_compiler = self.protoc(self.programs, '36.2')
        result = self.configure()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(str(path_compiler), (self.root / 'build' / 'selected_protoc.txt').read_text())


if __name__ == '__main__':
    unittest.main()
