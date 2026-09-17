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

"""Pixel-area oracle for the straight-edged H fixtures, independent of SDFs.

The Java2D paths are frozen in em units with source-font hashes. Decomposing
non-zero-winding, axis-aligned outlines into disjoint rectangles makes each
pixel's covered area exact (apart from the final 8-bit image quantization).
No engine layout, distance generation, smoothing kernel or capture alignment
is used to construct the oracle.
"""

import hashlib
import json
import math
from functools import lru_cache
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageFilter, ImageStat
except ImportError:
    # make_report still emits a prerequisite failure report without Pillow.
    Image = ImageChops = ImageFilter = ImageStat = None

DATA = Path(__file__).parent / 'data/font_render'
# Explicit quality budget for small visual differences: half a percentage
# point in edge RMS coverage, one percent of stroke area, and 1/16 pixel
# in centroid. These are acceptance budgets, not numerical-error guarantees.
QUALITY_TOLERANCES = {'edge_rmse': .005, 'area_error': .01, 'centroid_error_pixels': 1 / 16}


def rectangles(commands):
    """Decompose an orthogonal non-zero-winding path without double-counting overlaps."""
    contours = []
    for command in commands:
        if command[0] == 'M':
            contours.append([])
        if command[0] in ('M', 'L'):
            contours[-1].append(tuple(command[1:]))
        elif command[0] != 'Z':
            raise ValueError('Coverage oracle requires straight, axis-aligned outlines')
    segments = [(a, b) for contour in contours for a, b in zip(contour, contour[1:] + contour[:1])]
    if any(a[0] != b[0] and a[1] != b[1] for a, b in segments):
        raise ValueError('Coverage oracle requires straight, axis-aligned outlines')
    xs = sorted({x for contour in contours for x, y in contour})
    ys = sorted({y for contour in contours for x, y in contour})
    result = []
    for left, right in zip(xs, xs[1:]):
        for top, bottom in zip(ys, ys[1:]):
            x, y = (left + right) / 2, (top + bottom) / 2
            winding = sum(1 if b[1] > a[1] else -1 for a, b in segments
                          if min(a[1], b[1]) <= y < max(a[1], b[1]) and a[0] > x)
            if winding:
                result.append((left, top, right, bottom))
    return result


def rasterize(rects, dimensions, samples_per_pixel):
    """Integrate disjoint rectangles over a one-screen-pixel square footprint."""
    width, height = dimensions
    half = samples_per_pixel / 2
    values = [0.0] * (width * height)
    for left, top, right, bottom in rects:
        columns = [(x, max(0, min(x + .5 + half, right) - max(x + .5 - half, left)) / samples_per_pixel)
                   for x in range(max(0, math.floor(left - half)), min(width, math.ceil(right + half)))]
        for y in range(max(0, math.floor(top - half)), min(height, math.ceil(bottom + half))):
            coverage_y = max(0, min(y + .5 + half, bottom) - max(y + .5 - half, top)) / samples_per_pixel
            for x, coverage_x in columns:
                values[y * width + x] += coverage_x * coverage_y
    image = Image.new('L', dimensions)
    image.putdata([round(min(1, value) * 255) for value in values])
    return image


def stable_case_name(case):
    scale = case['edge_scale'] * case['size'] / 32
    return case['source'] + '_single_edge_' + {.5: 'half', 1: 'one', 2: 'two'}[scale]


@lru_cache(maxsize=8)
def reference(source, effective_scale):
    fixture = json.loads((DATA / 'coverage_outlines.json').read_text())['fonts'][source]
    font = DATA.parent / fixture['file']
    if hashlib.sha256(font.read_bytes()).hexdigest() != fixture['sha256']:
        raise ValueError('Source font changed; regenerate and review the independent coverage outline')
    geometry = json.loads((DATA / 'capture_geometry.json').read_text())[source + '_edge_' + {.5: 'half', 1: 'one', 2: 'two'}[effective_scale]]
    factor = 32 * effective_scale * 8
    rects = [(geometry['origin_x'] + left * factor,
              geometry['origin_top'] + (fixture['ascent_em'] + top) * factor,
              geometry['origin_x'] + right * factor,
              geometry['origin_top'] + (fixture['ascent_em'] + bottom) * factor)
             for left, top, right, bottom in rectangles(fixture['commands'])]
    return rasterize(rects, (geometry['width'], geometry['height']), 8)


def measure(image, oracle):
    image = image.convert('L')
    if image.size != oracle.size:
        raise ValueError('Capture dimensions differ from the independent coverage fixture')
    # A fixed two-screen-pixel band on each side of the oracle's boundary.
    # Blank canvas never dilutes the edge error score.
    shape = oracle.point(lambda value: 255 if value >= 128 else 0)
    band = ImageChops.difference(shape.filter(ImageFilter.MaxFilter(33)), shape.filter(ImageFilter.MinFilter(33)))
    rms = ImageStat.Stat(ImageChops.difference(image, oracle), band).rms[0] / 255
    moments = []
    for source in (image, oracle):
        mass = mx = my = 0
        for i, alpha in enumerate(source.getdata()):
            mass += alpha
            mx += (i % source.width + .5) * alpha
            my += (i // source.width + .5) * alpha
        if not mass:
            raise ValueError('Empty coverage image')
        moments.append((mass, mx / mass, my / mass))
    a, b = moments
    return {'edge_rmse': rms, 'area_error': abs(a[0] - b[0]) / b[0],
            'centroid_error_pixels': math.hypot(a[1] - b[1], a[2] - b[2]) / 8}


def compare_quality(actual, stable, case, destination):
    oracle = reference(case['source'].split('_')[0], case['edge_scale'] * case['size'] / 32)
    oracle.save(destination / 'coverage_reference.png')
    current = measure(actual, oracle)
    previous = measure(stable, oracle)
    failures = []
    for key, tolerance in QUALITY_TOLERANCES.items():
        if current[key] > previous[key] + tolerance:
            failures.append(f'Coverage {key} {current[key]:.6f} exceeds 1.13.1 {previous[key]:.6f} + {tolerance:g}')
    return {'actual': current, 'stable': previous, 'tolerances': QUALITY_TOLERANCES,
            'status': 'fail' if failures else 'pass', 'oracle': 'Exact pixel-area integration of independent Java2D H outlines'}, failures
