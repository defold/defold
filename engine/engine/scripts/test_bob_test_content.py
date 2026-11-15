#!/usr/bin/env python3
import argparse
import configparser
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest


class BobTestContentTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="bob test ü ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.source = self.root / "engine/src/test content ü"
        self.source.mkdir(parents=True)
        (self.source / "game.project").write_text(
            "[project]\ntitle = default\n[bootstrap]\nmain_collection = /main.collectionc\n"
            "[input]\ngame_binding = /game.input_bindingc\n"
            "[display]\nwidth = 640\nheight = 480\n", encoding="utf-8")
        (self.source / "game.input_binding").write_text("", encoding="utf-8")
        for name in ("main", "first", "second"):
            (self.source / (name + ".collection")).write_text('name: "' + name + '"\n', encoding="utf-8")
        (self.source / "first.ini").write_text(
            "[project]\ntitle = first\n[bootstrap]\nmain_collection = /first.collectionc\n"
            "[display]\nwidth = 123\n", encoding="utf-8")
        (self.source / "second.ini").write_text(
            "[bootstrap]\nmain_collection = /second.collectionc\n[display]\nheight = 321\n", encoding="utf-8")
        self.env = dict(os.environ, DM_BOB_ROOTFOLDER=str(self.root / "bob tools"))

    def run_batch(self, commands):
        manifest = self.root / "commands.json"
        manifest.write_text(json.dumps(commands), encoding="utf-8")
        return subprocess.run(
            [self.options.java, *shlex.split(self.env.get("DM_JAVA_RUNTIME_FLAGS", "")),
             "-cp", os.pathsep.join(self.options.classpath), "BobTestContent", str(manifest)],
            cwd=self.root, env=self.env, capture_output=True, encoding="utf-8")

    def build_command(self, output, settings=None):
        command = ["--root", str(self.source.relative_to(self.root)), "--output", output,
                   "--platform", self.options.platform, "build"]
        if settings:
            command.extend(["--settings", str((self.source / settings).relative_to(self.root))])
        return command

    # Settings and output options must not leak between projects in the same JVM.
    def test_settings_are_isolated(self):
        result = self.run_batch([self.build_command("build/first", "first.ini"),
                                 self.build_command("build/second", "second.ini"),
                                 self.build_command("build/default")])
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        for output, title, width, height, collection in (
                ("first", "first", 123, 480, "first"),
                ("second", "default", 640, 321, "second"),
                ("default", "default", 640, 480, "main")):
            with self.subTest(output=output):
                config = configparser.ConfigParser(interpolation=None)
                config.read(self.source / "build" / output / "game.projectc")
                self.assertEqual(title, config["project"]["title"])
                self.assertEqual(width, config.getint("display", "width"))
                self.assertEqual(height, config.getint("display", "height"))
                self.assertEqual("/" + collection + ".collectionc", config["bootstrap"]["main_collection"])

    # A failed settings build must stop the script before later builds and the success stamp.
    def test_failed_build_stops_batch_without_stamp(self):
        (self.source / "invalid.ini").write_text(
            "[bootstrap]\nmain_collection = /missing.collectionc\n", encoding="utf-8")
        stage = self.root / "engine/build/src/test"
        stamp = stage / ".cmake/test_content.stamp"
        script = Path(__file__).resolve().parents[1] / "src/test/build_cmake_test_content.py"
        command = [sys.executable, str(script), "--source-root", str(self.source),
                   "--stage-root", str(stage), "--stamp", str(stamp),
                   "--java", self.options.java, "--platform", self.options.platform]
        for entry in self.options.classpath:
            command.extend(["--classpath", entry])
        for settings in ("first.ini", "invalid.ini", "second.ini"):
            command.extend(["--settings", settings])
        result = subprocess.run(command, cwd=self.root, env=self.env, capture_output=True, encoding="utf-8")
        self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("missing.collection", result.stdout + result.stderr)
        self.assertTrue((stage / "build/default/first.collectionc").exists())
        self.assertFalse((stage / "build/default/second.collectionc").exists())
        self.assertFalse(stamp.exists())

    # Invalid CLI options must propagate Bob's failure and prevent subsequent commands.
    def test_invalid_options_stop_batch(self):
        result = self.run_batch([["--no-such-bob-option"], self.build_command("build/default")])
        self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("no-such-bob-option", result.stdout + result.stderr)
        self.assertFalse((self.source / "build/default/game.projectc").exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--java", required=True)
    parser.add_argument("--classpath", action="append", required=True)
    parser.add_argument("--platform", required=True)
    BobTestContentTests.options, tests = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *tests])
