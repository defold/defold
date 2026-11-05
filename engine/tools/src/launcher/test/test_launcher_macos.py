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
printf '%s' "$$" > "$LAUNCHER_TEST_PID_FILE"
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
            "vmargs = -Ddefold.launcherpath=${bootstrap.launcherpath}\n"
            "main = EditorMain\n")

    def launch(self, *args, exit_code=0, closed_output=False, launcher=None,
               cwd=None, search_path=None):
        env = dict(os.environ, LAUNCHER_TEST_EXIT_CODE=str(exit_code),
                   LAUNCHER_TEST_PID_FILE=str(self.directory / "java.pid"))
        if search_path is not None:
            env["PATH"] = search_path
        command = str(self.launcher) if launcher is None else launcher
        # A separate process group also lets a failed regression clean up a
        # child left behind by the old fork-and-wait implementation.
        with subprocess.Popen([command, *args], env=env, cwd=cwd,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, start_new_session=True) as process:
            try:
                if closed_output:
                    process.stdout.close()
                    process.wait(timeout=15)
                    output = ""
                else:
                    output, _ = process.communicate(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                self.fail("Launcher did not exit within 15 seconds")
            return process.pid, process.returncode, output

    # The Java stub must inherit the launcher PID, proving no waiting parent
    # remains. Also verify startup logging, classpath/main arguments, a project
    # path containing spaces, and CFProcessPath survive the handoff.
    def test_replaces_launcher_and_preserves_arguments(self):
        pid, code, output = self.launch("--project=path with spaces/game.project")
        self.assertEqual(0, code, output)
        self.assertIn("Launcher version", output)
        self.assertIn(f"pid={pid}\n", output)
        self.assertIn("arg=-cp\narg=editor.jar\n", output)
        self.assertIn("arg=EditorMain\narg=--project=path with spaces/game.project\n", output)
        self.assertIn(f"launcher={self.launcher}\n", output)

    # Relative and PATH launches must record an absolute executable path that
    # can start another editor after changing directories (issue #7729).
    def test_relaunches_after_relative_and_path_launches(self):
        working_directory = self.directory / "working"
        working_directory.mkdir()
        launches = (
            ("relative", os.path.relpath(self.launcher, working_directory), None),
            ("absolute PATH entry", "Defold", str(self.launcher.parent)),
            ("relative PATH entry", "Defold", os.path.relpath(self.launcher.parent, working_directory)),
        )
        for name, command, search_path in launches:
            with self.subTest(launch=name):
                _, code, output = self.launch(launcher=command, cwd=working_directory,
                                              search_path=search_path)
                self.assertEqual(0, code, output)
                prefix = "arg=-Ddefold.launcherpath="
                launcher_paths = [line[len(prefix):] for line in output.splitlines()
                                  if line.startswith(prefix)]
                self.assertEqual(1, len(launcher_paths), output)
                launcher_path = launcher_paths[0]
                self.assertTrue(Path(launcher_path).is_absolute(), output)
                self.assertEqual(self.launcher, Path(launcher_path).resolve())
                self.assertIn(f"launcher={launcher_path}\n", output)
                _, code, output = self.launch(launcher=launcher_path, cwd=self.directory,
                                              search_path="")
                self.assertEqual(0, code, output)
                self.assertIn("arg=EditorMain\n", output)

    # Pass through both an ordinary failure code (7) and the legacy restart code
    # (17) unchanged, without relaunching the executable when it exits with 17.
    def test_preserves_exit_status(self):
        for exit_code in (7, 17):
            with self.subTest(exit_code=exit_code):
                _, code, output = self.launch(exit_code=exit_code)
                self.assertEqual(exit_code, code, output)

    # A nonexistent configured Java executable must produce a launch failure
    # diagnostic and exit with status 127.
    def test_missing_java_exits_with_error(self):
        _, code, output = self.launch(f"--config=launcher.java={self.directory}/missing-java")
        self.assertEqual(127, code, output)
        self.assertIn("Failed to launch application:", output)

    # Simulate the old editor closing the new launcher's output pipe on restart.
    # Logging must not kill the launcher with SIGPIPE; the PID file verifies
    # that the Java stub starts in the original process despite the closed pipe.
    def test_restart_survives_closed_output_pipe(self):
        pid, code, _ = self.launch(closed_output=True)
        self.assertEqual(0, code)
        self.assertEqual(str(pid), (self.directory / "java.pid").read_text())


if __name__ == "__main__":
    LAUNCHER = Path(sys.argv.pop(1)).resolve()
    unittest.main()
