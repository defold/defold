# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import json
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


CMAKE_MODULES = Path(__file__).resolve().parent


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja are required')
class WindowsReconfigureTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='windows-reconfigure-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.build = self.root / 'build'
        self.msvc = 'sdk/ext/SDKs/VisualStudio/VC/Tools/MSVC/14.50.0'
        for name in ('bin/Hostx64/x64/cl.exe', 'include/vcruntime.h', 'lib/x64/libcmt.lib'):
            self.write(self.msvc + '/' + name, '')
        self.create_sdk('10.0.26100.0')
        self.write('modules/tools.cmake', '# This fixture only exercises compiler configuration.\n')
        for name in ('host.c', 'host.cpp', 'test.cpp'):
            self.write(name, 'int fixture;\n')

        # Use CMake's real MSVC platform initialization on any host. The compiler
        # recorder lets Ninja exercise command changes without a Windows SDK binary.
        self.write('CMakeLists.txt', f'''
cmake_minimum_required(VERSION 4.0)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(WIN32 TRUE)
set(TARGET_PLATFORM x86_64-win32)
set(DEFOLD_SDK_ROOT "${{CMAKE_CURRENT_SOURCE_DIR}}/sdk" CACHE PATH "")
set(DEFOLD_BUILD_HOME "${{CMAKE_CURRENT_BINARY_DIR}}/outputs" CACHE PATH "")
set(DEFOLD_VISUAL_STUDIO_ROOT "${{CMAKE_CURRENT_SOURCE_DIR}}/sdk/ext/SDKs/VisualStudio")
set(DEFOLD_WINDOWS_SDK_VERSION 10.0.26100.0 CACHE STRING "")
foreach(lang C CXX)
  set(CMAKE_${{lang}}_COMPILER "${{CMAKE_COMMAND}}")
  set(CMAKE_${{lang}}_COMPILER_FORCED TRUE)
  set(CMAKE_${{lang}}_COMPILER_ID_RUN TRUE)
  set(CMAKE_${{lang}}_COMPILER_ID MSVC)
  set(CMAKE_${{lang}}_COMPILER_VERSION 19.50)
  set(CMAKE_${{lang}}_COMPILER_ARCHITECTURE_ID x64)
  set(CMAKE_${{lang}}_EXTENSIONS_COMPUTED_DEFAULT ON)
endforeach()
set(CMAKE_C_STANDARD_COMPUTED_DEFAULT 90)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT 14)
set(CMAKE_RC_COMPILER "${{CMAKE_COMMAND}}")
list(APPEND CMAKE_MODULE_PATH "${{CMAKE_CURRENT_SOURCE_DIR}}/modules" "{CMAKE_MODULES.as_posix()}")
include(defold)
project(WindowsReconfigure LANGUAGES C CXX)
set(CMAKE_CXX_COMPILE_FEATURES cxx_std_11 cxx_std_14 cxx_std_17)
foreach(lang C CXX)
  set(CMAKE_${{lang}}_COMPILE_OBJECT "\\\"{Path(sys.executable).as_posix()}\\\" \\\"{Path(__file__).resolve().as_posix()}\\\" --record-compile <OBJECT> <DEFINES> <INCLUDES> <FLAGS> <SOURCE>")
endforeach()
add_library(host OBJECT host.c host.cpp)
if(BUILD_TESTS)
  add_library(tests OBJECT test.cpp)
endif()
''')

    def write(self, relative_path, content):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def create_sdk(self, version):
        for directory in ('shared', 'ucrt', 'um', 'winrt'):
            self.write(f'sdk/ext/SDKs/Windows Kits/10/Include/{version}/{directory}/fixture.h', '')
        for name in ('um/x64/kernel32.lib', 'ucrt/x64/ucrt.lib'):
            self.write(f'sdk/ext/SDKs/Windows Kits/10/Lib/{version}/{name}', '')

    def run_command(self, *command):
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def configure(self, tests, *options):
        self.run_command('cmake', '-S', str(self.root), '-B', str(self.build), '-G', 'Ninja',
                         '-DBUILD_TESTS=' + ('ON' if tests else 'OFF'), *options)

    def build_objects(self):
        self.run_command('cmake', '--build', str(self.build))
        return {name: json.loads((self.build / 'CMakeFiles/host.dir' / (name + '.obj')).read_text())
                for name in ('host.c', 'host.cpp')}

    def assert_sdk_includes(self, objects, version):
        paths = [self.root / f'sdk/ext/SDKs/Windows Kits/10/Include/{version}/{directory}'
                 for directory in ('shared', 'ucrt', 'um', 'winrt')]
        paths.append(self.root / self.msvc / 'include')
        for obj in objects.values():
            arguments = [arg.replace('\\', '/') for arg in obj['args']]
            for path in paths:
                self.assertEqual(1, sum(path.as_posix() in arg for arg in arguments), arguments)

    # Enabling engine tests must reuse unchanged host objects, including the first-ever configure transition.
    def test_host_objects_are_reused_when_tests_are_enabled(self):
        self.configure(False)
        host = self.build_objects()
        self.assert_sdk_includes(host, '10.0.26100.0')
        for obj in host.values():
            self.assertIn('/DWIN32', obj['args'])
            self.assertNotIn('/EHsc', obj['args'])
            self.assertNotIn('/DNDEBUG', obj['args'])
        self.configure(True)
        self.assertEqual(host, self.build_objects())
        self.assertTrue((self.build / 'CMakeFiles/tests.dir/test.cpp.obj').exists())
        self.configure(True)
        self.assertEqual(host, self.build_objects())

    # Preserve user flags across configuration, while still rebuilding the affected language when they change.
    def test_user_flags_are_preserved_and_changes_rebuild_objects(self):
        self.configure(False, '-DCMAKE_C_FLAGS=/DUSER_C', '-DCMAKE_CXX_FLAGS=/DUSER_CXX /EHsc')
        host = self.build_objects()
        self.assertIn('/DUSER_C', host['host.c']['args'])
        self.assertIn('/DUSER_CXX', host['host.cpp']['args'])
        self.assertNotIn('/EHsc', host['host.cpp']['args'])
        self.configure(True)
        self.assertEqual(host, self.build_objects())
        self.configure(True, '-DCMAKE_C_FLAGS=/DCHANGED_C')
        changed = self.build_objects()
        self.assertNotEqual(host['host.c'], changed['host.c'])
        self.assertEqual(host['host.cpp'], changed['host.cpp'])
        self.assertIn('/DCHANGED_C', changed['host.c']['args'])
        self.assertNotIn('/DUSER_C', changed['host.c']['args'])

    # Switching SDKs must refresh include paths and invalidate objects instead of retaining cached SDK flags.
    def test_sdk_changes_refresh_include_paths(self):
        self.configure(False)
        host = self.build_objects()
        self.create_sdk('10.0.26200.0')
        self.configure(True, '-DDEFOLD_WINDOWS_SDK_VERSION=10.0.26200.0')
        changed = self.build_objects()
        self.assert_sdk_includes(changed, '10.0.26200.0')
        for name, obj in changed.items():
            self.assertNotEqual(host[name], obj)
            self.assertFalse(any('10.0.26100.0' in arg for arg in obj['args']))
        self.configure(True)
        self.assertEqual(changed, self.build_objects())

    # CMake's nested compiler checks need SDK headers even without a target linked to defold_sdk.
    def test_compiler_probes_receive_sdk_include_paths(self):
        self.write('probe_rules.cmake', f'''
foreach(lang C CXX)
  set(CMAKE_${{lang}}_COMPILE_OBJECT "\\\"{Path(sys.executable).as_posix()}\\\" \\\"{Path(__file__).resolve().as_posix()}\\\" --record-probe \\\"${{CMAKE_CURRENT_LIST_DIR}}/probe-${{lang}}.json\\\" <OBJECT> <DEFINES> <INCLUDES> <FLAGS> <SOURCE>")
  set(CMAKE_${{lang}}_CREATE_STATIC_LIBRARY "\\\"${{CMAKE_COMMAND}}\\\" -E touch <TARGET>")
endforeach()
''')
        cmake_file = self.root / 'CMakeLists.txt'
        cmake_source = cmake_file.read_text().replace(
            'project(WindowsReconfigure LANGUAGES C CXX)',
            'set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/probe_rules.cmake")\n'
            'project(WindowsReconfigure LANGUAGES C CXX)')
        cmake_file.write_text(cmake_source + '''
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
foreach(extension c cpp)
  try_compile(probe_result SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/host.${extension}"
    CMAKE_FLAGS "-DCMAKE_USER_MAKE_RULES_OVERRIDE=${CMAKE_CURRENT_SOURCE_DIR}/probe_rules.cmake"
    OUTPUT_VARIABLE probe_output NO_CACHE)
  if(NOT probe_result)
    message(FATAL_ERROR "Compiler probe failed: ${probe_output}")
  endif()
endforeach()
''')
        self.configure(False)
        probes = {lang: json.loads((self.root / f'probe-{lang}.json').read_text()) for lang in ('C', 'CXX')}
        self.assert_sdk_includes(probes, '10.0.26100.0')


if __name__ == '__main__':
    if sys.argv[1:2] in (['--record-compile'], ['--record-probe']):
        arguments = iter(sys.argv[2:])
        probe_output = Path(next(arguments)) if sys.argv[1] == '--record-probe' else None
        output = Path(next(arguments))
        output.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps({'args': list(arguments), 'built': time.time_ns()})
        output.write_text(data)
        if probe_output:
            probe_output.write_text(data)
    else:
        unittest.main()
