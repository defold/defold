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
import unittest
import zipfile

import build


REPOSITORY = Path(__file__).resolve().parents[1]


class PythonProtobufTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='protobuf setup '))
        self.root = Path(directory).resolve()
        self.ext = self.root / 'sdk' / 'ext'
        self.packages = self.ext / 'lib' / 'python'
        self.wheel = REPOSITORY / 'packages' / 'python' / f'protobuf-{build.sdk.VERSION_PYTHON_PROTOBUF}-py3-none-any.whl'
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
        self.configuration.get_python = lambda: [sys.executable]

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
        info = self.configuration._check_python_protobuf()
        self.assertEqual(build.sdk.VERSION_PYTHON_PROTOBUF, info['version'])
        self.assertEqual(self.packages / 'google' / 'protobuf' / '__init__.py', Path(info['path']))

    # Missing and mismatched runtimes must fail during setup with a concrete repair command.
    def test_preflight_rejects_missing_or_wrong_runtime(self):
        init = self.packages / 'google' / 'protobuf' / '__init__.py'
        init.write_text("__version__ = '0.0.0'\n")
        with self.assertRaisesRegex(build.sdk.SDKException, '0.0.0.*required.*install_ext'):
            self.configuration._check_python_protobuf()
        shutil.rmtree(self.packages)
        with self.assertRaisesRegex(build.sdk.SDKException, 'Could not load.*install_ext'):
            self.configuration._check_python_protobuf()

    # A fresh offline installation must install and validate the pinned wheel rather than a host package.
    @unittest.skipUnless(importlib.util.find_spec('pip'), 'pip is required for installation')
    def test_setup_installs_and_validates_pinned_wheel(self):
        shutil.rmtree(self.packages)
        self.configuration._install_python_packages([f'protobuf=={build.sdk.VERSION_PYTHON_PROTOBUF}'])
        info = self.configuration._check_python_protobuf()
        self.assertEqual(build.sdk.VERSION_PYTHON_PROTOBUF, info['version'])
        self.assertTrue(Path(info['path']).is_relative_to(self.packages))


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
        self.expected_version = build.sdk.VERSION_PYTHON_PROTOBUF.split('.', 1)[1]
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
include("${DEFOLD_HOME}/scripts/cmake/functions_sdk.cmake")
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
        path_compiler = self.protoc(self.programs, self.expected_version)
        initial = self.configure()
        self.assertEqual(0, initial.returncode, initial.stdout + initial.stderr)
        selected = self.root / 'build' / 'selected_protoc.txt'
        self.assertEqual(str(path_compiler), selected.read_text())
        self.protoc(self.programs, '36.2')
        sdk_compiler = self.protoc(self.sdk / 'ext' / 'bin' / 'test-host', self.expected_version)
        updated = self.configure()
        self.assertEqual(0, updated.returncode, updated.stdout + updated.stderr)
        self.assertEqual(str(sdk_compiler), selected.read_text())
        cache = (self.root / 'build' / 'CMakeCache.txt').read_text()
        self.assertIn(f'DEFOLD_PROTOC_EXECUTABLE:FILEPATH={sdk_compiler}', cache)

    # A newer host Protoc must fail at preflight instead of generating code the bundled runtime cannot load.
    def test_incompatible_path_compiler_is_rejected(self):
        self.protoc(self.programs, '36.2')
        result = self.configure()
        self.assertNotEqual(0, result.returncode)
        diagnostic = ' '.join(result.stderr.split())
        self.assertIn('libprotoc 36.2', diagnostic)
        self.assertIn(f'requires protoc {self.expected_version}', diagnostic)
        self.assertIn('./scripts/build.py install_ext', diagnostic)


if __name__ == '__main__':
    unittest.main()
