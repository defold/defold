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

"""Compare font captures and build a portable render-test gallery.

This module only reads reference images. Candidate generation and acceptance are
separate operations so a failed run cannot silently become its own reference.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import html
import io
import json
import math
import re
import shutil
import sys
from collections import defaultdict
from functools import lru_cache
from pathlib import Path
from urllib.parse import quote
from collections import Counter
import shlex
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "scripts"))
import likeness
import font_coverage
FONT_ROOT = Path(__file__).resolve().parents[2]

try:
    from PIL import Image, ImageChops
except ImportError:
    # A failed prerequisite check still needs its HTML/JSON failure report.
    Image = ImageChops = None


DATA = Path(__file__).resolve().parent / 'data/font_render'
TEXT = 'ABCDEFGabcdefg 0123456789'
SOURCES = ('ttf_sdf', 'otf_sdf', 'ttf_bitmap', 'otf_bitmap', 'ttf_sdf_bank', 'otf_sdf_bank', 'ttf_bitmap_bank', 'otf_bitmap_bank', 'fnt')
VECTOR_SOURCES = ('ttf_vector', 'otf_vector', 'ttf_vector_bank', 'otf_vector_bank')

def cases(full, rich):
    result = []
    def add(source, multi, scenario='default', text=TEXT, **options):
        case = dict(source=source, multi=multi, scenario=scenario, text=text, size=40, outline=4, outline_alpha=1,
                    threshold=98, face_alpha=1, shadow_alpha=0, shadow_blur=0, shadow_x=0, shadow_y=0, markup=False, change=False)
        case.update(options)
        case['id'] = f'{source}_{"multi" if multi else "single"}_{scenario}'
        result.append(case)
    for source in SOURCES:
        for multi in (False, True): add(source, multi)
    for source in ('ttf_sdf', 'otf_sdf'):
        for name, scale in (('half', .5), ('one', 1), ('two', 2)):
            add(source, False, 'edge_' + name, text='H', size=32, outline=0, outline_alpha=0, edge_scale=scale)
        add(source, False, 'solid_half', text='H', size=128, outline=0, outline_alpha=0, edge_scale=.5, opaque_interior=True)
        for width in (.5, 1.5):
            add(source, False, 'spread_' + str(width).replace('.', '_'), text='H', size=32, outline=width, outline_alpha=0, edge_scale=1)
    effects = dict(face_half=dict(face_alpha=.5), face_zero=dict(face_alpha=0), face_only=dict(outline_alpha=0),
                   width_zero=dict(outline=0), width_two=dict(outline=2), width_eight=dict(outline=8),
                   outline_half=dict(outline_alpha=.5), shadow=dict(shadow_alpha=1,shadow_x=6,shadow_y=-6),
                   shadow_half=dict(shadow_alpha=.5,shadow_x=6,shadow_y=-6),
                   shadow_blur=dict(shadow_alpha=1,shadow_blur=4,shadow_x=6,shadow_y=-6),
                   shadow_negative=dict(shadow_alpha=1,shadow_x=-6,shadow_y=6), text_change=dict(change=True), named_style=dict())
    for source in ('ttf_sdf','ttf_bitmap'):
        for multi in (False, True):
            for name, options in effects.items(): add(source,multi,name,**options)
            if rich:
                for name, text in dict(inline_width_zero=f'<outline size=0>{TEXT}</outline>',
                    inline_alpha_zero=f'<outline alpha=0>{TEXT}</outline>',inline_alpha_half=f'<outline alpha=0.5>{TEXT}</outline>',
                    inline_color=f'<color=#ff8080>{TEXT}</color>',
                    nested=f'AB<outline size=0>CD<outline size=2>EF</outline>ab</outline>cdefG 0123456789',
                    inline_shadow=f'<shadow x=6 y=-6 blur=4>{TEXT}</shadow>',
                    mixed=f'ABCD<color=#ff8080>EFGab</color>cdefG 0123456789').items():
                    add(source,multi,name,text=text,markup=True)
    # Vector always uses separate face/effect quads. Exercise runtime sources and
    # the exact prebaked curve payload exported by Fontc, including magnification.
    for source in VECTOR_SOURCES:
        add(source, True, 'face_only', outline_alpha=0, outline=0)
        add(source, True, 'effects', shadow_alpha=1, shadow_blur=2, shadow_x=6, shadow_y=-6)
        add(source, True, 'scaled', text='Example', size=80, outline_alpha=0, outline=0)
        add(source, True, 'depth_overlap', text='Depth', size=80, outline_alpha=0, outline=0)
        if rich:
            add(source, True, 'rich_style', text='<color=#ff8080><size=150%>AB</size></color>CD', markup=True, outline_alpha=0, outline=0)
            add(source, True, 'decorations', text='<ul>HH HH</ul> <strike>HH HH</strike>', markup=True, outline_alpha=0, outline=0)
            add(source, True, 'decorations_dashed', text='<ul pattern=dashed>HH HH</ul> <strike pattern=dashed>HH HH</strike>', markup=True, outline_alpha=0, outline=0)
            for name, size in (('half', 20), ('double', 80)):
                add(source, True, 'decorations_outline_' + name, text='<outline size=8><ul>H H</ul> <strike>H H</strike></outline>', markup=True, size=size, outline=2)
    if full:
        lorem=json.loads((DATA/'lorem.json').read_text(encoding="utf-8"))
        for language in ('english','arabic'):
            for multi in (False,True):add('arabic' if language=='arabic' else 'latin',multi,language,text=lorem[language])
    return result


def generate_cases(output):
    """Emit the C++ cases used by all four native test configurations."""
    geometry = json.loads((DATA / "capture_geometry.json").read_text(encoding="utf-8"))
    lines=[]
    for name, values in geometry.items():
        fields = ", ".join(str(values[key]) for key in ("width", "height", "origin_x", "origin_top", "layout_width"))
        lines.append("const FontImageCaptureGeometry g_Capture_" + name + " = { " + fields + " };")
    for full,rich in ((False,False),(False,True),(True,False),(True,True)):
        condition=f'{"defined" if full else "!defined"}(FONT_USE_SKRIBIDI) && {"!defined" if rich else "defined"}(FONT_IMAGE_RICH_NULL)'
        lines.append('#if '+condition)
        lines.append('static const char* g_ImageCaseNames[] = { '+', '.join(json.dumps(c['id']) for c in cases(full,rich))+' };')
        for c in cases(full,rich):
            values=[json.dumps(c[k],ensure_ascii=False) for k in ('id','source','text')]
            values += [str(float(c[k]))+'f' for k in ('size','outline','outline_alpha','face_alpha','shadow_alpha','shadow_blur','shadow_x','shadow_y')]
            values += [str(c[k]).lower() for k in ('multi','markup','change')]
            values += [str(float(c.get('edge_scale', 0)))+'f']
            lines.append('TEST(FontImages_'+c['id']+', Render)\n{\n    const FontImageCase c = { '+', '.join(values)+' };\n    TestFontImage(c);\n}\n')
        lines.append('#endif')
    output.parent.mkdir(parents=True,exist_ok=True); output.write_text('\n'.join(lines)+'\n', encoding="utf-8")


def _json_value(value):
    if isinstance(value, dict):
        return {str(key): _json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    return str(value)


def _copy_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() != destination.resolve():
        shutil.copyfile(source, destination)


def _rgb(image: Image.Image, background: tuple[int, int, int]) -> Image.Image:
    # Compare visible colors, including alpha, against the declared clear color.
    rgba = image.convert("RGBA")
    canvas = Image.new("RGBA", rgba.size, (*background, 255))
    return Image.alpha_composite(canvas, rgba).convert("RGB")


def _effect_pixels(image: Image.Image, background: tuple[int, int, int]) -> dict[str, int]:
    red, green, blue = image.split()
    counts = {}
    # The fixtures reserve blue for outline and green for shadow. Requiring a
    # dominant channel excludes white faces and neutral clear pixels. Pillow
    # performs these operations in native code for large screenshot matrices.
    for effect, channel, other_a, other_b, clear in (("outline", blue, red, green, background[2]), ("shadow", green, red, blue, background[1])):
        other = ImageChops.lighter(ImageChops.lighter(other_a, other_b), Image.new("L", image.size, clear))
        dominance = ImageChops.subtract(channel, other)
        counts[effect] = sum(dominance.histogram()[9:])
    return counts


def _background(case: dict) -> tuple[int, int, int]:
    background = tuple(case.get("background", (32, 32, 32)))
    if len(background) != 3 or any(type(channel) is not int or not 0 <= channel <= 255 for channel in background):
        raise ValueError("Background must contain three byte-valued RGB channels")
    return background


def _roi_box(image: Image.Image, case: dict) -> tuple[int, int, int, int]:
    roi = case.get("roi", (0, 0, *image.size))
    if len(roi) != 4 or any(type(value) is not int for value in roi):
        raise ValueError("ROI must contain integer x, y, width, height")
    x, y, width, height = roi
    if x < 0 or y < 0 or width <= 0 or height <= 0 or x + width > image.width or y + height > image.height:
        raise ValueError(f"ROI {roi} is outside image dimensions {image.size}")
    return x, y, x + width, y + height


def _effect_failures(effect_pixels: dict, case: dict) -> list[str]:
    failures = []
    for effect in ("outline", "shadow"):
        expectation = case.get(f"expect_{effect}")
        if expectation is not None and type(expectation) is not bool:
            raise ValueError(f"expect_{effect} must be a boolean or null")
        if expectation is None:
            continue
        for source, counts in effect_pixels.items():
            count = counts[effect]
            if expectation and not count:
                failures.append(f"Expected visible {effect} in {source}, found no reserved-color pixels")
            elif not expectation and count:
                failures.append(f"Expected no {effect} in {source}, found {count} reserved-color pixels")
    return failures


def validate_capture(actual_path: Path | str, case: dict) -> dict:
    """Check one candidate's visible content without inventing a reference.

    Return measurement data for the candidate report, or raise ValueError for a
    malformed/blank capture or incorrect effect visibility. This does not judge
    likeness or accept the candidate; those still require reference review.
    """
    if Image is None:
        raise ValueError("PNG validation requires Pillow; run the render-test prerequisite checks")
    try:
        actual_path = Path(actual_path)
        if not actual_path.is_file():
            raise ValueError(f"Missing actual image: {actual_path}")
        background = _background(case)
        with Image.open(actual_path) as source:
            if source.format != "PNG":
                raise ValueError("Captures must be PNG images")
            actual = _rgb(source, background)
        box = _roi_box(actual, case)
        actual_roi = actual.crop(box)
        foreground_pixels = likeness.foreground_mask(actual_roi, background).histogram()[255]
        if not foreground_pixels:
            raise ValueError("No foreground pixels in capture region")
        effect_pixels = {"actual": _effect_pixels(actual_roi, background)}
        failures = _effect_failures(effect_pixels, case)
        if failures:
            raise ValueError("; ".join(failures))
        return {
            "dimensions": list(actual.size),
            "foreground_pixels": foreground_pixels,
            "effect_pixels": effect_pixels,
            "roi": [box[0], box[1], box[2] - box[0], box[3] - box[1]],
        }
    except (OSError, TypeError, OverflowError, Image.DecompressionBombError) as error:
        raise ValueError(str(error)) from error


def sdf_edge_width(image, upper_alpha=.5):
    """Measure the first H stem's 10%-50% transition in screen pixels.

    The capture samples at 8x screen resolution using the real SDF shader.
    The current filter uses a two-pixel full smoothstep band;
    its 10%-50% span is 0.6084 pixels. This expectation is independent of
    generated references, the distance encoding and the smoothing formula.
    Using the outer half avoids requiring thin, downscaled stems to reach 90%.
    """
    rgb = image.convert('RGB')
    bounds = likeness.foreground_mask(rgb, (0, 0, 0)).getbbox()
    if bounds is None:
        raise ValueError('Empty SDF edge capture')
    _, top, _, bottom = bounds
    widths = []
    for fraction in (.25, .30, .35):
        y = top + int((bottom - top) * fraction)
        row = [rgb.getpixel((x, y))[0] / 255 for x in range(rgb.width)]
        crossings = []
        for alpha in (.1, upper_alpha):
            crossing = next((x + (alpha - a) / (b - a)
                             for x, (a, b) in enumerate(zip(row, row[1:]))
                             if a < alpha <= b), None)
            if crossing is None:
                raise ValueError(f'SDF stem does not reach {alpha:.0%} opacity')
            crossings.append(crossing)
        widths.append((crossings[1] - crossings[0]) / 8)
    return sum(widths) / len(widths)


def compare_case(actual_path: Path | str, expected_path: Path | str, case: dict, report_dir: Path | str) -> dict:
    """Return one result, retaining available artifacts even when comparison fails.

    ``report_dir`` is this case's artifact directory. Artifact paths in the
    result are relative to it; ``artifact_root`` lets build_report copy a run
    into another directory without referring to the source-controlled images.
    """
    destination = Path(report_dir).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    result = {
        "id": str(case.get("id", "unnamed")),
        "description": str(case.get("description", "")),
        "status": "error",
        "reason": "",
        "threshold": case.get("threshold", 98.0),
        "likeness_percent": None,
        "whole_likeness_percent": None,
        "normalized_difference": None,
        "metric": "foreground union RGB RMSE",
        "difference_amplification": 4,
        "paths": {},
        "artifact_root": str(destination),
        "configuration": case.get("configuration", case),
        "logs": case.get("logs", ""),
    }
    if Image is None:
        result["reason"] = "PNG comparison requires Pillow; run the render-test prerequisite checks"
        return _json_value(result)
    try:
        actual_path, expected_path = Path(actual_path), Path(expected_path)
        edge_failures = []
        missing = []
        for key, source, name in (("actual", actual_path, "actual.png"), ("reference", expected_path, "reference.png")):
            if not source.is_file():
                missing.append(f"Missing {key} image: {source}")
            else:
                _copy_file(source, destination / name)
                result["paths"][key] = name
        if case.get('edge_scale') and actual_path.is_file():
            # A matching baseline with the same broken edge must never pass.
            with Image.open(actual_path) as actual:
                if actual.format != "PNG":
                    raise ValueError("Captures must be PNG images")
                result["dimensions"] = list(actual.size)
                width = sdf_edge_width(actual)
            result['edge_width_pixels'] = width
            result['expected_edge_width_pixels'] = 0.6084
            result['edge_width_tolerance_pixels'] = 0.08
            result['reference_version'] = 'native'
            result['display_zoom'] = 4
            if abs(width - 0.6084) > .08:
                edge_failures.append(f'SDF edge width {width:.4f} px; expected 0.6084 ± 0.08 px (current smoothing contract)')
            if case.get('opaque_interior'):
                with Image.open(actual_path) as actual:
                    peak = actual.convert('L').getextrema()[1] / 255.0
                    result['interior_opacity'] = peak
                    if peak < .99:
                        edge_failures.append(f'SDF interior opacity {peak:.2%}; expected at least 99%')
                    if peak >= .9:
                        full_width = sdf_edge_width(actual, .9)
                        result['full_edge_width_pixels'] = full_width
                        if abs(full_width - 1.2168) > .1:
                            edge_failures.append(f'SDF 10%-90% edge width {full_width:.4f} px; expected 1.2168 ± 0.1 px')
        if case.get('stable_reference') and actual_path.is_file():
            stable_path = Path(case['stable_reference'])
            _copy_file(stable_path, destination / 'stable_reference.png')
            result['paths']['stable_reference'] = 'stable_reference.png'
            background = _background(case)
            with Image.open(actual_path) as source_actual, Image.open(stable_path) as source_stable:
                actual = _rgb(source_actual, background)
                stable = _rgb(source_stable, background)
            box = _roi_box(actual, case)
            if stable.size != actual.size:
                raise ValueError('1.13.1 comparison dimensions differ from the capture fixture')
            stable_metrics = likeness.compare(actual, stable, destination / 'stable_difference.png', background, box)
            result['stable_likeness_percent'] = stable_metrics['likeness_percent']
            result['paths']['stable_difference'] = 'stable_difference.png'
            if case.get('edge_scale'):
                quality, quality_failures = font_coverage.compare_quality(actual, stable, case, destination)
                result['coverage_quality'] = quality
                edge_failures.extend(quality_failures)
                result['paths']['coverage_reference'] = 'coverage_reference.png'
            result['reference_origin_note'] = (
                '1.13.1 fixture origin adjusted by -1.5 font px horizontally and +0.5 font px vertically '
                '(Y up), compensating legacy padding and corner sampling. No image registration or resampling.')

        if missing:
            if actual_path.is_file() and not expected_path.is_file():
                # Missing baselines skip comparison, but must not hide a bad
                # capture. Verify the existing PNG before marking it skipped.
                with Image.open(actual_path) as actual:
                    if actual.format != "PNG":
                        raise ValueError("Captures must be PNG images")
                    actual.verify()
                result["status"] = "fail" if edge_failures else "skipped"
                result["reason"] = "; ".join(edge_failures + missing)
                return _json_value(result)
            raise ValueError("; ".join(missing))

        threshold = float(case.get("threshold", 98.0))
        if not math.isfinite(threshold) or not 0.0 <= threshold <= 100.0:
            raise ValueError("Likeness threshold must be finite and between 0 and 100")
        result["threshold"] = threshold
        background = _background(case)

        with Image.open(actual_path) as source_actual, Image.open(expected_path) as source_expected:
            if source_actual.format != "PNG" or source_expected.format != "PNG":
                raise ValueError("Captures and references must be PNG images")
            actual = _rgb(source_actual, background)
            expected = _rgb(source_expected, background)
        result["dimensions"] = list(actual.size)
        result["reference_dimensions"] = list(expected.size)
        actual_size, expected_size = actual.size, expected.size
        dimensions_differ = actual_size != expected_size
        if dimensions_differ:
            # Compare unequal viewports on a common canvas. Pad at the
            # bottom/right; never resize or align pixels
            # to improve the score or hide a layout/viewport change.
            size = (max(actual.width, expected.width), max(actual.height, expected.height))
            actual_canvas = Image.new("RGB", size, background)
            expected_canvas = Image.new("RGB", size, background)
            actual_canvas.paste(actual, (0, 0))
            expected_canvas.paste(expected, (0, 0))
            actual, expected = actual_canvas, expected_canvas
            result["difference_note"] = "Padded to common dimensions at the top-left; no scaling or alignment"
        box = _roi_box(actual, case)
        actual_roi, expected_roi = actual.crop(box), expected.crop(box)
        result.update(likeness.compare(actual, expected, destination / "difference.png", background, box))
        result["paths"]["difference"] = "difference.png"


        result["effect_pixels"] = {
            "actual": _effect_pixels(actual_roi, background),
            "reference": _effect_pixels(expected_roi, background),
        }
        failures = list(edge_failures)
        if case.get('edge_scale'):
            # Check the contour origin separately. Never move image pixels to
            # maximize likeness: that would hide a broken capture fixture.
            bounds = [image.convert('L').point(lambda value: 255 if value >= 128 else 0).getbbox()
                      for image in (actual, expected)]
            if any(bound is None for bound in bounds):
                raise ValueError('SDF contour does not reach 50% opacity')
            a, b = bounds
            offset = [(a[axis] + a[axis + 2] - b[axis] - b[axis + 2]) / 2 for axis in (0, 1)]
            result['contour_offset_capture_pixels'] = offset
            result['contour_offset_tolerance_capture_pixels'] = 1.0
            if max(abs(value) for value in offset) > 1.0:
                failures.append(f'SDF contour origin differs by ({offset[0]:g}, {offset[1]:g}) capture px; expected within 1 px')
        if dimensions_differ:
            failures.append(f"Image dimensions differ: actual {actual_size}, reference {expected_size}")
        if result["likeness_percent"] < threshold:
            failures.append(f"Foreground likeness {result['likeness_percent']:.4f}% is below {threshold:g}%")
        failures.extend(_effect_failures(result["effect_pixels"], case))
        result["status"] = "fail" if failures else "pass"
        result["reason"] = "; ".join(failures) or "Image and effect visibility checks passed"
    except (OSError, ValueError, TypeError, OverflowError, Image.DecompressionBombError) as error:
        result["reason"] = str(error)
    return _json_value(result)


def _case_directory(case_id: str, index: int) -> str:
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "-", case_id).strip(".-")[:100] or "case"
    digest = hashlib.sha256(case_id.encode("utf8")).hexdigest()[:10]
    return f"{index:04d}-{slug}-{digest}"


def _escape(value) -> str:
    return html.escape(str(value), quote=True)


def _data_url(contents: bytes, mime: str) -> str:
    return f"data:{mime};base64," + base64.b64encode(contents).decode("ascii")


def _download_html(label: str, path: Path, mime: str = "application/octet-stream") -> str:
    return f'<a download="{_escape(path.name)}" href="{_data_url(path.read_bytes(), mime)}">{_escape(label)}</a>'


def _case_anchor(result: dict) -> str:
    return "case-" + hashlib.sha256(result["id"].encode("utf8")).hexdigest()[:16]


@lru_cache(maxsize=128)
def _webp_data_url(png_bytes: bytes) -> str:
    # Cache by contents so repeated captures across case/configuration pages
    # share conversion work. Keep transparent RGB values intact as well.
    with Image.open(io.BytesIO(png_bytes)) as image:
        output = io.BytesIO()
        image.save(output, format="WEBP", lossless=True, method=4, exact=True)
    return _data_url(output.getvalue(), "image/webp")


def _image_html(label: str, path: str | None, root: Path | None = None, zoom: int = 1, width: int | None = None) -> str:
    if not path:
        return f'<figure><div class="missing">No {_escape(label.lower())} image</div><figcaption>{_escape(label)}</figcaption></figure>'
    attributes = f' class="zoomed" style="width: {width * zoom}px"' if zoom != 1 and width else ''
    if root is not None:
        # Embed lossless WebP; the comparison captures and references remain PNGs.
        contents = None
        try:
            contents = (root / path).read_bytes()
            source = _webp_data_url(contents)
        except (OSError, ValueError, Image.DecompressionBombError) as error:
            # Comparison retains corrupt captures as evidence. A preview failure
            # must not prevent the remaining cases or the report from finishing.
            download = ""
            if contents is not None:
                download = f'<a download="{_escape(Path(path).name)}" href="{_data_url(contents, "application/octet-stream")}">Download original file</a>'
            return f'<figure><div class="error">Cannot preview {_escape(label.lower())}: {_escape(error)}</div>{download}<figcaption>{_escape(label)}</figcaption></figure>'
        return f'<figure><img{attributes} loading="lazy" src="{source}" alt="{_escape(label)}"><figcaption>{_escape(label)}</figcaption></figure>'
    href = quote(path, safe="/.-_")
    return f'<figure><a href="{href}"><img{attributes} loading="lazy" src="{href}" alt="{_escape(label)}"></a><figcaption>{_escape(label)}</figcaption></figure>'


STYLE = """
:root { color-scheme: dark; font-family: system-ui, sans-serif; }
body { margin: 2rem auto; padding: 0 1rem; max-width: 1500px; background: #141820; color: #e8ebf0; }
a { color: #92c5ff; } h1,h2,h3 { line-height: 1.3; overflow-wrap: anywhere; }
.counts,.links { display: flex; flex-wrap: wrap; gap: 1rem; margin: 1rem 0; }
.count,article { background: #202733; border: 1px solid #384457; border-radius: .6rem; padding: 1rem; }
article { margin: 1rem 0; } .pass { color: #86e3a4; } .fail,.error { color: #ff9b9b; } .skipped { color: #e9cd83; }
.images { display: grid; grid-template-columns: repeat(3,minmax(0,1fr)); gap: 1rem; }
figure { margin: 0; min-width: 0; } img { width: 100%; object-fit: contain; background: #202020; }
.images.zoomed { display: flex; flex-wrap: wrap; align-items: flex-start; }
.images.zoomed figure { flex: 0 0 auto; max-width: 100%; overflow-x: auto; }
img.zoomed { max-width: none; image-rendering: pixelated; }
figcaption { margin-top: .35rem; color: #bac5d5; } .missing { min-height: 6rem; padding: 1rem; border: 1px dashed #566479; }
pre { white-space: pre-wrap; overflow-wrap: anywhere; background: #141820; padding: 1rem; }
summary { cursor: pointer; padding: .5rem 0; } .reason { overflow-wrap: anywhere; }
@media(max-width: 700px) { .images { grid-template-columns: 1fr; } }
"""


def _case_options_html(configuration: dict) -> str:
    contents = ""
    if configuration.get("options_description"):
        contents += f'<p>{_escape(configuration["options_description"])}</p>'
    if configuration.get("reproduce_command"):
        contents += f'<details><summary>Reproduce image</summary><pre>{_escape(configuration["reproduce_command"])}</pre></details>'
    return contents


def _image_labels(result: dict) -> tuple:
    return (("actual", "Actual"),
            ("reference", "Reference" + (" · " + result['reference_version'] if result.get('reference_version') else "")),
            ("difference", "Difference (contrast ×4)"))


def _edge_check_html(result: dict) -> str:
    width = result.get('edge_width_pixels')
    if width is None:
        return ''
    expected = result['expected_edge_width_pixels']
    tolerance = result.get('edge_width_tolerance_pixels', 0.08)
    status = 'pass' if abs(width - expected) <= tolerance else 'fail'
    body = (f'<p class="{status}">Edge coverage: {status.upper()} — '
            f'{width:.4f} px; expected {expected:.4f} ± {tolerance:g} px</p>')
    if result.get('interior_opacity') is not None:
        opacity = result['interior_opacity']
        status = 'pass' if opacity >= .99 else 'fail'
        body += f'<p class="{status}">Solid interior: {status.upper()} — {opacity:.2%}; expected at least 99%</p>'
    if result.get('full_edge_width_pixels') is not None:
        width = result['full_edge_width_pixels']
        status = 'pass' if abs(width - 1.2168) <= .1 else 'fail'
        body += f'<p class="{status}">10%–90% edge coverage: {status.upper()} — {width:.4f} px; expected 1.2168 ± 0.1 px</p>'
    if result.get('coverage_quality'):
        quality = result['coverage_quality']
        body += f'<p class="{quality["status"]}">Independent coverage quality: {quality["status"].upper()}</p>'
        body += '<table><tr><th>Error (lower is better)</th><th>Current</th><th>1.13.1</th><th>Allowed increase</th></tr>'
        for key, label, factor, unit in (('edge_rmse', 'Edge RMS coverage', 100, '%'), ('area_error', 'Stroke area', 100, '%'), ('centroid_error_pixels', 'Center of coverage', 1, ' px')):
            body += f'<tr><td>{label}</td><td>{quality["actual"][key] * factor:.4f}{unit}</td><td>{quality["stable"][key] * factor:.4f}{unit}</td><td>{quality["tolerances"][key] * factor:g}{unit}</td></tr>'
        body += '</table>'
    return body



def _page(title: str, contents: str) -> str:
    return f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{_escape(title)}</title><style>{STYLE}</style></head><body>{contents}</body></html>'


def _artifact_source(result: dict, path: str, output_dir: Path) -> Path:
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = Path(result.get("artifact_root", output_dir)) / candidate
    return candidate


def _log_text(result: dict, output_dir: Path) -> str:
    logs = result.get("logs", "")
    if not logs:
        return ""
    if isinstance(logs, str):
        # Treat only explicit existing single-line paths as log files. Error
        # messages and multiline console output are report data, not paths.
        if "\n" not in logs and len(logs) < 4096:
            try:
                source = _artifact_source(result, logs, output_dir)
                if source.is_file():
                    return source.read_text(encoding="utf8", errors="replace")
            except OSError:
                pass
        return logs
    return json.dumps(logs, indent=2, ensure_ascii=False, default=str)


def build_report(results: list[dict], output_dir: Path | str, metadata: dict) -> dict:
    """Keep each executable's results separate, with a combined entry point."""
    metadata = _json_value(metadata)
    configurations = {}
    if metadata.get("source") in ("stable", "current") and not metadata.get("executable"):
        for case in metadata.get("expected_cases", []):
            if not isinstance(case, dict) or "full_layout" not in case or "rich_text" not in case:
                continue
            # Stable has no native rich-text parser. Both compatible plain-text
            # variants run in the same stable executable and belong together.
            rich = case["rich_text"] if metadata["source"] == "current" else False
            key = f"layout{int(case['full_layout'])}-rich{int(rich)}"
            configurations.setdefault(key, []).append(case)
    reports = []
    for key, cases in configurations.items():
        ids = {case["id"] for case in cases}
        child_metadata = dict(metadata, expected_cases=cases,
                              title=f"{metadata.get('title', 'Font rendering tests')} — {key}")
        child_metadata.pop("executable_reports", None)
        child_metadata["graphics_probe_errors"] = [probe for probe in metadata.get("graphics_probe_errors", [])
                                                    if probe["case"] in ids]
        child_metadata["executable"] = metadata.get("executables", {}).get(key, {
            "source": metadata["source"], "backend": metadata.get("backend"),
            "full_layout": cases[0]["full_layout"],
            "rich_text": cases[0]["rich_text"] if metadata["source"] == "current" else False})
        # Resolve artifacts against the original report before copying them to
        # a child directory. This also supports rebuilding downloaded reports.
        child_results = []
        for result in results:
            if result["id"] in ids:
                child = dict(result)
                child.setdefault("artifact_root", str(Path(output_dir).resolve()))
                child_results.append(child)
        relative = f"executables/{key}"
        summary = _build_report(child_results, Path(output_dir) / relative, child_metadata)
        reports.append({"configuration": key, "path": relative + "/index.html", "summary": summary})
    metadata["executable_reports"] = reports
    return _build_report(results, output_dir, metadata)


def _build_report(results: list[dict], output_dir: Path | str, metadata: dict) -> dict:
    """Write portable HTML, Markdown and JSON, without dropping failed cases.

    ``metadata.expected_cases`` optionally supplies the enumerated manifest
    (case dictionaries or IDs). Any missing result is materialized as an error.
    ``metadata.expected`` alternatively supplies an expected count.
    """
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    results = _json_value(results)
    metadata = _json_value(metadata)
    global_errors = [f"Graphics preflight failed for {probe['case']}: {probe['reason']}"
                     for probe in metadata.get("graphics_probe_errors", [])]
    seen = set()
    for result in results:
        case_id = str(result.get("id", "unnamed"))
        result["id"] = case_id
        # Older saved results included this label in every failure sentence.
        # The score and concrete failure already describe the result.
        issue = result.get("issue", "")
        if issue:
            result["reason"] = result.get("reason", "").removeprefix(issue + ": ")
        if case_id in seen:
            result["status"] = "error"
            result["reason"] = f"Duplicate result for case {case_id}"
        seen.add(case_id)
    expected_cases = metadata.get("expected_cases")
    if expected_cases is not None:
        expected_ids = []
        for case in expected_cases:
            case = case if isinstance(case, dict) else {"id": str(case)}
            case_id = str(case["id"])
            expected_ids.append(case_id)
            if case_id not in seen:
                results.append({"id": case_id, "status": "error", "reason": "Requested subtest produced no result", "configuration": case, "executed": False})
                seen.add(case_id)
        if len(set(expected_ids)) != len(expected_ids):
            global_errors.append("Expected manifest contains duplicate case IDs")
        unexpected = seen - set(expected_ids)
        if unexpected:
            global_errors.append("Unexpected case results: " + ", ".join(sorted(unexpected)))
        expected_count = len(expected_ids)
    else:
        expected_count = int(metadata.get("expected", len(results)))
        if expected_count != len(results):
            global_errors.append(f"Expected {expected_count} subtests, received {len(results)} results")
    if not expected_count or not results:
        global_errors.append("Zero subtests executed")

    groups = defaultdict(list)
    case_details = {}
    for index, result in enumerate(results):
        if result.get("status") not in ("pass", "fail", "error", "skipped"):
            result["status"] = "error"
            result["reason"] = "Subtest has no valid completion status"
        folder_name = _case_directory(result["id"], index)
        if metadata.get("stable_case_paths") and re.fullmatch(r"[A-Za-z0-9_-]+", result["id"]):
            folder_name = result["id"]
        case_dir = output_dir / "cases" / folder_name
        case_dir.mkdir(parents=True, exist_ok=True)
        copied_paths = {}
        copy_errors = []
        candidate = bool(result.get("candidate"))
        for key in ("actual", "reference", "difference", "stable_reference", "stable_difference", "coverage_reference"):
            path = result.get("paths", {}).get(key)
            if not path:
                if result["status"] == "pass" and key in ("actual", "reference", "difference") and (key == "actual" or not candidate):
                    copy_errors.append(f"Passing case has no {key} image")
                continue
            source = _artifact_source(result, path, output_dir)
            target = case_dir / f"{key}.png"
            try:
                _copy_file(source, target)
                copied_paths[key] = target.relative_to(output_dir).as_posix()
            except OSError as error:
                copy_errors.append(f"Could not retain {key} image: {error}")
        extras = {}
        for key, path in result.get("extra_artifacts", {}).items():
            source = _artifact_source(result, path, output_dir)
            suffix = source.suffix if re.fullmatch(r"\.[A-Za-z0-9]{1,8}", source.suffix) else ".bin"
            name = "data-" + re.sub(r"[^A-Za-z0-9_-]", "-", key) + suffix
            try:
                _copy_file(source, case_dir / name)
                extras[key] = (case_dir / name).relative_to(output_dir).as_posix()
            except OSError as error:
                copy_errors.append(f"Could not retain {key} data: {error}")
        logs = _log_text(result, output_dir)
        if logs:
            (case_dir / "console.log").write_text(logs, encoding="utf8")
            copied_paths["log"] = (case_dir / "console.log").relative_to(output_dir).as_posix()
        if copy_errors:
            result["status"] = "error"
            result["reason"] = "; ".join([result.get("reason", ""), *copy_errors]).strip("; ")
        result["paths"] = copied_paths
        result["extra_artifacts"] = extras
        result["artifact_root"] = str(output_dir)
        result["logs"] = copied_paths.get("log", "")
        result["report_path"] = (case_dir / "index.html").relative_to(output_dir).as_posix()
        configuration = result.get("configuration", {})
        options = _case_options_html(configuration)
        group = str(configuration.get("scenario", configuration.get("group", "Font rendering")))
        groups[group].append(result)
        zoom = result.get('display_zoom', 1)
        image_width = result.get('dimensions', [None])[0]
        image_class = 'images zoomed' if zoom != 1 else 'images'
        labels = _image_labels(result)
        local_images = "".join(_image_html(label, Path(copied_paths[key]).name if key in copied_paths else None, zoom=zoom, width=image_width) for key, label in labels)
        likeness = result.get("likeness_percent")
        score = f"{likeness:.4f}%" if isinstance(likeness, (int, float)) else "—"
        verification = '<p class="error">REFERENCE CANDIDATE — capture checks only; image requires review and acceptance.</p>' if candidate else ""
        extra_links = "".join(f'<a href="{quote(Path(path).name)}">{_escape(key)}</a>' for key, path in extras.items())
        body = f'<p><a href="../../index.html">All subtests</a></p><h1>{_escape(result["id"])}</h1><p class="{result["status"]}">{result["status"].upper()}{" · likeness " + score if isinstance(likeness, (int, float)) else ""}</p><p class="reason">{_escape(result.get("reason", ""))}</p><div class="{image_class}">{local_images}</div><h2>Configuration and comparison</h2><pre>{_escape(json.dumps(result, indent=2, ensure_ascii=False))}</pre><h2>Console log</h2><pre>{_escape(logs or "No console output")}</pre>'
        reference_origin = (f'<p>{_escape(result["reference_origin_note"])}</p>'
                            if result.get('reference_origin_note') else '')
        body = options + _edge_check_html(result) + reference_origin + body
        body += verification + f'<div class="links">{extra_links}</div>'
        downloads = "".join(_download_html(key, output_dir / path) for key, path in extras.items())
        case_details[result["id"]] = (
            f'<details><summary>Configuration, comparison and logs</summary>'
            f'<pre>{_escape(json.dumps(result, indent=2, ensure_ascii=False))}</pre>'
            f'<h4>Console log</h4><pre>{_escape(logs or "No console output")}</pre>'
            f'<div class="links">{downloads}</div></details>')
        (case_dir / "index.html").write_text(_page(result["id"], body), encoding="utf8")
        (case_dir / "result.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf8")

    passed = sum(result["status"] == "pass" for result in results)
    errors = sum(result["status"] == "error" for result in results)
    skipped = sum(result["status"] == "skipped" for result in results)
    failed = len(results) - passed - skipped
    completed = sum(result.get("executed", True) for result in results)
    status = "fail" if failed or global_errors else "pass" if passed else "skipped"
    summary = {"status": status, "expected": expected_count, "completed": completed, "passed": passed, "failed": failed, "skipped": skipped, "errors": errors, "report_errors": global_errors}
    document = {"metadata": metadata, "summary": summary, "results": results}
    (output_dir / "results.json").write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf8")
    (output_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf8")

    title = str(metadata.get("title", "Font rendering tests"))
    counts = "".join(f'<div class="count">{key.title()}: <strong>{summary[key]}</strong></div>' for key in ("expected", "completed", "passed", "failed", "skipped", "errors"))
    body = f'<h1>{_escape(title)}</h1><p class="{summary["status"]}">{summary["status"].upper()}</p><div class="counts">{counts}</div><div class="links"><a href="results.json">All results (JSON)</a><a href="summary.md">Markdown summary</a></div>'
    failed_checks = [result for result in results if result['status'] in ('fail', 'error')]
    if failed_checks or global_errors:
        body += '<h2>Failing checks</h2><ul>'
        for result in failed_checks:
            body += (f'<li class="{result["status"]}"><a href="#{_case_anchor(result)}">{_escape(result["id"])}</a>'
                     f' — {_escape(result.get("reason") or result["status"].upper())}</li>')
        body += '</ul>'
        for error in global_errors:
            body += f'<p class="error">{_escape(error)}</p>'
    lowest_scores = sorted(
        (result for result in results
         if isinstance(result.get("likeness_percent"), (int, float))
         and math.isfinite(result["likeness_percent"])),
        key=lambda result: (result["likeness_percent"], result["id"]))[:5]
    if lowest_scores:
        body += ('<h2>Lowest image likeness scores</h2>'
                 '<p>These percentages measure reference-image comparisons only. '
                 'Edge width and other validation checks can fail independently of image likeness.</p><ol>')
        for result in lowest_scores:
            body += (f'<li><a href="#{_case_anchor(result)}">{_escape(result["id"])}</a>'
                     f' — <strong>{result["likeness_percent"]:.4f}%</strong></li>')
        body += '</ol>'
    executable_reports = metadata.get("executable_reports", [])
    if executable_reports:
        body += '<h2>Reports by executable</h2><ul>'
        for executable in executable_reports:
            state = executable["summary"]
            body += f'<li>{_escape(executable["configuration"])}: {state["passed"]} passed, {state["failed"]} failed, {state["skipped"]} skipped, {state["completed"]}/{state["expected"]} completed</li>'
        body += '</ul>'
    markdown = [f"# {title}", "", f"**{summary['status'].upper()}** — Expected: {expected_count}; completed: {completed}; passed: {passed}; failed: {failed}; skipped: {skipped}; errors: {errors}.", ""]
    markdown.extend(f"- [{item['configuration']}]({item['path']})" for item in executable_reports)
    markdown.extend(f"- {error}" for error in global_errors)
    markdown.extend(["", "Image likeness measures reference-image comparisons only. Edge width and other validation checks can fail independently.",
                     "", "| Subtest | Status | Image likeness | Result |", "| --- | --- | --- | --- |"])
    for group, cases in groups.items():
        body += f"<h2>{_escape(group)}</h2>"
        for result in cases:
            options = _case_options_html(result.get("configuration", {})) + _edge_check_html(result)
            if result.get('reference_origin_note'):
                options += f'<p>{_escape(result["reference_origin_note"])}</p>'
            likeness = result.get("likeness_percent")
            score = f"{likeness:.4f}%" if isinstance(likeness, (int, float)) else "—"
            zoom = result.get('display_zoom', 1)
            image_width = result.get('dimensions', [None])[0]
            image_class = 'images zoomed' if zoom != 1 else 'images'
            labels = _image_labels(result)
            images = "".join(_image_html(label, result["paths"].get(key), output_dir, zoom=zoom, width=image_width) for key, label in labels)
            if result.get("candidate"):
                images = '<p class="error">REFERENCE CANDIDATE — requires review and acceptance.</p>' + images
            body += f'<article id="{_case_anchor(result)}"><h3><a href="#{_case_anchor(result)}">{_escape(result["id"])}</a></h3>{options}<p class="{result["status"]}">{result["status"].upper()}{" · likeness " + score if isinstance(likeness, (int, float)) else ""}</p><p class="reason">{_escape(result.get("reason", ""))}</p><div class="{image_class}">{images}</div>{case_details[result["id"]]}</article>'
            escaped_id = result["id"].replace("\\", "\\\\").replace("|", "\\|").replace("[", "\\[").replace("]", "\\]").replace("\n", " ")
            reason = result.get('reason', '').replace('|', '\\|').replace('\n', ' ')
            markdown.append(f"| [{escaped_id}]({quote(result['report_path'], safe='/.-_')}) | {result['status'].upper()} | {score} | {reason} |")
    body += f'<details><summary>Run metadata</summary><pre>{_escape(json.dumps(metadata, indent=2, ensure_ascii=False))}</pre></details>'
    (output_dir / "summary.md").write_text("\n".join(markdown) + "\n", encoding="utf8")
    body = body.replace('<a href="results.json">All results (JSON)</a>',
                        _download_html("All results (JSON)", output_dir / "results.json", "application/json"))
    body = body.replace('<a href="summary.md">Markdown summary</a>',
                        _download_html("Markdown summary", output_dir / "summary.md", "text/markdown"))
    (output_dir / "index.html").write_text(_page(title, body), encoding="utf8")
    return summary


def build_reports(images, output, binaries, generation_results):
    expected=[]
    results=[]
    executables={}
    for full,rich in ((False,False),(False,True),(True,False),(True,True)):
        configuration=('full' if full else 'legacy')+('-rich' if rich else '-plain')
        binary=binaries.get(configuration,'test_font_bitmap_gen'+('_skribidi' if full else '')+('' if rich else '_plain'))
        key=f'layout{int(full)}-rich{int(rich)}'
        executables[key]=dict(path=binary,full_layout=full,rich_text=rich)
        for case in cases(full,rich):
            name=case['id']
            command='cd '+shlex.quote(str(FONT_ROOT))+' && '+shlex.join([binary,'--case',name,'--output',str(images)])
            case=dict(case,id=configuration+'-'+name,full_layout=full,rich_text=rich,background=[0,0,0],
                scenario=configuration,options_description=f"{case['source']} · {'multi' if case['multi'] else 'single'} layer · {configuration} · {case['scenario']} · outline {case['outline']} px",
                reproduce_command=command)
            if case['change']:
                case['options_description'] += ' · short text/half outline → full text/full outline; shared atlas and GPU resources'
            if case.get('edge_scale'):
                case['options_description'] += f" · scale {case['edge_scale']} · edge sampled at 8× · display zoom 4× (nearest neighbour) · native reference and independent coverage checks"
            if name.endswith('_named_style'):
                case['options_description'] += ' · named style requests 2 px'
                if 'bitmap' in case['source']:
                    case['options_description'] += ' (baked outline retained)'
            if case.get('edge_scale'):
                stable_name = font_coverage.stable_case_name(case)
                case['stable_reference'] = str(FONT_ROOT/'src/test/data/reference/1.13.1'/configuration/(stable_name+'.png'))
                case['options_description'] += ' · 1.13.1 comparison is informational; the 98% gate uses reviewed native references'
            expected.append(case)
            result=compare_case(images/configuration/(name+'.png'),
                FONT_ROOT/'src/test/data/reference'/configuration/(name+'.png'),case,output/'comparisons'/case['id'])
            result['executed']=(images/configuration/(name+'.png')).is_file()
            data=images/configuration/(name+'.json')
            if data.exists(): result['extra_artifacts']={'glyph_vertex_data':str(data.resolve())}
            if generation_results:
                status=images/(configuration+'.exit-code'); log=images/(configuration+'.log')
                result['logs']=log.read_text(errors='replace', encoding="utf-8") if log.exists() else 'No generation log'
                if not status.exists() or status.read_text(encoding="utf-8").strip()!='0':
                    result['status']='error';result['reason']='Native assertions/generation failed; '+result['reason']
            if result['status'] in ('fail', 'error'):print(f"{case['id']}: {result['reason']}\nReproduce: {command}")
            results.append(result)
    return build_report(results,output,dict(title='Font library rendering tests',source='current',backend='opengl',
        expected_cases=expected,executables=executables,stable_case_paths=True,
        scope='Font generation, glyph-bank providers, layout, production Vector vertex/cache backend and shaders. No engine or project.',
        reference_policy='Rendered references require review; old raw-distance-field images are not equivalent.'))


def print_summary(summary, results):
    """Keep successful capture distinct from reference/comparison validation."""
    errors = Counter()
    for result in results:
        if result['status'] != 'error':
            continue
        reason = result.get('reason', '')
        if reason.startswith('Missing reference image'):
            errors['missing references'] += 1
        elif reason.startswith('Image dimensions differ'):
            errors['image-size mismatches'] += 1
        else:
            errors['other validation errors'] += 1
    mismatches = sum(result['status'] == 'fail' for result in results)
    print(f"Captures: {summary['completed']}/{summary['expected']} images produced")
    print(f"Comparisons: {summary['passed']} passed, {mismatches} failed image/quality checks, "
          f"{summary['errors']} validation errors, {summary.get('skipped', 0)} skipped")
    if errors:
        print('Validation errors: ' + ', '.join(f'{count} {reason}' for reason, count in errors.items()))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generate-cases',type=Path,help='Write the native C++ case include file')
    parser.add_argument('--images',type=Path)
    parser.add_argument('--results',type=Path,help='Rebuild a report from existing results.json')
    parser.add_argument('--output',type=Path,default=FONT_ROOT/'build/font-render-report')
    parser.add_argument('--executable',action='append',default=[])
    parser.add_argument('--generation-results',action='store_true')
    args=parser.parse_args()
    if args.generate_cases:
        generate_cases(args.generate_cases)
        return 0
    if args.results:
        document = json.loads(args.results.read_text(encoding="utf-8"))
        for result in document['results']:
            result['artifact_root'] = str(args.results.resolve().parent)
        summary = build_report(document['results'], args.output.resolve(), document.get('metadata', {}))
        print_summary(summary, document['results'])
        return 0 if summary['status'] in ('pass', 'skipped') else 1
    if not args.images:
        parser.error('--images or --results is required')
    summary=build_reports(args.images.resolve(),args.output.resolve(),dict(x.split('=',1) for x in args.executable),args.generation_results)
    document = json.loads((args.output/'results.json').read_text(encoding="utf-8"))
    print_summary(summary, document['results'])
    print(args.output.resolve()/'index.html')
    return 0 if summary['status'] in ('pass', 'skipped') else 1

if __name__=='__main__':sys.exit(main())
