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

"""PNG likeness using Pillow: 100 * (1 - normalized RGB RMSE).

Install with: ./scripts/build.py install_ext, then ./scripts/build.py shell
The foreground metric excludes clear pixels without rescaling or aligning images.
"""
import argparse
import math
from pathlib import Path
import sys
import tempfile

try:
    from PIL import Image, ImageChops, ImageStat
except ImportError:
    Image = ImageChops = ImageStat = None


def foreground_mask(image, background):
    channels = ImageChops.difference(image, Image.new("RGB", image.size, background)).split()
    maximum = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
    return maximum.point(lambda value: 255 if value > 1 else 0)


def _likeness(squared_error, samples):
    normalized = math.sqrt(squared_error / samples) / 255.0
    return normalized, 100.0 * (1.0 - normalized)


def compare(actual, reference, difference_path, background=(0, 0, 0), region=None, foreground=True):
    """Compare equal-sized RGB images and write a 4x absolute difference PNG.

    Inputs must already be composited onto their background. Foreground RMSE
    uses the union in region, so extra pixels and missing effects count fully;
    whole-image RMSE is reported separately. No reference files are modified.
    Set foreground=False for whole-image tests, including uniform clears.
    """
    if actual.size != reference.size:
        raise ValueError("Image dimensions must match")
    region = region or (0, 0, actual.width, actual.height)
    difference = ImageChops.difference(actual, reference)
    difference.point(lambda value: min(255, value * 4)).save(difference_path)
    _, whole = _likeness(sum(ImageStat.Stat(difference).sum2), actual.width * actual.height * 3)
    if not foreground:
        return dict(normalized_difference=1.0 - whole / 100.0, likeness_percent=whole,
                    whole_likeness_percent=whole, foreground_pixels=None)
    a, b = actual.crop(region), reference.crop(region)
    mask = ImageChops.lighter(foreground_mask(a, background), foreground_mask(b, background))
    count = mask.histogram()[255]
    if not count:
        raise ValueError("No foreground pixels in comparison region")
    error = sum(ImageStat.Stat(difference.crop(region), mask).sum2)
    normalized, likeness = _likeness(error, count * 3)
    return dict(normalized_difference=normalized, likeness_percent=likeness,
                whole_likeness_percent=whole, foreground_pixels=count)


def check_tools():
    """Verify PNG comparison and lossless WebP reports without a graphics device."""
    if Image is None:
        raise ValueError('Pillow is required for image tests. Run ./scripts/build.py install_ext and ./scripts/build.py shell (interpreter: %s)' % sys.executable)
    import PIL
    with tempfile.TemporaryDirectory(prefix="defold-likeness-check-") as temporary:
        a, b, diff = [Path(temporary) / name for name in ("a.png", "b.png", "difference.png")]
        Image.new("RGB", (2, 2), "white").save(a)
        Image.new("RGB", (2, 2), "black").save(b)
        with Image.open(a) as actual, Image.open(b) as reference:
            if compare(actual, actual, diff)['likeness_percent'] != 100:
                raise ValueError("Identical-image comparison failed")
            if compare(actual, reference, diff)['likeness_percent'] != 0:
                raise ValueError("Different-image comparison failed")
        with Image.open(diff) as difference:
            if difference.convert('RGB').getpixel((0, 0)) != (255, 255, 255):
                raise ValueError("Difference PNG round-trip failed")
        # HTML reports use lossless WebP, including RGB values of transparent pixels.
        original = Image.new("RGBA", (2, 2))
        original.putdata([(17, 31, 63, 0), (4, 128, 255, 127), (255, 0, 1, 255), (0, 0, 0, 255)])
        webp = Path(temporary) / "probe.webp"
        try:
            original.save(webp, format="WEBP", lossless=True, exact=True)
            with Image.open(webp) as decoded:
                if decoded.convert("RGBA").tobytes() != original.tobytes():
                    raise ValueError("Lossless WebP round-trip changed pixels")
        except (OSError, KeyError) as error:
            raise ValueError("Pillow with lossless WebP support is required for HTML reports. Run ./scripts/build.py install_ext") from error
    print("Pillow %s: %s (PNG read/write, RGB RMSE, difference and lossless WebP probes passed)" % (PIL.__version__, PIL.__file__))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify Pillow PNG and comparison support")
    args = parser.parse_args()
    if not args.check:
        parser.error("Use --check; rendering reports call compare() directly")
    try:
        check_tools()
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + "\n")
