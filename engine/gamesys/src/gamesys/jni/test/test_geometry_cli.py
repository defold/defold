import json
import subprocess
import sys
import unittest


class GeometryCliTest(unittest.TestCase):
    # Verifies the jar entry point loads JNI and emits the expected slice-9
    # geometry, guarding against packaging and native binding regressions.
    def test_slice9_fixture(self):
        args = ["GeometryJni", "slice9", "32.5", "24.25", "2", "3", "4", "5",
                "0.75", "0.25", "0", "0", "64", "64", "0", "0", "0", "1", "1", "1", "1", "0"]
        result = subprocess.run(JAVA_COMMAND + args, check=True, capture_output=True, text=True)
        data = json.loads(result.stdout)
        self.assertEqual(32, len(data["positions"]))
        self.assertEqual(32, len(data["uvs"]))
        self.assertEqual(54, len(data["indices"]))
        xs = [-24.375, -22.375, 4.125, 8.125]
        ys = [-6.0625, -1.0625, 15.1875, 18.1875]
        us = [0, 2 / 64, 1 - 4 / 64, 1]
        vs = [0, 5 / 64, 1 - 3 / 64, 1]
        for i in range(16):
            self.assertAlmostEqual(xs[i % 4], data["positions"][i * 2] * 32.5, places=5)
            self.assertAlmostEqual(ys[i // 4], data["positions"][i * 2 + 1] * 24.25, places=5)
            self.assertAlmostEqual(us[i % 4], data["uvs"][i * 2], places=6)
            self.assertAlmostEqual(vs[i // 4], data["uvs"][i * 2 + 1], places=6)
        self.assertEqual([0, 1, 5, 0, 5, 4], data["indices"][:6])
        self.assertEqual([10, 11, 15, 10, 15, 14], data["indices"][-6:])

    # Verifies alternate CLI parameters reach JNI, and direct module invocation
    # produces the same result as the jar's module dispatcher.
    def test_module_entry_point(self):
        args = ["slice9", "80", "40", "4", "6", "8", "10", "0.5", "0.5",
                "1", "1", "128", "64", "0", "0", "0", "1", "1", "1", "1", "0"]
        jar_index = JAVA_COMMAND.index("-jar")
        direct_command = (JAVA_COMMAND[:jar_index] + ["-cp", JAVA_COMMAND[jar_index + 1],
                                                    "com.defold.engine.GeometryJni"])
        output = subprocess.run(JAVA_COMMAND + ["GeometryJni"] + args,
                                check=True, capture_output=True, text=True)
        direct = subprocess.run(direct_command + args, check=True, capture_output=True, text=True)
        self.assertEqual(output.stdout, direct.stdout)
        data = json.loads(output.stdout)
        xs = [-40, -32, 36, 40]
        ys = [-20, -14, 10, 20]
        us = [1, 1 - 8 / 128, 4 / 128, 0]
        vs = [1, 1 - 6 / 64, 10 / 64, 0]
        for i in range(16):
            self.assertAlmostEqual(xs[i % 4], data["positions"][i * 2] * 80, places=5)
            self.assertAlmostEqual(ys[i // 4], data["positions"][i * 2 + 1] * 40, places=5)
            self.assertAlmostEqual(us[i % 4], data["uvs"][i * 2], places=6)
            self.assertAlmostEqual(vs[i // 4], data["uvs"][i * 2 + 1], places=6)

    # Verifies the geometry command exposes usage and rejects unknown or
    # incomplete commands instead of silently emitting a fixture.
    def test_command_usage(self):
        for args in [["GeometryJni"], ["GeometryJni", "--help"]]:
            with self.subTest(args=args):
                result = subprocess.run(JAVA_COMMAND + args, capture_output=True, text=True)
                self.assertEqual(0, result.returncode)
                self.assertIn("Usage:", result.stdout)
        for args in [["GeometryJni", "unknown"], ["GeometryJni", "slice9"]]:
            with self.subTest(args=args):
                result = subprocess.run(JAVA_COMMAND + args, capture_output=True, text=True)
                self.assertNotEqual(0, result.returncode)


if __name__ == "__main__":
    JAVA_COMMAND = sys.argv[1:]
    unittest.main(argv=[sys.argv[0]])
