import json
import subprocess
import sys
import unittest


class EngineSharedMainTest(unittest.TestCase):
    # Verifies the jar entry point loads JNI and emits the expected slice-9
    # geometry, guarding against packaging and native binding regressions.
    def test_slice9_fixture(self):
        result = subprocess.run(JAVA_COMMAND, check=True, capture_output=True, text=True)
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


if __name__ == "__main__":
    JAVA_COMMAND = sys.argv[1:]
    unittest.main(argv=[sys.argv[0]])
