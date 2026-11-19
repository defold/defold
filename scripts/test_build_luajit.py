# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import hashlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(all(shutil.which(tool) for tool in ('cmake', 'ninja', 'git')),
                     'CMake, Ninja and Git are required')
class LuaJITSourceTests(unittest.TestCase):
    # Both patches must apply inside a checkout, including recovery from an old cached extraction.
    def test_patches_inside_git_checkout(self):
        repository = Path(__file__).resolve().parent.parent
        luajit = repository / 'external/luajit'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            (root / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 4.0)
project(luajit_sources LANGUAGES C CXX)
# Source preparation does not require a target SDK or a runtime build.
set(DEFOLD_CMAKE_INCLUDED ON)
set(DEFOLD_SDK_ROOT "${CMAKE_BINARY_DIR}/sdk")
set(TARGET_PLATFORM wasm-web)
add_library(defold_sdk INTERFACE)
add_subdirectory("${LUAJIT_DIRECTORY}" luajit)
''')
            command = ['cmake', '-S', str(root), '-B', str(root / 'build'), '-G', 'Ninja',
                       '-DLUAJIT_DIRECTORY=' + luajit.as_posix()]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(0, result.returncode, result.stdout)
            source = next((root / 'build/luajit/sources').glob('LuaJIT-*'))
            self.assertIn('lua_rename.h', (source / 'src/lua.h').read_text())
            self.assertTrue((source / 'src/lj_vminterp.c').is_file())
            self.assertIn('LJ_TARGET_WASM32', (source / 'src/lj_arch.h').read_text())

            # The previous signature could be written even when the Wasm patch was skipped.
            archive = luajit / (source.name.removeprefix('LuaJIT-') + '.zip')
            old_signature = ''.join(hashlib.sha256(path.read_bytes()).hexdigest() + '\n'
                                    for path in (archive, luajit / 'defold.patch', luajit / 'defold-wasm.patch'))
            (source / '.defold-source').write_text(old_signature)
            (source / 'src/lj_vminterp.c').unlink()
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(0, result.returncode, result.stdout)
            self.assertTrue((source / 'src/lj_vminterp.c').is_file())

            # Reuse a correctly patched cache without extracting and patching it again.
            probe = source / 'cache-probe'
            probe.touch()
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(0, result.returncode, result.stdout)
            self.assertTrue(probe.is_file())


if __name__ == '__main__':
    unittest.main()
