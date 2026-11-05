#!/usr/bin/env python3
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

    def render(self, text, size=48, frames=None, mode="vector", font=None):
        self.serial += 1
        output = pathlib.Path(self.directory.name) / f"{self.serial}.png"
        command = [str(VIEWER), "-t", text, "-sz", str(size), "-o", str(output)]
        if font is not None:
            command += ["-f", str(ROOT / "engine/font/src/test/data" / font)]
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

    # Export modes must retain spatial color/alpha and evaluate radial gradients per pixel.
    def test_spatial_gradient_exports(self):
        for mode in ("vector", "bitmap", "sdf"):
            for kind in ("linear", "radial"):
                with self.subTest(mode=mode, kind=kind):
                    text = f"<gradient type={kind} color0=#ff0000 color1=#0000ff>MMMM</gradient>"
                    image = self.render(text, size=80, mode=mode)[0]
                    pixels = list(image.getdata())
                    self.assertGreater(sum(r > b + 40 and r > g + 40 for r, g, b in pixels), 20)
                    self.assertGreater(sum(b > r + 40 and b > g + 40 for r, g, b in pixels), 20)
                    image = self.render(f"A<shadow color=#00000000><gradient type={kind} color0=#ffffff00 color1=#ffffff00>MMMM</gradient></shadow>", mode=mode)[0]
                    expected = self.render("A", mode=mode)[0]
                    self.assertEqual(expected.size, image.size)
                    self.assertIsNone(ImageChops.difference(expected, image).getbbox())
        for kind in ("linear", "radial"):
            with self.subTest(emoji_gradient=kind):
                image = self.render(f"A<gradient type={kind} color0=#ffffff00 color1=#ffffff00>😀</gradient>")[0]
                expected = self.render("A")[0]
                self.assertEqual(expected.size, image.size)
                self.assertIsNone(ImageChops.difference(expected, image).getbbox())

    # Known sRGB samples catch nonlinear interpolation/composition and lost alpha.
    def test_linear_light_colors(self):
        cases = [
            ("A", (137, 188, 225)),  # Black to white, at 25%, 50%, 75%.
            ("I", (67, 93, 112)),   # Gray 8 to 128, exercising palette decoding.
            ("D", (188, 188, 188)), # Half-transparent white over opaque black.
            ("F", (128, 128, 128)), # Solid palette values must round-trip.
            ("G", (8, 8, 8)),
        ]
        for text, expected in cases:
            with self.subTest(text=text):
                image = self.render(text, size=200, font="color-test.ttf")[0]
                x0, y0, x1, y1 = self.bounds(image)
                for fraction, value in zip((0.25, 0.5, 0.75), expected):
                    pixel = image.getpixel((int(x0 + fraction * (x1 - x0)), (y0 + y1) // 2))
                    for channel in pixel:
                        self.assertAlmostEqual(value, channel, delta=2)
        image = self.render("H", size=200, font="color-test.ttf")[0]
        x0, y0, x1, y1 = self.bounds(image)
        # Premultiplication prevents the transparent white stop from adding white.
        pixel = image.getpixel(((x0 + x1) // 2, (y0 + y1) // 2))
        for actual, background in zip(pixel, image.getpixel((0, 0))):
            self.assertAlmostEqual(background / 2, actual, delta=2)

    # Dynamic foreground conversion must preserve the requested text color.
    def test_foreground_color_round_trip(self):
        image = self.render("<color=#804020>E</color>", size=200, font="color-test.ttf")[0]
        x0, y0, x1, y1 = self.bounds(image)
        pixel = image.getpixel(((x0 + x1) // 2, (y0 + y1) // 2))
        for expected, actual in zip((128, 64, 32), pixel):
            self.assertAlmostEqual(expected, actual, delta=1)

    # PaintGlyph uses the referenced outline, ignoring its independent COLR clip box.
    def test_reused_outline_ignores_color_clip(self):
        clipped = self.render("B", size=200, font="color-test.ttf")[0]
        reused = self.render("C", size=200, font="color-test.ttf")[0]
        b = self.bounds(clipped)
        c = self.bounds(reused)
        self.assertAlmostEqual(2 * (b[2] - b[0]), c[2] - c[0], delta=2)
        self.assertEqual(b[3] - b[1], c[3] - c[1])

    # Composite fills sharing a clip apply its antialiased edge coverage once.
    def test_shared_composite_edge_coverage(self):
        for size in (19, 40):
            for reference, composite in (("J", "K"), ("J", "N"), ("P", "O")):
                with self.subTest(size=size, composite=composite):
                    expected = self.render(reference, size=size, font="color-test.ttf")[0]
                    actual = self.render(composite, size=size, font="color-test.ttf")[0]
                    self.assertEqual(expected.size, actual.size)
                    self.assertIsNone(ImageChops.difference(expected, actual).getbbox())

    # Independent clips and layer lists must not be merged just because their
    # outlines match; only composite operands inherit a shared clip's coverage.
    def test_independent_layer_edge_coverage(self):
        single = self.render("J", size=40, font="color-test.ttf")[0]
        separate = self.render("L", size=40, font="color-test.ttf")[0]
        layers = self.render("M", size=40, font="color-test.ttf")[0]
        self.assertEqual(single.size, separate.size)
        self.assertEqual(separate.size, layers.size)
        self.assertIsNotNone(ImageChops.difference(single, separate).getbbox())
        self.assertIsNone(ImageChops.difference(separate, layers).getbbox())

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

    # Fading a glyph must preserve its composed colors, including overlapping
    # translucent paints and mirrored paths, rather than expose lower layers.
    def test_opacity_preserves_composed_colors(self):
        text = "😀 🍎 🚀"
        normal = self.render(text, size=64)[0]
        faded = self.render("<color=#FFFFFF80>" + text + "</color>", size=64)[0]
        self.assertEqual(normal.size, faded.size)
        background = Image.new("RGB", normal.size, normal.getpixel((0, 0)))
        expected = Image.blend(background, normal, 128 / 255)
        difference = ImageChops.difference(expected, faded)
        self.assertLessEqual(max(high for low, high in difference.getextrema()), 2)

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

    # Each mode must draw ordinary text through its selected rendering path.
    def test_ordinary_text_in_each_mode(self):
        images = []
        for mode in ("sdf", "bitmap", "vector"):
            with self.subTest(mode=mode):
                # Small SDF text has too few near-white pixels on 1x framebuffers.
                image = self.render("Slug curves Aa B8", size=48, mode=mode)[0]
                images.append(image)
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
