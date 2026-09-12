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

"""Exercise the macOS launcher in a temporary app bundle, without a JVM."""

import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest


class LauncherTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="defold-launcher-")
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name).resolve()
        contents = self.directory / "Launcher Test.app" / "Contents"
        macos = contents / "MacOS"
        resources = contents / "Resources"
        macos.mkdir(parents=True)
        resources.mkdir()
        self.launcher = macos / "Defold"
        shutil.copy2(LAUNCHER, self.launcher)
        with (contents / "Info.plist").open("wb") as stream:
            plistlib.dump({
                "CFBundleExecutable": "Defold",
                "CFBundleIdentifier": "com.defold.launcher-test",
                "CFBundleName": "Launcher Test",
                "CFBundlePackageType": "APPL",
            }, stream)
        java = self.directory / "Java Stub"
        java.write_text('''#!/bin/sh
printf 'pid=%s\n' "$$"
printf 'arg=%s\n' "$@"
printf 'launcher=%s\n' "$CFProcessPath"
exit "${LAUNCHER_TEST_EXIT_CODE:-0}"
''')
        java.chmod(0o755)
        (resources / "config").write_text(
            "[launcher]\n"
            f"java = {java}\n"
            "jar = editor.jar\n"
            "main = EditorMain\n")

    def launch(self, *args, exit_code=0):
        env = dict(os.environ, LAUNCHER_TEST_EXIT_CODE=str(exit_code))
        # A separate process group also lets a failed regression clean up a
        # child left behind by the old fork-and-wait implementation.
        with subprocess.Popen([str(self.launcher), *args], env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, start_new_session=True) as process:
            try:
                output, _ = process.communicate(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.communicate()
                self.fail("Launcher did not exit within 15 seconds")
            return process.pid, process.returncode, output

    def test_replaces_launcher_and_preserves_arguments(self):
        pid, code, output = self.launch("--project=path with spaces/game.project")
        self.assertEqual(0, code, output)
        self.assertIn("Launcher version", output)
        self.assertIn(f"pid={pid}\n", output)
        self.assertIn("arg=-cp\narg=editor.jar\n", output)
        self.assertIn("arg=EditorMain\narg=--project=path with spaces/game.project\n", output)
        self.assertIn(f"launcher={self.launcher}\n", output)

    def test_preserves_exit_status(self):
        for exit_code in (7, 17):
            with self.subTest(exit_code=exit_code):
                _, code, output = self.launch(exit_code=exit_code)
                self.assertEqual(exit_code, code, output)

    def test_missing_java_exits_with_error(self):
        _, code, output = self.launch(f"--config=launcher.java={self.directory}/missing-java")
        self.assertEqual(127, code, output)
        self.assertIn("Failed to launch application:", output)


if __name__ == "__main__":
    LAUNCHER = Path(sys.argv.pop(1)).resolve()
    unittest.main()
