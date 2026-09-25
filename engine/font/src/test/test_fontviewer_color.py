#!/usr/bin/env python3
# Copyright 2026 The Defold Foundation
# Licensed under the Defold License version 1.0; https://www.defold.com/license

"""Render fontviewer's rendering modes and COLR/Slug demo. Requires a desktop GL context and Pillow.

Usage: python3 src/test/test_fontviewer_color.py /path/to/fontviewer
"""

import argparse
import pathlib
import subprocess
import sys
import tempfile
import unittest

from PIL import Image, ImageChops

VIEWER = None
ROOT = pathlib.Path(__file__).resolve().parents[4]


class ColorEmojiTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if VIEWER is None:
            raise unittest.SkipTest("Pass a fontviewer executable to run desktop rendering checks")

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="fontviewer-color-")
        self.addCleanup(self.directory.cleanup)
        self.serial = 0

    def render(self, text, size=48, frames=None, mode="vector"):
        self.serial += 1
        output = pathlib.Path(self.directory.name) / f"{self.serial}.png"
        command = [str(VIEWER), "-t", text, "-sz", str(size), "-o", str(output)]
        if mode is not None:
            command += ["--mode", mode]
        if frames is not None:
            command += ["--frames", str(frames)]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        images = []
        for path in [output, *sorted(output.parent.glob(output.stem + "-*.png"))]:
            with Image.open(path) as image:
                images.append(image.convert("RGB"))
        return images

    def test_unsupported_color_glyph_keeps_surrounding_text(self):
        normal = self.render("Hello 😀 world")[0]
        for flag in ("🇸🇪", "🇺🇸"):
            with self.subTest(flag=flag):
                image = self.render("Hello " + flag + " 😀 world")[0]
                self.assertGreater(image.width, normal.width)
                self.assertGreater(sum(r > 190 and g > 140 and b < 90 for r, g, b in image.getdata()), 100)
                self.assertGreater(sum(r > 200 and g > 200 and b > 200 for r, g, b in image.getdata()), 300)

    def bounds(self, image):
        background = Image.new("RGB", image.size, image.getpixel((0, 0)))
        return ImageChops.difference(image, background).getbbox()

    def test_default_is_vector(self):
        default = self.render("Slug 😀 🚀", mode=None)[0]
        vector = self.render("Slug 😀 🚀", mode="vector")[0]
        self.assertEqual(default.size, vector.size)
        self.assertIsNone(ImageChops.difference(default, vector).getbbox())

    def test_solid_and_gradient_layers(self):
        image = self.render("😀 😎 🚀 ❤️ 👋 🎉 🌟 🌸 🔥 🍎")[0]
        pixels = list(image.getdata())
        # Missing glyphs, monochrome silhouettes and missing palette/gradient
        # textures each fail these independent color and shading checks.
        self.assertGreater(sum(r > 190 and g < 100 and b < 100 for r, g, b in pixels), 200)
        self.assertGreater(sum(r > 190 and g > 140 and b < 90 for r, g, b in pixels), 200)
        self.assertGreater(sum(b > r + 25 and b > 100 for r, g, b in pixels), 100)
        self.assertGreater(len(set(pixels)), 1000)
        # Ten separate emoji advances, including the variation-selector heart.
        self.assertGreater(image.width / image.height, 7)

    def test_palette_survives_text_color(self):
        normal = self.render("😀 🚀")[0]
        tinted = self.render("<color=#0000FF>😀 🚀</color>")[0]
        self.assertEqual(normal.size, tinted.size)
        self.assertIsNone(ImageChops.difference(normal, tinted).getbbox())

    def test_transparent_markup(self):
        normal = self.render("X")[0]
        transparent = self.render("X<color=#FFFFFF00>😀</color>")[0]
        self.assertEqual(normal.size, transparent.size)
        self.assertIsNone(ImageChops.difference(normal, transparent).getbbox())

    def test_rich_text_size(self):
        small = self.render("😀", 32)[0]
        large = self.render("<size=200%>😀</size>", 32)[0]
        for axis in (0, 1):
            a = self.bounds(small)
            b = self.bounds(large)
            ratio = (b[axis + 2] - b[axis]) / (a[axis + 2] - a[axis])
            self.assertAlmostEqual(2.0, ratio, delta=0.1)

    def test_zwj_and_skin_tone(self):
        family = self.render("👨‍👩‍👧‍👦")[0]
        thumb = self.render("👍🏽")[0]
        for image in (family, thumb):
            self.assertLess(image.width / image.height, 1.5)
        self.assertTrue(any(r > g > b and r - b > 50 for r, g, b in thumb.getdata()))

    def test_wave_updates_cached_geometry(self):
        images = self.render("<wave amplitude=8 hz=1 wavelength=3>😀 🚀 🎉</wave>", frames=4)
        self.assertGreater(len(images), 1)
        self.assertIsNotNone(ImageChops.difference(images[0], images[1]).getbbox())

    def test_raster_modes_use_monochrome_emojis(self):
        for mode in ("sdf", "bitmap"):
            with self.subTest(mode=mode):
                image = self.render("😀 🚀 ❤️", mode=mode)[0]
                self.assertGreater(sum(r > 150 for r, g, b in image.getdata()), 300)
                self.assertTrue(all(r == g == b for r, g, b in image.getdata()))
                tinted = self.render("<color=#FF0000>😀 🚀 ❤️</color>", mode=mode)[0]
                self.assertTrue(any(r > 150 and g < 100 and b < 100 for r, g, b in tinted.getdata()))

    def test_ordinary_text_in_each_mode(self):
        images = [self.render("Slug curves Aa B8", size=19, mode=mode)[0] for mode in ("sdf", "bitmap", "vector")]
        for image in images:
            self.assertGreater(sum(r > 200 for r, g, b in image.getdata()), 200)
        # The selected generation/shader must affect ordinary glyphs too.
        self.assertEqual(3, len({(image.size, image.tobytes()) for image in images}))

    def test_bitmap_span_outline_widths(self):
        normal = self.render("<outline color=#0000FF size=1>OO</outline>", mode="bitmap")[0]
        wide = self.render("<outline color=#0000FF size=4>OO</outline>", mode="bitmap")[0]
        mixed = self.render("<outline color=#0000FF size=1>O</outline><outline color=#0000FF size=4>O</outline>", mode="bitmap")[0]
        def blue_pixels(image):
            return sum(b > 150 and r < 100 and g < 100 for r, g, b in image.getdata())
        self.assertGreater(blue_pixels(normal), 20)
        self.assertGreater(blue_pixels(mixed), blue_pixels(normal))
        self.assertGreater(blue_pixels(wide), blue_pixels(mixed))

    def test_rich_text_demo(self):
        markup = (ROOT / "engine/font/src/test/data/markup.txt").read_text()
        for mode in ("sdf", "bitmap", "vector"):
            with self.subTest(mode=mode):
                images = self.render(markup, 20, frames=2, mode=mode)
                self.assertTrue(images)
                self.assertGreater(images[0].width, 500)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fontviewer", type=pathlib.Path)
    arguments, test_arguments = parser.parse_known_args()
    VIEWER = arguments.fontviewer.resolve()
    unittest.main(argv=[sys.argv[0], *test_arguments])
