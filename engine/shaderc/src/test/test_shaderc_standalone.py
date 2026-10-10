#!/usr/bin/env python3
"""Exercise the standalone compiler's exit status with real SPIR-V input."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class ShadercStandaloneTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def compile(self, source, output):
        path = self.root / 'test.frag'
        path.write_text(source)
        spirv = self.root / 'test.spv'
        subprocess.run([GLSLANG, '-V', '-S', 'frag', str(path), '-o', str(spirv)], check=True, capture_output=True)
        return self.run_shaderc(spirv, output)

    def run_shaderc(self, spirv, output):
        return subprocess.run([SHADERC, str(spirv), '--language', 'msl', '--version', '22',
                               '--stage', 'frag', '--out', str(output)], capture_output=True, text=True)

    # Successful cross-compilation must produce MSL and return success.
    def test_msl_output(self):
        output = self.root / 'test.msl'
        result = self.compile('#version 450\nlayout(location=0) out vec4 color;\nvoid main() { color = vec4(1); }\n', output)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('fragment ', output.read_text())

    # A failed MSL compilation must not report success or overwrite an existing shader.
    def test_compile_failure_preserves_output(self):
        output = self.root / 'test.msl'
        output.write_text('previous shader')
        result = self.compile('''#version 450
layout(location=0) out vec4 color;
layout(set=0,binding=0) uniform Constants { double value; } constants;
void main() { color = vec4(float(constants.value)); }
''', output)
        self.assertNotEqual(0, result.returncode)
        self.assertIn('double types are not supported', result.stderr)
        self.assertEqual('previous shader', output.read_text())

    # Missing input must return a normal failure instead of passing null data to the compiler.
    def test_missing_input(self):
        output = self.root / 'test.msl'
        result = self.run_shaderc(self.root / 'missing.spv', output)
        self.assertEqual(1, result.returncode)
        self.assertFalse(output.exists())

    # Failure to open the output file must propagate to scripts invoking the tool.
    def test_write_failure(self):
        result = self.compile('#version 450\nlayout(location=0) out vec4 color;\nvoid main() { color = vec4(1); }\n', self.root)
        self.assertEqual(1, result.returncode)
        self.assertIn('Failed to open', result.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shaderc', required=True)
    parser.add_argument('--glslang', required=True)
    args = parser.parse_args()
    SHADERC, GLSLANG = args.shaderc, args.glslang
    unittest.main(argv=[__file__])
