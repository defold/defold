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
import json
import re
import shutil
from html.parser import HTMLParser
import tempfile
import unittest
from unittest import mock
from pathlib import Path
from PIL import Image, ImageDraw
import make_report as report
import contextlib
import os
import platform
import subprocess


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
        self.assertIn("SKIPPED", (destination / "index.html").read_text())
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
        document = json.loads((destination / "results.json").read_text())
        self.assertEqual(3, len(document["results"]))
        for result in document["results"]:
            self.assertTrue((destination / result["report_path"]).is_file())
            for path in result["paths"].values():
                self.assertFalse(Path(path).is_absolute())
                self.assertTrue((destination / path).is_file())
        page = (destination / "index.html").read_text()
        self.assertIn("crashed", page)
        self.assertIn("missing", page)
        rebuilt = report.build_report(document["results"], self.root / "rebuilt", document["metadata"])
        self.assertEqual(summary, rebuilt)

    def test_html_is_standalone_after_removing_report_directory(self):
        # Exercise the shared report and its executable-specific report. Neither
        # may depend on sibling PNGs, case pages, logs or JSON after sharing.
        self.image(self.actual)
        self.image(self.expected)
        result = self.compare(logs="capture complete\n<unsafe>")
        result["configuration"].update(full_layout=False, rich_text=True,
                                       reproduce_command="test_font_bitmap_gen --case example")
        attachment = self.root / "glyphs.json"
        attachment.write_text('{"glyphs": 23}')
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
            page = shared.read_text()
            parser = Links()
            parser.feed(page)
            self.assertTrue(all(link.startswith(("data:", "#")) for link in parser.links))
            self.assertEqual(3, len(parser.images))
            for image in parser.images:
                self.assertTrue(image.startswith("data:image/png;base64,"))
                with Image.open(io.BytesIO(base64.b64decode(image.split(",", 1)[1]))) as png:
                    self.assertEqual("PNG", png.format)
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
        page = (destination / 'index.html').read_text()
        section = page.split('<h2>Lowest likeness scores</h2><ol>')[1].split('</ol>')[0]
        entries = re.findall(r'<a href="#([^"]+)">([^<]+)</a> — <strong>([^<]+)</strong>', section)
        self.assertEqual(['a', 'b', 'e', 'f', 'c'], [entry[1] for entry in entries])
        self.assertEqual(['8.0000%', '8.0000%', '30.0000%', '60.0000%', '75.0000%'],
                         [entry[2] for entry in entries])
        for anchor, _, _ in entries:
            self.assertIn(f'<article id="{anchor}">', page)
        self.assertLess(page.index('Lowest likeness scores'), page.index('<article'))

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
        document = json.loads((destination / "results.json").read_text())
        self.assertEqual(4, len(document["metadata"]["executable_reports"]))
        for item in document["metadata"]["executable_reports"]:
            child = destination / Path(item["path"]).parent
            data = json.loads((child / "results.json").read_text())
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
        document = json.loads((destination / "results.json").read_text())
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
        self.assertIn("adapter fallback", (destination / "index.html").read_text())

    def test_report_escapes_text_and_case_paths(self):
        # Case IDs and logs appear in HTML; use escaped text and generated paths.
        case_id = '../../<script>alert("bad")</script>'
        result = {"id": case_id, "status": "error", "reason": "<b>not HTML</b>", "logs": "<script>bad()</script>"}
        destination = self.root / "escaped"
        report.build_report([result], destination, {})
        page = (destination / "index.html").read_text()
        self.assertNotIn("<script>", page)
        self.assertIn("&lt;script&gt;", page)
        paths = json.loads((destination / "results.json").read_text())["results"][0]["paths"]
        self.assertTrue((destination / paths["log"]).resolve().is_relative_to(destination.resolve()))

    def test_candidate_reports_do_not_require_accepted_reference(self):
        # Candidate generation may pass capture/data checks before review, but
        # its gallery must never describe that image as a verified comparison.
        self.image(self.actual)
        metadata = self.root / "capture.json"
        metadata.write_text('{"backend": "metal"}')
        candidate = {"id": "candidate", "status": "pass", "candidate": "candidate.png", "paths": {"actual": str(self.actual)}, "extra_artifacts": {"capture": str(metadata)}}
        destination = self.root / "candidates"
        self.assertEqual("pass", report.build_report([candidate], destination, {})["status"])
        self.assertIn("REFERENCE CANDIDATE", (destination / "index.html").read_text())
        document = json.loads((destination / "results.json").read_text())
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
        self.assertIn('0 passed, 1 visual mismatches, 1 validation errors, 1 skipped', output.getvalue())
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
        self.assertEqual(344, count)

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
                self.assertIn('--case '+case['id'],page.read_text())
                self.assertTrue((page.parent/'actual.png').exists())
                self.assertTrue((page.parent/'difference.png').exists())

    def test_missing_images_finish_a_failure_report(self):
        with tempfile.TemporaryDirectory() as temporary, contextlib.redirect_stdout(io.StringIO()):
            root=Path(temporary)
            summary=report.build_reports(root/'missing',root/'report',{},False)
            self.assertEqual(344,summary['failed'])
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
        self.assertIn("PNG read/write, RGB RMSE and difference probes passed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
