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
import json
import math
import re
import shutil
import sys
from collections import defaultdict
from pathlib import Path
from urllib.parse import quote
from collections import Counter
import shlex
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "scripts"))
import likeness
FONT_ROOT = Path(__file__).resolve().parents[2]

try:
    from PIL import Image, ImageChops
except ImportError:
    # A failed prerequisite check still needs its HTML/JSON failure report.
    Image = ImageChops = None


DATA = Path(__file__).resolve().parent / 'data/font_render'
TEXT = 'ABCDEFGabcdefg 0123456789'
SOURCES = ('ttf_sdf', 'otf_sdf', 'ttf_bitmap', 'otf_bitmap', 'ttf_sdf_bank', 'otf_sdf_bank', 'ttf_bitmap_bank', 'otf_bitmap_bank', 'fnt')

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
        missing = []
        for key, source, name in (("actual", actual_path, "actual.png"), ("reference", expected_path, "reference.png")):
            if not source.is_file():
                missing.append(f"Missing {key} image: {source}")
            else:
                _copy_file(source, destination / name)
                result["paths"][key] = name
        if missing:
            if actual_path.is_file() and not expected_path.is_file():
                # Missing baselines skip comparison, but must not hide a bad
                # capture. Verify the existing PNG before marking it skipped.
                with Image.open(actual_path) as actual:
                    if actual.format != "PNG":
                        raise ValueError("Captures must be PNG images")
                    actual.verify()
                result["status"] = "skipped"
                result["reason"] = "; ".join(missing)
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
        failures = []
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


def _image_html(label: str, path: str | None, root: Path | None = None) -> str:
    if not path:
        return f'<figure><div class="missing">No {_escape(label.lower())} image</div><figcaption>{_escape(label)}</figcaption></figure>'
    if root is not None:
        # Inline PNG bytes so copying index.html alone retains every image.
        source = _data_url((root / path).read_bytes(), "image/png")
        return f'<figure><img loading="lazy" src="{source}" alt="{_escape(label)}"><figcaption>{_escape(label)}</figcaption></figure>'
    href = quote(path, safe="/.-_")
    return f'<figure><a href="{href}"><img loading="lazy" src="{href}" alt="{_escape(label)}"></a><figcaption>{_escape(label)}</figcaption></figure>'


STYLE = """
:root { color-scheme: dark; font-family: system-ui, sans-serif; }
body { margin: 2rem auto; padding: 0 1rem; max-width: 1500px; background: #141820; color: #e8ebf0; }
a { color: #92c5ff; } h1,h2,h3 { line-height: 1.3; overflow-wrap: anywhere; }
.counts,.links { display: flex; flex-wrap: wrap; gap: 1rem; margin: 1rem 0; }
.count,article { background: #202733; border: 1px solid #384457; border-radius: .6rem; padding: 1rem; }
article { margin: 1rem 0; } .pass { color: #86e3a4; } .fail,.error { color: #ff9b9b; } .skipped { color: #e9cd83; }
.images { display: grid; grid-template-columns: repeat(3,minmax(0,1fr)); gap: 1rem; }
figure { margin: 0; min-width: 0; } img { width: 100%; object-fit: contain; background: #202020; }
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
        for key in ("actual", "reference", "difference"):
            path = result.get("paths", {}).get(key)
            if not path:
                if result["status"] == "pass" and (key == "actual" or not candidate):
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
        local_images = "".join(_image_html(label, Path(copied_paths[key]).name if key in copied_paths else None) for key, label in (("actual", "Actual"), ("reference", "Reference"), ("difference", "Difference (4×)")))
        likeness = result.get("likeness_percent")
        score = f"{likeness:.4f}%" if isinstance(likeness, (int, float)) else "—"
        verification = '<p class="error">REFERENCE CANDIDATE — capture checks only; image requires review and acceptance.</p>' if candidate else ""
        extra_links = "".join(f'<a href="{quote(Path(path).name)}">{_escape(key)}</a>' for key, path in extras.items())
        body = f'<p><a href="../../index.html">All subtests</a></p><h1>{_escape(result["id"])}</h1><p class="{result["status"]}">{result["status"].upper()}{" · likeness " + score if isinstance(likeness, (int, float)) else ""}</p><p class="reason">{_escape(result.get("reason", ""))}</p><div class="images">{local_images}</div><h2>Configuration and comparison</h2><pre>{_escape(json.dumps(result, indent=2, ensure_ascii=False))}</pre><h2>Console log</h2><pre>{_escape(logs or "No console output")}</pre>'
        body = options + body
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
    lowest_scores = sorted(
        (result for result in results
         if isinstance(result.get("likeness_percent"), (int, float))
         and math.isfinite(result["likeness_percent"])),
        key=lambda result: (result["likeness_percent"], result["id"]))[:5]
    if lowest_scores:
        body += '<h2>Lowest likeness scores</h2><ol>'
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
    for error in global_errors:
        body += f'<p class="error">{_escape(error)}</p>'
    markdown = [f"# {title}", "", f"**{summary['status'].upper()}** — Expected: {expected_count}; completed: {completed}; passed: {passed}; failed: {failed}; skipped: {skipped}; errors: {errors}.", ""]
    markdown.extend(f"- [{item['configuration']}]({item['path']})" for item in executable_reports)
    markdown.extend(f"- {error}" for error in global_errors)
    markdown.extend(["", "| Subtest | Status | Likeness |", "| --- | --- | --- |"])
    for group, cases in groups.items():
        body += f"<h2>{_escape(group)}</h2>"
        for result in cases:
            options = _case_options_html(result.get("configuration", {}))
            likeness = result.get("likeness_percent")
            score = f"{likeness:.4f}%" if isinstance(likeness, (int, float)) else "—"
            images = "".join(_image_html(label, result["paths"].get(key), output_dir) for key, label in (("actual", "Actual"), ("reference", "Reference"), ("difference", "Difference (4×)")))
            if result.get("candidate"):
                images = '<p class="error">REFERENCE CANDIDATE — requires review and acceptance.</p>' + images
            body += f'<article id="{_case_anchor(result)}"><h3><a href="#{_case_anchor(result)}">{_escape(result["id"])}</a></h3>{options}<p class="{result["status"]}">{result["status"].upper()}{" · likeness " + score if isinstance(likeness, (int, float)) else ""}</p><p class="reason">{_escape(result.get("reason", ""))}</p><div class="images">{images}</div>{case_details[result["id"]]}</article>'
            escaped_id = result["id"].replace("\\", "\\\\").replace("|", "\\|").replace("[", "\\[").replace("]", "\\]").replace("\n", " ")
            markdown.append(f"| [{escaped_id}]({quote(result['report_path'], safe='/.-_')}) | {result['status'].upper()} | {score} |")
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
            if name.endswith('_named_style'):
                case['options_description'] += ' · named style requests 2 px'
                if 'bitmap' in case['source']:
                    case['options_description'] += ' (baked outline retained)'
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
        scope='Font generation, glyph-bank providers, layout, vertex packing and shaders. No engine or project.',
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
    print(f"Comparisons: {summary['passed']} passed, {mismatches} visual mismatches, "
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
