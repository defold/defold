#!/usr/bin/env python3
"""Test safe shader publication and Bob's shared WGSL transformation."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import generate_graphics_shaders as shaders


class GraphicsShadersTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def transform(self, source):
        path = self.root / 'vertex.wgsl'
        path.write_text(source)
        shaders.add_wgsl_flipped_entry_point(path, JAVA)
        return path.read_text()

    # Compile every variant with host tools so source edits cannot silently leave stale fixtures.
    def test_checked_in_variants(self):
        shaders.generate(GLSLANG, SHADERC, TINT, shaders.SOURCE_DIR, JAVA, check=True)

    # Preserve the backbuffer entry point and flip only the offscreen variant.
    def test_wgsl_flipped_entry_point(self):
        entry = '''@vertex
fn main(@location(0) position : vec3f) -> main_out {
  gl_Position = vec4f(position, 1.0f);
  return main_out(gl_Position);
}'''
        source = 'var<private> gl_Position : vec4f;\n' + entry + '\n// end\n'
        generated = self.transform(source)
        self.assertIn(entry, generated)
        self.assertEqual(2, generated.count('@vertex'))
        self.assertEqual(1, generated.count('gl_Position.y = -gl_Position.y;'))
        self.assertIn('// defold-webgpu-flipped-entry-point: _defold_webgpu_main_flipped', generated)
        self.assertIn('gl_Position.y = -gl_Position.y;\n  return main_out(gl_Position);', generated)
        self.assertTrue(generated.endswith('// end\n'))

    # Use Bob's collision handling instead of emitting a duplicate WGSL function name.
    def test_wgsl_entry_point_name_collision(self):
        source = '''fn _defold_webgpu_main_flipped() {}
@vertex fn main() -> vec4f {
  return gl_Position;
}
'''
        generated = self.transform(source)
        self.assertIn('// defold-webgpu-flipped-entry-point: _defold_webgpu_main_flipped_\n', generated)
        self.assertEqual(1, generated.count('fn _defold_webgpu_main_flipped('))
        self.assertEqual(1, generated.count('fn _defold_webgpu_main_flipped_('))

    # Reject changed Tint entry-point shapes without modifying the input file.
    def test_unrecognized_wgsl_entry_point(self):
        for source in ('@fragment fn main() {}', '@vertex fn main() {', '@vertex fn main() { return position; }'):
            with self.subTest(source=source), self.assertRaises(subprocess.CalledProcessError):
                self.transform(source)
            self.assertEqual(source, (self.root / 'vertex.wgsl').read_text())

    # An old compiler returning success with empty output must not replace checked-in shaders.
    def test_empty_compiler_output_preserves_variants(self):
        previous = self.root / 'graphics_capture.vp.msl'
        previous.write_text('previous shader')

        def run(command, **kwargs):
            if command[0] == 'shaderc':
                Path(command[command.index('--out') + 1]).write_text('')

        with mock.patch.object(shaders.subprocess, 'run', side_effect=run):
            with self.assertRaisesRegex(ValueError, 'empty shader'):
                shaders.generate('glslang', 'shaderc', 'tint', self.root)
        self.assertEqual('previous shader', previous.read_text())
        self.assertEqual([previous], list(self.root.iterdir()))

    # A later compiler failure must leave all old variants intact, including earlier successful stages.
    def test_failed_compiler_preserves_variants(self):
        previous = self.root / 'graphics_capture.vp.msl'
        previous.write_text('previous shader')

        def run(command, **kwargs):
            if command[0] == 'shaderc':
                if command[command.index('--language') + 1] == 'glsl':
                    raise subprocess.CalledProcessError(1, command)
                Path(command[command.index('--out') + 1]).write_text('new shader')

        with mock.patch.object(shaders.subprocess, 'run', side_effect=run):
            with self.assertRaises(subprocess.CalledProcessError):
                shaders.generate('glslang', 'shaderc', 'tint', self.root)
        self.assertEqual('previous shader', previous.read_text())
        self.assertEqual([previous], list(self.root.iterdir()))

    # Check mode must report stale and missing variants without updating either one.
    def test_check_does_not_write_stale_or_missing_variants(self):
        old = self.root / 'old.fp'
        old.write_text('previous shader')
        expected = {'old.fp': 'new shader', 'missing.fp': 'new shader'}
        with self.assertRaisesRegex(ValueError, 'old.fp, missing.fp'):
            shaders.write_shaders(expected, self.root, check=True)
        self.assertEqual('previous shader', old.read_text())
        self.assertFalse((self.root / 'missing.fp').exists())

    # Check mode accepts matching content without rewriting files.
    def test_check_accepts_current_variants(self):
        path = self.root / 'current.fp'
        path.write_text('current shader')
        with mock.patch.object(Path, 'write_text', side_effect=AssertionError('Check mode wrote a file')):
            shaders.write_shaders({'current.fp': 'current shader'}, self.root, check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--glslang', required=True)
    parser.add_argument('--shaderc', required=True)
    parser.add_argument('--tint', required=True)
    parser.add_argument('--java', default='java')
    args = parser.parse_args()
    GLSLANG, SHADERC, TINT, JAVA = args.glslang, args.shaderc, args.tint, args.java
    unittest.main(argv=[__file__])
