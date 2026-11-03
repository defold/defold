"""Exercise the Linux launcher and relaunch from another working directory."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class LauncherTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="defold-launcher-linux-")
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name).resolve()
        self.install = self.directory / "Defold Install"
        self.install.mkdir()
        self.working = self.directory / "working"
        self.working.mkdir()
        self.launcher = self.install / "Defold"
        shutil.copy2(LAUNCHER, self.launcher)
        java = self.install / "Java Stub"
        java.write_text("#!/bin/sh\nprintf 'arg=%s\\n' \"$@\"\n")
        java.chmod(0o755)
        (self.install / "config").write_text(
            "[launcher]\n"
            "java = ${bootstrap.resourcespath}/Java Stub\n"
            "jar = ${bootstrap.resourcespath}/editor.jar\n"
            "vmargs = -Ddefold.launcherpath=${bootstrap.launcherpath}\n"
            "main = EditorMain\n")

    def check_relaunch(self, command, search_path=None):
        env = dict(os.environ)
        if search_path is not None:
            env["PATH"] = search_path
        result = subprocess.run([command], cwd=self.working, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=15)
        self.assertEqual(0, result.returncode, result.stdout)
        prefix = "arg=-Ddefold.launcherpath="
        paths = [line[len(prefix):] for line in result.stdout.splitlines()
                 if line.startswith(prefix)]
        self.assertEqual(1, len(paths), result.stdout)
        self.assertTrue(Path(paths[0]).is_absolute(), result.stdout)
        self.assertEqual(self.launcher, Path(paths[0]).resolve())
        env["PATH"] = ""
        child = subprocess.run([paths[0], "--project=path with spaces/game.project"],
                               cwd=self.directory, env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, timeout=15)
        self.assertEqual(0, child.returncode, child.stdout)
        self.assertIn("arg=EditorMain\n", child.stdout)
        self.assertIn("arg=--project=path with spaces/game.project\n", child.stdout)

    # Absolute launches must preserve their executable path across a relaunch.
    def test_absolute_launch(self):
        self.check_relaunch(str(self.launcher))

    # A relative invocation outside the install directory must survive a new cwd.
    def test_relative_launch(self):
        self.check_relaunch(os.path.relpath(self.launcher, self.working))

    # PATH lookup must locate both the config and the executable for a relaunch.
    def test_absolute_path_entry(self):
        self.check_relaunch("Defold", str(self.install))

    # A relative PATH entry must be resolved before a subsequent cwd change.
    def test_relative_path_entry(self):
        self.check_relaunch("Defold", os.path.relpath(self.install, self.working))


if __name__ == "__main__":
    LAUNCHER = Path(sys.argv.pop(1)).resolve()
    unittest.main()
