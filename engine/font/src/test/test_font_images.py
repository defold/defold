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

"""Deterministic checks of image comparison and report failure accounting."""


import base64
import io
import hashlib
import json
import re
import shutil
from html.parser import HTMLParser
import tempfile
import unittest
from unittest import mock
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageEnhance, ImageFilter
import make_report as report
import font_coverage
import contextlib
import os
import platform
import subprocess


class CoverageTest(unittest.TestCase):
    def test_rectangle_fractional_pixel_area(self):
        # A unit-height rectangle starting a quarter-pixel into the image.
        image = font_coverage.rasterize([(.25, 0, 1.25, 1)], (3, 1), 1)
        self.assertEqual([191, 64, 0], list(image.getdata()))

    def test_overlapping_contours_do_not_double_count_area(self):
        commands = []
        for left, right in ((0, 2), (1, 3)):
            commands.extend([['M', left, 0], ['L', right, 0], ['L', right, 1], ['L', left, 1], ['Z']])
        rects = font_coverage.rectangles(commands)
        self.assertEqual(3, sum((r - l) * (b - t) for l, t, r, b in rects))
        self.assertEqual([255, 255, 255, 0], list(font_coverage.rasterize(rects, (4, 1), 1).getdata()))

    def test_unsupported_curves_require_an_oracle_update(self):
        with self.assertRaisesRegex(ValueError, 'straight, axis-aligned'):
            font_coverage.rectangles([['M', 0, 0], ['Q', 1, 1, 2, 0]])

    def test_quality_accepts_improved_coverage_despite_different_pixels(self):
        oracle = font_coverage.reference('ttf', 1)
        stable = oracle.filter(ImageFilter.BoxBlur(4))
        self.assertNotEqual(oracle.tobytes(), stable.tobytes())
        case = {'source': 'ttf_sdf', 'edge_scale': 1, 'size': 32}
        with tempfile.TemporaryDirectory() as directory:
            quality, failures = font_coverage.compare_quality(oracle, stable, case, Path(directory))
        self.assertEqual([], failures)
        self.assertEqual(0, quality['actual']['edge_rmse'])
        self.assertLess(quality['actual']['edge_rmse'], quality['stable']['edge_rmse'])

    def test_quality_rejects_lost_opacity_blur_and_displacement(self):
        oracle = font_coverage.reference('ttf', 1)
        case = {'source': 'ttf_sdf', 'edge_scale': 1, 'size': 32}
        candidates = (ImageEnhance.Brightness(oracle).enhance(.84),
                      oracle.filter(ImageFilter.BoxBlur(10)), ImageChops.offset(oracle, 8, 0))
        with tempfile.TemporaryDirectory() as directory:
            for candidate in candidates:
                quality, failures = font_coverage.compare_quality(candidate, oracle, case, Path(directory))
                self.assertEqual('fail', quality['status'])
                self.assertTrue(failures)


class ReportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.actual = self.root / "capture.png"
        self.expected = self.root / "accepted.png"
        self.case = {"id": "ttf-single-label", "description": "Blue outline survives default-style inheritance", "expect_outline": True, "expect_shadow": False}

    def image(self, path, outline=True, shadow=False, size=(256, 256)):
        image = Image.new("RGB", size, (32, 32, 32))
        draw = ImageDraw.Draw(image)
        if shadow:
            draw.rectangle((15, 15, 30, 30), fill=(0, 255, 0))
        if outline:
            draw.rectangle((9, 9, 22, 22), fill=(0, 0, 255))
        draw.rectangle((13, 13, 18, 18), fill=(255, 255, 255))
        image.save(path)

    def compare(self, **overrides):
        return report.compare_case(self.actual, self.expected, {**self.case, **overrides}, self.root / "comparison")

    def test_case_generation_with_windows_default_encoding(self):
        # Simulate Windows-1252 defaults on any host. Both the Arabic fixture
        # read and generated C++ write must explicitly choose UTF-8.
        read_text = Path.read_text
        write_text = Path.write_text

        def windows_read(path, *args, **kwargs):
            kwargs.setdefault("encoding", "cp1252")
            return read_text(path, *args, **kwargs)

        def windows_write(path, *args, **kwargs):
            kwargs.setdefault("encoding", "cp1252")
            return write_text(path, *args, **kwargs)

        generated = self.root / "font_image_cases.inc"
        with mock.patch.object(Path, "read_text", windows_read), mock.patch.object(Path, "write_text", windows_write):
            report.generate_cases(generated)
        arabic = json.loads((report.DATA / "lorem.json").read_text(encoding="utf-8"))["arabic"]
        self.assertIn(json.dumps(arabic, ensure_ascii=False), generated.read_text(encoding="utf-8"))

    def test_identical_and_reference_never_changes(self):
        # Comparing or reporting may copy a verified PNG, but must not rewrite it.
        self.image(self.actual)
        self.image(self.expected)
        reference_bytes = self.expected.read_bytes()
        result = self.compare()
        self.assertEqual("pass", result["status"])
        self.assertEqual(100.0, result["likeness_percent"])
        self.assertEqual(100.0, result["whole_likeness_percent"])
        self.assertGreater(result["effect_pixels"]["actual"]["outline"], 0)
        self.assertEqual(reference_bytes, self.expected.read_bytes())

    def test_sdf_edge_width_is_independent_of_reference(self):
        # Draw a straight edge from its screen-space coverage, independently
        # of font distance encoding. The renderer samples this at 8x resolution.
        for full_width, peak, expected in ((2.0, 1.0, 'pass'), (2.0, .75, 'pass'),
                                           (2.0 / 3.0, 1.0, 'fail'), (4.0, 1.0, 'fail')):
            image = Image.new('RGB', (128, 64))
            for x in range(image.width):
                t = max(0, min(1, .5 + (x / 8 - 4) / full_width))
                # Thin, downscaled stems can be correct without reaching 90%.
                alpha = round(255 * min(peak, t * t * (3 - 2 * t)))
                for y in range(8, 56):
                    image.putpixel((x, y), (alpha, alpha, alpha))
            image.save(self.actual)
            image.save(self.expected)
            result = self.compare(edge_scale=1, expect_outline=False, background=[0, 0, 0])
            self.assertEqual(expected, result['status'], result['reason'])
            self.assertEqual(100, result['likeness_percent'])
            self.assertIn('actual', result['paths'])
            self.expected.unlink()
            self.assertEqual('skipped' if expected == 'pass' else 'fail', self.compare(edge_scale=1)['status'])

    def test_solid_interior_fails_even_with_identical_references(self):
        for peak, inner_width, expected in ((.844, 2.0, 'fail'), (1.0, 2.0, 'pass'), (1.0, 2.0 / 3.0, 'fail')):
            image = Image.new('RGB', (128, 64))
            for x in range(image.width):
                width = 2.0 if x < 32 else inner_width
                t = max(0, min(1, .5 + (x / 8 - 4) / width))
                alpha = round(255 * min(peak, t * t * (3 - 2 * t)))
                for y in range(8, 56):
                    image.putpixel((x, y), (alpha, alpha, alpha))
            image.save(self.actual)
            image.save(self.expected)
            result = self.compare(edge_scale=.5, opaque_interior=True, expect_outline=False, background=[0, 0, 0])
            self.assertEqual(expected, result['status'], result['reason'])
            self.assertEqual(100, result['likeness_percent'])
            if expected == 'fail':
                self.assertIn('SDF interior opacity' if peak < .99 else 'SDF 10%-90% edge width', result['reason'])
            else:
                self.assertAlmostEqual(1.2168, result['full_edge_width_pixels'], delta=.1)

    def test_stable_likeness_is_informational_and_images_are_portable(self):
        self.image(self.actual)
        self.image(self.expected)
        stable = self.root / 'stable.png'
        Image.new('RGB', (128, 64), 'white').save(stable)
        with Image.open(self.actual) as actual:
            Image.new('RGB', actual.size, 'white').save(stable)
        result = self.compare(stable_reference=str(stable))
        self.assertEqual('pass', result['status'], result['reason'])
        self.assertEqual(100, result['likeness_percent'])
        self.assertLess(result['stable_likeness_percent'], 98)
        destination = self.root / 'historical-comparison'
        report.build_report([result], destination, {})
        stable.unlink()
        for page in (destination / 'index.html', next((destination / 'cases').glob('*/index.html'))):
            contents = page.read_text()
            self.assertEqual(3, contents.count('<img '))
            self.assertIn('<figcaption>Actual</figcaption>', contents)
            self.assertIn('<figcaption>Reference</figcaption>', contents)
            self.assertIn('<figcaption>Difference (contrast ×4)</figcaption>', contents)
            self.assertIn('1.13.1 fixture origin adjusted', contents)
        copied = next((destination / 'cases').glob('*/stable_reference.png'))
        self.assertTrue(copied.is_file())

    def test_sdf_reference_difference_and_fourfold_report_zoom(self):
        def edge(path, width):
            image = Image.new('RGB', (128, 64))
            for x in range(image.width):
                t = max(0, min(1, .5 + (x / 8 - 4) / width))
                alpha = round(255 * t * t * (3 - 2 * t))
                for y in range(8, 56):
                    image.putpixel((x, y), (alpha, alpha, alpha))
            image.save(path)
        edge(self.actual, 2 / 3)
        edge(self.expected, 2)
        result = self.compare(edge_scale=1, expect_outline=False, background=[0, 0, 0])
        self.assertEqual('fail', result['status'])
        self.assertLess(result['likeness_percent'], 98)
        self.assertIn('SDF edge width', result['reason'])
        self.assertEqual({'actual', 'reference', 'difference'}, set(result['paths']))
        output = self.root / 'zoom-report'
        report.build_report([result], output, {})
        case_page = next((output / 'cases').glob('*/index.html'))
        for page in (output / 'index.html', case_page):
            html = page.read_text()
            self.assertEqual(3, html.count('class="zoomed" style="width: 512px"'))
            self.assertIn('image-rendering: pixelated', html)
            self.assertIn('Reference · native', html)

    def test_quality_failure_is_visible_with_identical_native_references(self):
        source = font_coverage.reference('ttf', 1)
        source.point(lambda value: round(value * .84)).save(self.actual)
        shutil.copyfile(self.actual, self.expected)
        stable = report.FONT_ROOT / 'src/test/data/reference/1.13.1/full-plain/ttf_sdf_single_edge_one.png'
        result = self.compare(source='ttf_sdf', size=32, edge_scale=1, stable_reference=str(stable),
                              expect_outline=False, background=[0, 0, 0])
        self.assertEqual(100, result['likeness_percent'])
        self.assertEqual('fail', result['coverage_quality']['status'])
        self.assertIn('Coverage area_error', result['reason'])
        destination = self.root / 'quality-failure'
        report.build_report([result], destination, {})
        for path in (destination / 'index.html', next((destination / 'cases').glob('*/index.html'))):
            page = path.read_text()
            self.assertEqual(3, page.count('<img '))
            self.assertIn('Independent coverage quality: FAIL', page)
            self.assertIn('Error (lower is better)', page)
        self.assertTrue(next((destination / 'cases').glob('*/coverage_reference.png')).is_file())

    def test_sdf_contour_offset_fails_independently_of_likeness_threshold(self):
        # A translated, sharp synthetic H has perfect shape identity, but the
        # capture origins still differ. Do not register images to hide this.
        image = Image.new('RGB', (128, 128))
        for box in ((24, 16, 40, 112), (80, 16, 96, 112), (24, 56, 96, 72)):
            image.paste('white', box)
        image.save(self.actual)
        shifted = Image.new('RGB', image.size)
        shifted.paste(image, (6, 2))
        shifted.save(self.expected)
        result = self.compare(edge_scale=1, threshold=0, expect_outline=False, background=[0, 0, 0])
        self.assertEqual('fail', result['status'])
        self.assertEqual([-6.0, -2.0], result['contour_offset_capture_pixels'])
        self.assertIn('SDF contour origin differs by (-6, -2)', result['reason'])
        self.assertNotIn('Foreground likeness', result['reason'])

    def test_stable_sdf_references_preserve_original_edge_coverage(self):
        reference_root = report.FONT_ROOT / 'src/test/data/reference/1.13.1'
        provenance = json.loads((reference_root / 'sdf-edge-1.13.1-provenance.json').read_text())
        geometry = json.loads((report.DATA / 'capture_geometry.json').read_text())
        self.assertEqual('1.13.1', provenance['version'])
        self.assertEqual([-1.5, .5], provenance['capture']['origin_adjustment_font_pixels'])
        expected = set()
        for full, rich in ((False, False), (False, True), (True, False), (True, True)):
            configuration = ('full' if full else 'legacy') + ('-rich' if rich else '-plain')
            for case in report.cases(full, rich):
                if '_single_edge_' not in case['id']:
                    continue
                relative = configuration + '/' + case['id'] + '.png'
                expected.add(relative)
                path = reference_root / relative
                record = provenance['images'][relative]
                self.assertEqual(record['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
                with Image.open(path) as image:
                    self.assertAlmostEqual(.6084, report.sdf_edge_width(image), delta=.08)
                    capture = geometry[case['id'].replace('_sdf_single', '')]
                    self.assertEqual((capture['width'], capture['height']), image.size)
        self.assertEqual(24, len(expected))
        self.assertEqual(expected, set(provenance['images']))

    def test_sdf_edge_missing_or_blank_capture_is_an_error(self):
        self.assertEqual('error', self.compare(edge_scale=1)['status'])
        Image.new('RGB', (64, 64)).save(self.actual)
        self.assertEqual('error', self.compare(edge_scale=1)['status'])

    def test_missing_outline_cannot_hide_in_clear_background(self):
        # The outline is tiny relative to this canvas, so whole-image RMSE alone
        # would pass the regression. Foreground RMSE and visibility both fail.
        self.image(self.actual, outline=False, size=(1024, 1024))
        self.image(self.expected, size=(1024, 1024))
        result = self.compare()
        self.assertEqual("fail", result["status"])
        self.assertGreater(result["whole_likeness_percent"], 95.0)
        self.assertLess(result["likeness_percent"], 95.0)
        self.assertIn("Expected visible outline in actual", result["reason"])

    def test_extra_pixels_are_in_foreground_union(self):
        # Unexpected drawing must contribute even where the reference is clear.
        self.image(self.actual, shadow=True)
        self.image(self.expected)
        result = self.compare()
        self.assertEqual("fail", result["status"])
        self.assertIn("Expected no shadow in actual", result["reason"])
        self.assertLess(result["likeness_percent"], 95.0)

    def test_normalized_rgb_rmse_has_known_value(self):
        # All three channels differ by 127 in one foreground pixel, giving a
        # known normalized RMSE independent of the comparator implementation.
        Image.new("RGB", (1, 1), (128, 128, 128)).save(self.actual)
        Image.new("RGB", (1, 1), (255, 255, 255)).save(self.expected)
        result = self.compare(expect_outline=False)
        self.assertAlmostEqual(127 / 255, result["normalized_difference"])
        self.assertAlmostEqual(100 * (1 - 127 / 255), result["likeness_percent"])
        self.assertEqual("fail", result["status"])

    def test_effect_check_rejects_equally_broken_images(self):
        # Identical captures are insufficient if both paths lost the outline.
        self.image(self.actual, outline=False)
        self.image(self.expected, outline=False)
        result = self.compare()
        self.assertEqual(100.0, result["likeness_percent"])
        self.assertEqual("fail", result["status"])

    def test_candidate_validation_rejects_blank_and_missing_effect(self):
        # Candidate generation cannot compare with an accepted reference yet,
        # but it must reject both an empty frame and a silently lost outline.
        Image.new("RGB", (32, 32), (33, 32, 32)).save(self.actual)
        with self.assertRaisesRegex(ValueError, "No foreground pixels"):
            report.validate_capture(self.actual, self.case)
        self.image(self.actual, outline=False)
        with self.assertRaisesRegex(ValueError, "Expected visible outline in actual"):
            report.validate_capture(self.actual, self.case)
        self.image(self.actual)
        capture_bytes = self.actual.read_bytes()
        result = report.validate_capture(self.actual, self.case)
        self.assertGreater(result["foreground_pixels"], 0)
        self.assertGreater(result["effect_pixels"]["actual"]["outline"], 0)
        self.assertNotIn("likeness_percent", result)
        self.assertEqual(capture_bytes, self.actual.read_bytes())

    def test_candidate_visibility_is_separate_from_vertex_effects(self):
        # An opaque single-layer face can cover its crisp shadow completely.
        # Vertex shadow data is still required elsewhere; the capture expects
        # no visible green. A zero-width DF outline can leave a blue AA fringe,
        # so null leaves only that pixel expectation to reference comparison.
        self.image(self.actual)
        case = {**self.case, "expect_outline": None, "expect_shadow": False, "expect_shadow_data": True}
        result = report.validate_capture(self.actual, case)
        self.assertEqual(0, result["effect_pixels"]["actual"]["shadow"])
        self.image(self.actual, shadow=True)
        with self.assertRaisesRegex(ValueError, "Expected no shadow in actual"):
            report.validate_capture(self.actual, case)

    def test_candidate_validation_ignores_markers_outside_roi(self):
        # Reserved-color viewport markers outside the text region must neither
        # satisfy missing effects nor create unexpected visible shadow results.
        self.image(self.actual)
        with Image.open(self.actual) as image:
            ImageDraw.Draw(image).rectangle((100, 100, 110, 110), fill=(0, 255, 0))
            image.save(self.actual)
        report.validate_capture(self.actual, {**self.case, "roi": [0, 0, 32, 32]})
        with self.assertRaisesRegex(ValueError, "outside image dimensions"):
            report.validate_capture(self.actual, {**self.case, "roi": [0, 0, 300, 300]})
        with self.assertRaisesRegex(ValueError, "Missing actual image"):
            report.validate_capture(self.root / "missing.png", self.case)
        self.actual.write_bytes(b"not a PNG")
        with self.assertRaises(ValueError):
            report.validate_capture(self.actual, self.case)

    def test_missing_reference_preserves_actual(self):
        # A missing baseline skips comparison while retaining the actual image.
        self.image(self.actual)
        result = self.compare()
        self.assertEqual("skipped", result["status"])
        self.assertIn("Missing reference image", result["reason"])
        self.assertTrue((Path(result["artifact_root"]) / result["paths"]["actual"]).is_file())
        destination = self.root / "skipped-report"
        summary = report.build_report([result], destination, {"expected": 1})
        self.assertEqual("skipped", summary["status"])
        self.assertEqual(1, summary["skipped"])
        self.assertEqual(0, summary["failed"])
        self.assertEqual(0, summary["passed"])
        self.assertIn("SKIPPED", (destination / "index.html").read_text(encoding="utf-8"))
        self.actual.write_bytes(b"not a PNG")
        self.assertEqual("error", self.compare()["status"])

    def test_missing_actual_preserves_reference(self):
        self.image(self.expected)
        result = self.compare()
        self.assertEqual("error", result["status"])
        self.assertIn("Missing actual image", result["reason"])
        self.assertIn("reference", result["paths"])

    def test_dimensions_and_invalid_png_fail(self):
        # Never resize captures to make an accidental viewport change pass.
        self.image(self.actual, size=(64, 64))
        self.image(self.expected)
        with Image.open(self.expected) as expected:
            expected.putpixel((100, 100), (255, 32, 32))
            expected.save(self.expected)
        result = self.compare()
        self.assertIn("dimensions differ", result["reason"])
        self.assertEqual("fail", result["status"])
        self.assertIsInstance(result["likeness_percent"], float)
        self.assertIsInstance(result["whole_likeness_percent"], float)
        with Image.open(Path(result["artifact_root"]) / result["paths"]["difference"]) as difference:
            self.assertEqual((256, 256), difference.size)
            self.assertEqual((0, 0, 0), difference.getpixel((13, 13)))
            self.assertEqual((255, 0, 0), difference.getpixel((100, 100)))
        self.assertIn("no scaling", result["difference_note"])
        self.actual.write_bytes(b"not a PNG")
        self.assertEqual("error", self.compare()["status"])

    def test_empty_foreground_and_invalid_case_fail(self):
        # Two blank frames commonly indicate a capture timing bug, not success.
        Image.new("RGB", (32, 32), (32, 32, 32)).save(self.actual)
        Image.new("RGB", (32, 32), (33, 32, 32)).save(self.expected)
        self.assertIn("No foreground pixels", self.compare()["reason"])
        self.assertEqual("error", self.compare(threshold=-1)["status"])
        self.assertEqual("error", self.compare(threshold=float("nan"))["status"])
        self.assertEqual("error", self.compare(roi=(-1, 0, 2, 2))["status"])

    def test_roi_limits_comparison_and_effect_checks(self):
        # Decorations outside the declared text region must not affect likeness.
        self.image(self.actual)
        self.image(self.expected)
        with Image.open(self.actual) as image:
            ImageDraw.Draw(image).rectangle((100, 100, 200, 200), fill=(0, 255, 0))
            image.save(self.actual)
        result = self.compare(roi=(0, 0, 32, 32))
        self.assertEqual("pass", result["status"])
        self.assertLess(result["whole_likeness_percent"], 100)

    def test_portable_report_retains_mixed_and_missing_results(self):
        # A crashed subtest and a missing completion must remain in the gallery,
        # rather than disappearing while successful screenshots are collected.
        self.image(self.actual)
        self.image(self.expected)
        passed = self.compare(logs="capture complete\n")
        crashed = {"id": "crashed", "status": "error", "reason": "Process exited 139", "logs": "Fatal signal\n"}
        destination = self.root / "report"
        summary = report.build_report([passed, crashed], destination, {"expected_cases": [passed["id"], "crashed", "missing"]})
        self.assertEqual({"expected": 3, "completed": 2, "passed": 1, "failed": 2, "skipped": 0, "errors": 2, "status": "fail", "report_errors": []}, summary)
        document = json.loads((destination / "results.json").read_text(encoding="utf-8"))
        self.assertEqual(3, len(document["results"]))
        for result in document["results"]:
            self.assertTrue((destination / result["report_path"]).is_file())
            for path in result["paths"].values():
                self.assertFalse(Path(path).is_absolute())
                self.assertTrue((destination / path).is_file())
        page = (destination / "index.html").read_text(encoding="utf-8")
        self.assertIn("crashed", page)
        self.assertIn("missing", page)
        rebuilt = report.build_report(document["results"], self.root / "rebuilt", document["metadata"])
        self.assertEqual(summary, rebuilt)

    def test_corrupt_images_still_produce_complete_reports(self):
        # Exercise comparison through final HTML, including executable reports.
        # Keep the original corrupt bytes downloadable after sharing the HTML.
        for broken_key in ("actual", "reference"):
            with self.subTest(broken_key=broken_key):
                self.image(self.actual)
                self.image(self.expected)
                good = self.compare()
                good["configuration"].update(full_layout=False, rich_text=True)
                broken = self.actual if broken_key == "actual" else self.expected
                broken.write_bytes(b"not a PNG")
                case = {**self.case, "id": "corrupt", "full_layout": False, "rich_text": True}
                bad = report.compare_case(self.actual, self.expected, case, self.root / "bad")
                destination = self.root / ("report-" + broken_key)
                summary = report.build_report([bad, good], destination, {
                    "source": "current", "expected_cases": [bad["configuration"], good["configuration"]]})
                self.assertEqual("fail", summary["status"])
                self.assertEqual(1, summary["passed"])
                self.assertEqual(1, summary["errors"])
                document = json.loads((destination / "results.json").read_text(encoding="utf-8"))
                retained = destination / document["results"][0]["paths"][broken_key]
                self.assertEqual(b"not a PNG", retained.read_bytes())
                for path in (destination / "index.html", destination / "executables/layout0-rich1/index.html"):
                    page = path.read_text(encoding="utf-8")
                    self.assertIn("Cannot preview " + broken_key, page)
                    self.assertIn('download="' + broken_key + '.png"', page)
                    self.assertIn(base64.b64encode(b"not a PNG").decode("ascii"), page)
                    self.assertIn("PASS", page)
                self.assertTrue((destination / "summary.md").is_file())

    def test_webp_embedding_preserves_pixels_and_source(self):
        # Include transparent and partially transparent pixels: lossless must
        # preserve every channel, not only the visible composited result.
        image = Image.new("RGBA", (2, 2))
        image.putdata([(17, 31, 63, 0), (4, 128, 255, 127), (255, 0, 1, 255), (0, 0, 0, 255)])
        image.save(self.actual)
        original_bytes = self.actual.read_bytes()
        page = report._image_html("Actual", self.actual.name, self.root)
        encoded = re.search(r"data:image/webp;base64,([A-Za-z0-9+/=]+)", page).group(1)
        with Image.open(io.BytesIO(base64.b64decode(encoded))) as decoded:
            self.assertEqual("WEBP", decoded.format)
            self.assertEqual(image.tobytes(), decoded.convert("RGBA").tobytes())
        self.assertEqual(original_bytes, self.actual.read_bytes())

    def test_html_is_standalone_after_removing_report_directory(self):
        # Exercise the shared report and its executable-specific report. Neither
        # may depend on sibling PNGs, case pages, logs or JSON after sharing.
        self.image(self.actual)
        self.image(self.expected)
        result = self.compare(logs="capture complete\n<unsafe>")
        result["configuration"].update(full_layout=False, rich_text=True,
                                       reproduce_command="test_font_bitmap_gen --case example")
        attachment = self.root / "glyphs.json"
        attachment.write_text('{"glyphs": 23}', encoding="utf-8")
        result["extra_artifacts"] = {"glyphs": str(attachment)}
        destination = self.root / "standalone"
        report.build_report([result], destination, {
            "source": "current", "expected_cases": [result["configuration"]]})
        shared_pages = []
        for index, source in enumerate((destination / "index.html",
                                        destination / "executables/layout0-rich1/index.html")):
            shared = self.root / f"shared-{index}.html"
            shutil.copyfile(source, shared)
            shared_pages.append(shared)
        shutil.rmtree(destination)

        class Links(HTMLParser):
            def __init__(self):
                super().__init__()
                self.links = []
                self.images = []

            def handle_starttag(self, tag, attrs):
                values = dict(attrs)
                self.links.extend(value for key, value in attrs if key in ("src", "href"))
                if tag == "img":
                    self.images.append(values["src"])

        for shared in shared_pages:
            page = shared.read_text(encoding="utf-8")
            parser = Links()
            parser.feed(page)
            self.assertTrue(all(link.startswith(("data:", "#")) for link in parser.links))
            self.assertEqual(3, len(parser.images))
            for image in parser.images:
                self.assertTrue(image.startswith("data:image/webp;base64,"))
                with Image.open(io.BytesIO(base64.b64decode(image.split(",", 1)[1]))) as webp:
                    self.assertEqual("WEBP", webp.format)
            self.assertIn("capture complete\n&lt;unsafe&gt;", page)
            self.assertIn("test_font_bitmap_gen --case example", page)
            self.assertIn('download="data-glyphs.json"', page)
            self.assertIn('download="results.json"', page)

    def test_lowest_scores_link_to_five_cases_in_numeric_order(self):
        # Ignore unscored cases, break ties by ID, and link within the shared
        # HTML rather than to case pages that may not accompany the report.
        results = [dict(id=name, status='fail', likeness_percent=score)
                   for name, score in [('z', 90), ('b', 8), ('a', 8), ('c', 75),
                                       ('d', 100), ('e', 30), ('f', 60)]]
        results.append(dict(id='skipped', status='skipped', likeness_percent=None))
        destination = self.root / 'lowest'
        report.build_report(results, destination, {})
        page = (destination / 'index.html').read_text(encoding="utf-8")
        section = page.split('<h2>Lowest image likeness scores</h2>')[1].split('<ol>')[1].split('</ol>')[0]
        entries = re.findall(r'<a href="#([^"]+)">([^<]+)</a> — <strong>([^<]+)</strong>', section)
        self.assertEqual(['a', 'b', 'e', 'f', 'c'], [entry[1] for entry in entries])
        self.assertEqual(['8.0000%', '8.0000%', '30.0000%', '60.0000%', '75.0000%'],
                         [entry[2] for entry in entries])
        for anchor, _, _ in entries:
            self.assertIn(f'<article id="{anchor}">', page)
        self.assertLess(page.index('Lowest image likeness scores'), page.index('<article'))

    def test_edge_failure_is_prominent_when_all_image_scores_are_perfect(self):
        self.image(self.actual)
        self.image(self.expected)
        passed = dict(self.compare(), id='image-comparison')
        failed = dict(passed, id='sdf-edge', status='fail', likeness_percent=None,
                      edge_width_pixels=.21, expected_edge_width_pixels=.6084,
                      reason='SDF edge width 0.2100 px; expected 0.6084 ± 0.08 px')
        for result in (passed, failed):
            result['configuration'] = dict(result['configuration'], id=result['id'], full_layout=False, rich_text=True)
        destination = self.root / 'edge-summary'
        summary = report.build_report([passed, failed], destination, {
            'source': 'current', 'expected_cases': [r['configuration'] for r in (passed, failed)]})
        self.assertEqual('fail', summary['status'])
        for root in (destination, destination / 'executables/layout0-rich1'):
            page = (root / 'index.html').read_text(encoding='utf-8')
            self.assertLess(page.index('<h2>Failing checks'), page.index('<h2>Lowest image likeness'))
            failures = page.split('<h2>Failing checks</h2>')[1].split('</ul>')[0]
            self.assertIn('sdf-edge', failures)
            self.assertIn('0.2100 px; expected 0.6084', failures)
            self.assertNotIn('image-comparison', failures)
            self.assertIn('100.0000%', page)
            self.assertIn('reference-image comparisons only', page)
            anchor = re.search(r'href="#([^"]+)"', failures).group(1)
            self.assertIn(f'<article id="{anchor}">', page)
            markdown = (root / 'summary.md').read_text(encoding='utf-8')
            self.assertIn('0.2100 px; expected 0.6084', markdown)

    def test_passing_edge_coverage_is_visible_when_likeness_fails(self):
        self.image(self.actual)
        self.image(self.expected)
        result = dict(self.compare(), status='fail', likeness_percent=50.0,
                      edge_width_pixels=.61, expected_edge_width_pixels=.6084,
                      edge_width_tolerance_pixels=.08,
                      reason='Foreground likeness 50.0000% is below 98%')
        destination = self.root / 'edge-pass-likeness-fail'
        summary = report.build_report([result], destination, {})
        self.assertEqual('fail', summary['status'])
        pages = [destination / 'index.html', *destination.glob('cases/*/index.html')]
        self.assertEqual(2, len(pages))
        for path in pages:
            page = path.read_text(encoding='utf-8')
            self.assertIn('Edge coverage: PASS — 0.6100 px; expected 0.6084 ± 0.08 px', page)
            self.assertIn('Foreground likeness 50.0000% is below 98%', page)

    def test_zero_duplicate_and_missing_artifacts_never_pass(self):
        self.assertEqual("fail", report.build_report([], self.root / "empty", {})["status"])
        self.image(self.actual)
        self.image(self.expected)
        passed = self.compare()
        duplicate = report.build_report([passed, passed], self.root / "duplicate", {})
        self.assertEqual("fail", duplicate["status"])
        self.assertEqual(1, duplicate["errors"])
        broken = {"id": "broken", "status": "pass"}
        self.assertEqual("fail", report.build_report([broken], self.root / "broken", {})["status"])

    def test_executable_reports_partition_cases_and_preserve_missing_results(self):
        # Each linked feature combination has its own executable. A missing
        # capture must fail that report without appearing in unrelated builds.
        self.image(self.actual)
        self.image(self.expected)
        cases = [{"id": f"{full}-{rich}", "full_layout": full, "rich_text": rich}
                 for full in (False, True) for rich in (False, True)]
        results = [dict(self.compare(), id=case["id"], configuration=case) for case in cases[:-1]]
        destination = self.root / "partitioned"
        metadata = {"source": "current", "backend": "metal", "expected_cases": cases,
                    "graphics_probe_errors": [{"case": cases[-1]["id"], "reason": "probe failed"}]}
        summary = report.build_report(results, destination, metadata)
        self.assertEqual(3, summary["passed"])
        self.assertEqual(1, summary["failed"])
        document = json.loads((destination / "results.json").read_text(encoding="utf-8"))
        self.assertEqual(4, len(document["metadata"]["executable_reports"]))
        for item in document["metadata"]["executable_reports"]:
            child = destination / Path(item["path"]).parent
            data = json.loads((child / "results.json").read_text(encoding="utf-8"))
            self.assertEqual(1, data["summary"]["expected"])
            self.assertEqual(1, len(data["results"]))
            for path in data["results"][0]["paths"].values():
                self.assertTrue((child / path).is_file())
            if item["configuration"] == "layout1-rich1":
                self.assertEqual("fail", data["summary"]["status"])
                self.assertIn("probe failed", data["summary"]["report_errors"][0])
            else:
                self.assertEqual("pass", data["summary"]["status"])
        rebuilt = report.build_report(document["results"], self.root / "rebuilt-partitions", document["metadata"])
        self.assertEqual(summary, rebuilt)

    def test_stable_reports_share_executable_for_rich_case_variants(self):
        # Stable ignores the current rich-text build flag: grouping by that
        # requested case flag would incorrectly invent two extra executables.
        cases = [{"id": f"{full}-{rich}", "full_layout": full, "rich_text": rich}
                 for full in (False, True) for rich in (False, True)]
        destination = self.root / "stable-partitions"
        report.build_report([], destination, {"source": "stable", "expected_cases": cases})
        document = json.loads((destination / "results.json").read_text(encoding="utf-8"))
        children = document["metadata"]["executable_reports"]
        self.assertEqual(["layout0-rich0", "layout1-rich0"], [item["configuration"] for item in children])
        self.assertTrue(all(item["summary"]["expected"] == 2 for item in children))

    def test_graphics_probe_failure_survives_successful_case_capture(self):
        # Continue collecting screenshots after a probe fails, but do not turn
        # an adapter/capture preflight failure into a successful overall run.
        self.image(self.actual)
        self.image(self.expected)
        destination = self.root / "probe-failure"
        summary = report.build_report([self.compare()], destination,
                                      {"graphics_probe_errors": [{"case": "probe", "reason": "adapter fallback"}]})
        self.assertEqual(1, summary["passed"])
        self.assertEqual("fail", summary["status"])
        self.assertIn("adapter fallback", summary["report_errors"][0])
        self.assertIn("adapter fallback", (destination / "index.html").read_text(encoding="utf-8"))

    def test_report_escapes_text_and_case_paths(self):
        # Case IDs and logs appear in HTML; use escaped text and generated paths.
        case_id = '../../<script>alert("bad")</script>'
        result = {"id": case_id, "status": "error", "reason": "<b>not HTML</b>", "logs": "<script>bad()</script>"}
        destination = self.root / "escaped"
        report.build_report([result], destination, {})
        page = (destination / "index.html").read_text(encoding="utf-8")
        self.assertNotIn("<script>", page)
        self.assertIn("&lt;script&gt;", page)
        paths = json.loads((destination / "results.json").read_text(encoding="utf-8"))["results"][0]["paths"]
        self.assertTrue((destination / paths["log"]).resolve().is_relative_to(destination.resolve()))

    def test_candidate_reports_do_not_require_accepted_reference(self):
        # Candidate generation may pass capture/data checks before review, but
        # its gallery must never describe that image as a verified comparison.
        self.image(self.actual)
        metadata = self.root / "capture.json"
        metadata.write_text('{"backend": "metal"}', encoding="utf-8")
        candidate = {"id": "candidate", "status": "pass", "candidate": "candidate.png", "paths": {"actual": str(self.actual)}, "extra_artifacts": {"capture": str(metadata)}}
        destination = self.root / "candidates"
        self.assertEqual("pass", report.build_report([candidate], destination, {})["status"])
        self.assertIn("REFERENCE CANDIDATE", (destination / "index.html").read_text(encoding="utf-8"))
        document = json.loads((destination / "results.json").read_text(encoding="utf-8"))
        retained = document["results"][0]["extra_artifacts"]["capture"]
        self.assertTrue((destination / retained).is_file())
        self.assertEqual("pass", report.build_report(document["results"], self.root / "rebuilt-candidates", {})["status"])

    def test_missing_pillow_still_produces_failure_report(self):
        # Prerequisite failures must remain reportable without image packages.
        with mock.patch.object(report, "Image", None):
            result = self.compare()
            self.assertIn("requires Pillow", result["reason"])
            summary = report.build_report([result], self.root / "no-pillow", {})
            self.assertEqual("fail", summary["status"])
            with self.assertRaisesRegex(ValueError, "requires Pillow"):
                report.validate_capture(self.actual, self.case)







class FontImageReportTest(unittest.TestCase):
    def test_summary_distinguishes_capture_from_comparison_failures(self):
        # A PNG exists for every case, but only one comparison actually ran.
        # Missing baselines and incompatible framing must not imply a crash.
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            report.print_summary(
                dict(completed=3, expected=3, passed=0, errors=1, skipped=1),
                [dict(status='skipped', reason='Missing reference image: missing.png'),
                 dict(status='error', reason='Image dimensions differ: actual (10, 10), reference (9, 9)'),
                 dict(status='fail', reason='Foreground likeness 90% is below 95%')])
        self.assertIn('Captures: 3/3 images produced', output.getvalue())
        self.assertIn('0 passed, 1 failed image/quality checks, 1 validation errors, 1 skipped', output.getvalue())
        self.assertNotIn('missing references', output.getvalue())
        self.assertIn('1 image-size mismatches', output.getvalue())

    def test_matrix_keeps_all_sources_and_build_configurations(self):
        count = 0
        for full in (False, True):
            for rich in (False, True):
                cases = report.cases(full, rich)
                self.assertEqual(len(cases), len({c['id'] for c in cases}))
                self.assertTrue(set(report.SOURCES).issubset({c['source'] for c in cases}))
                self.assertEqual(rich, any(c['markup'] for c in cases))
                self.assertEqual(full, any(c['source'] == 'arabic' for c in cases))
                count += len(cases)
        self.assertEqual(392, count)

    def test_failed_comparison_keeps_images_and_passing_cases_stay_quiet(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            images = root/'images'
            case = report.cases(False, False)[0]
            configurations = ('legacy-plain','legacy-rich','full-plain','full-rich')
            for configuration in configurations:
                actual = images/configuration/(case['id']+'.png')
                reference = root/'src/test/data/reference'/configuration/actual.name
                actual.parent.mkdir(parents=True)
                reference.parent.mkdir(parents=True)
                Image.new('RGB',(8,8),'white').save(actual)
                Image.new('RGB',(8,8),'white').save(reference)
            with mock.patch.object(report,'FONT_ROOT',root), mock.patch.object(report,'cases',return_value=[case]):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    summary = report.build_reports(images,root/'report',{},False)
                self.assertEqual(4,summary['passed'])
                self.assertEqual('',output.getvalue())
                Image.new('RGB',(8,8),'black').save(images/'legacy-rich'/(case['id']+'.png'))
                with contextlib.redirect_stdout(output):
                    summary = report.build_reports(images,root/'report',{},False)
                self.assertEqual(1,summary['failed'])
                self.assertEqual(3,summary['passed'])
                self.assertIn('Reproduce:',output.getvalue())
                page=root/'report/cases'/('legacy-rich-'+case['id'])/'index.html'
                self.assertIn('--case '+case['id'],page.read_text(encoding="utf-8"))
                self.assertTrue((page.parent/'actual.png').exists())
                self.assertTrue((page.parent/'difference.png').exists())

    def test_missing_images_finish_a_failure_report(self):
        with tempfile.TemporaryDirectory() as temporary, contextlib.redirect_stdout(io.StringIO()):
            root=Path(temporary)
            summary=report.build_reports(root/'missing',root/'report',{},False)
            self.assertEqual(392,summary['failed'])
            self.assertEqual(0,summary['completed'])
            self.assertEqual('fail',summary['status'])
            self.assertTrue((root/'report/index.html').exists())






ROOT = Path(__file__).resolve().parents[2]
HOST = platform.machine().lower() + '-' + {'Darwin': 'macos', 'Linux': 'linux', 'Windows': 'win32'}.get(platform.system(), platform.system().lower())
BINARY = Path(os.environ.get('FONT_BITMAP_GENERATOR', ROOT / 'build' / HOST / 'src/test/test_font_bitmap_gen'))


@unittest.skipUnless(BINARY.is_file(), 'Build test_font_bitmap_gen first')
class BitmapGeneratorCliTest(unittest.TestCase):
    def run_generator(self, *args):
        return subprocess.run([str(BINARY), *args], cwd=ROOT, capture_output=True, text=True, timeout=5)

    def test_help_does_not_initialize_graphics(self):
        result = self.run_generator('--help')
        self.assertEqual(0, result.returncode)
        self.assertIn('--shadow-blur', result.stdout)

    def test_invalid_input_fails_before_graphics(self):
        # Include prefixes: --case must select exactly one supported case.
        for args in (('--case', 'ttf'), ('--case', 'missing'), ('--source', 'missing'),
                     ('--size', 'nan'), ('--size', '0'), ('--outline', '-1'),
                     ('--face-alpha', '1.1'), ('--size', '40px'), ('--layers', 'both'),
                     ('--outline',), ('--unknown', '1'),
                     ('--case', 'ttf_sdf_single_default', '--size', '50')):
            with self.subTest(args=args):
                self.assertEqual(2, self.run_generator(*args).returncode)



class LikenessPrerequisiteTest(unittest.TestCase):
    def test_missing_pillow_explains_install_command(self):
        with mock.patch.object(report.likeness, "Image", None):
            with self.assertRaisesRegex(ValueError, "build.py install_ext"):
                report.likeness.check_tools()

    def test_probe_verifies_png_and_rmse(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            report.likeness.check_tools()
        self.assertIn("PNG read/write, RGB RMSE, difference and lossless WebP probes passed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
