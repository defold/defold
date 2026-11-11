import argparse
import os
import subprocess
import sys
import unittest
import zipfile


class EngineSharedMainTest(unittest.TestCase):
    # Verifies the packaged manifest entry point loads the native library and
    # exposes generic usage without requiring a feature-specific command.
    def test_jar_usage(self):
        for args in [[], ["--help"]]:
            with self.subTest(args=args):
                result = subprocess.run(JAVA_COMMAND + args, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertIn("<class-name> [arguments]", result.stdout)

    # Verifies dispatch to arbitrary module entry points preserves every argument,
    # including empty strings, spaces and option-like values.
    def test_argument_forwarding(self):
        for args in [[], ["", "two words", "--option", "123"]]:
            with self.subTest(args=args):
                result = subprocess.run(DISPATCH_COMMAND + ["DispatchTestCommands$Echo"] + args,
                                        capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                expected = [str(len(args))] + [f"{i}:{value}" for i, value in enumerate(args)]
                self.assertEqual(expected, result.stdout.splitlines())

    # Verifies failures retain the module's exception and nonzero exit status,
    # rather than exposing a reflection wrapper or reporting success.
    def test_module_failure(self):
        result = subprocess.run(DISPATCH_COMMAND + ["DispatchTestCommands$Fail"],
                                capture_output=True, text=True)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("IllegalStateException: dispatch-test-failure", result.stderr)
        self.assertNotIn("InvocationTargetException", result.stderr)

    # Verifies missing classes, invalid entry points and recursive dispatch are
    # rejected before a module can be invoked incorrectly.
    def test_invalid_entry_points(self):
        for name in ["MissingTestCommand", "DispatchTestCommands$NoMain",
                     "DispatchTestCommands$InstanceMain", "DispatchTestCommands$WrongReturnType",
                     "EngineJni"]:
            with self.subTest(name=name):
                result = subprocess.run(DISPATCH_COMMAND + [name], capture_output=True, text=True)
                self.assertNotEqual(0, result.returncode)

    # Verifies the shipped jar contains its launcher but excludes test fixtures.
    def test_jar_contents(self):
        with zipfile.ZipFile(JAR_PATH) as jar:
            self.assertIn("com/defold/engine/EngineJni.class", jar.namelist())
            self.assertFalse(any("DispatchTestCommands" in name for name in jar.namelist()))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--test-classes", required=True)
    parser.add_argument("java_command", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    JAVA_COMMAND = options.java_command
    jar_index = JAVA_COMMAND.index("-jar")
    JAR_PATH = JAVA_COMMAND[jar_index + 1]
    DISPATCH_COMMAND = JAVA_COMMAND[:jar_index] + [
        "-cp", os.pathsep.join([JAR_PATH, options.test_classes]), "com.defold.engine.EngineJni"]
    unittest.main(argv=[sys.argv[0]])
