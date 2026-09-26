#!/usr/bin/env python3
"""Device-independent failure and report tests, plus capture CLI validation."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import run_graphics_images as report


def cubemap_fixture():
    image = report.likeness.Image.new('RGB', report.SIZE, report.BACKGROUND)
    glyphs = {
        '+': ('.#.', '.#.', '###', '.#.', '.#.'),
        '-': ('...', '...', '###', '...', '...'),
        'X': ('#.#', '#.#', '.#.', '#.#', '#.#'),
        'Y': ('#.#', '#.#', '.#.', '.#.', '.#.'),
        'Z': ('###', '..#', '.#.', '#..', '###'),
    }
    faces = (
        ('+X', (208, 64, 64), (128, 104)), ('-X', (64, 176, 96), (32, 104)),
        ('+Y', (72, 104, 208), (80, 56)), ('-Y', (208, 176, 48), (80, 152)),
        ('+Z', (48, 176, 192), (80, 104)), ('-Z', (176, 64, 192), (176, 104)),
    )
    for label, color, position in faces:
        face = report.likeness.Image.new('RGB', (16, 16), (24, 24, 24))
        face.paste(color, (1, 1, 15, 15))
        face.paste((24, 24, 24), (11, 12, 14, 14))
        face.paste((255, 255, 255), (2, 2, 6, 3))
        face.paste((255, 255, 255), (2, 2, 3, 5))
        for letter, left in zip(label, (4, 8)):
            for row, pattern in enumerate(glyphs[letter]):
                for column, pixel in enumerate(pattern):
                    if pixel == '#':
                        face.putpixel((left + column, 6 + row), (255, 255, 255))
        image.paste(face.resize((48, 48), report.likeness.Image.Resampling.NEAREST), position)
    return image


class GraphicsImagesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.references = self.root / 'references'
        self.references.mkdir()
        self.images = self.root / 'captures'
        self.images.mkdir()
        self.output = self.root / 'report'
        self.records = []
        for case in report.CASES:
            self.fixture(case).save(self.references / (case + '.png'))

    def fixture(self, case):
        if case == 'cubemap':
            return cubemap_fixture()
        image = report.likeness.Image.new('RGB', report.SIZE, report.BACKGROUND)
        orange, green, blue = (223, 96, 32), (32, 191, 96), (32, 96, 223)
        yellow, magenta, cyan = (223, 191, 32), (223, 32, 191), (32, 191, 223)
        # Describe the final pixels independently of the GPU's stencil commands.
        if case == 'stencil_nested':
            image.paste(orange, (32, 32, 192, 208))
            image.paste(green, (96, 64, 192, 176))
            image.paste(blue, (128, 96, 192, 144))
        elif case == 'stencil_masks':
            image.paste(yellow, (16, 16, 240, 240))
            image.paste(orange, (32, 32, 144, 224))
            image.paste(green, (96, 64, 224, 192))
            image.paste(blue, (96, 64, 144, 192))
            image.paste(magenta, (32, 112, 144, 128))
            image.paste(cyan, (96, 144, 224, 160))
        elif case == 'stencil_ops':
            for top in (24, 96, 168):
                for left in (24, 96, 168):
                    image.paste(green, (left, top, left + 48, top + 48))
        elif case == 'stencil_depth':
            image.paste(orange, (32, 64, 224, 112))
            image.paste(green, (32, 144, 224, 192))
            image.paste(orange, (96, 144, 160, 192))
            image.paste(blue, (32, 208, 224, 224))
        elif case == 'stencil_faces':
            image.paste(orange, (32, 32, 112, 112))
            image.paste(cyan, (144, 32, 224, 112))
            image.paste(magenta, (32, 144, 112, 224))
            image.paste(green, (144, 144, 224, 224))
        elif case != 'clear':
            # A deterministic foreground rectangle is sufficient for harness tests.
            image.paste((223, 96, 32), (64, 48, 160, 160))
        return image

    def add_backend(self, backend, status='pass', root=None):
        root = root or self.images
        for case in report.CASES:
            path = root / backend / (case + '.png')
            path.parent.mkdir(parents=True, exist_ok=True)
            self.fixture(case).save(path)
            self.records.append(dict(backend=backend, actual_backend=backend, case=case, status=status,
                                     exit_code=0, command=['capture', '--backend', backend], log=''))
        (root / 'captures.json').write_text(json.dumps(self.records))

    def make_report(self, roots=None):
        with contextlib.redirect_stdout(io.StringIO()):
            return report.make_report(roots or [self.images], self.references, self.output)

    def test_threshold_boundary(self):
        self.assertFalse(report.passed(98.999999))
        self.assertTrue(report.passed(99.0))
        self.assertTrue(report.passed(99.000001))
        a = self.root / 'a.png'
        b = self.root / 'b.png'
        report.likeness.Image.new('RGB', report.SIZE, (100, 100, 100)).save(a)
        for change, status in ((2, 'pass'), (3, 'fail')):
            report.likeness.Image.new('RGB', report.SIZE, (100 + change,) * 3).save(b)
            result = report.comparison(a, b, self.root / 'diff.png', 'clear')
            self.assertEqual(status, result['status'])
            self.assertAlmostEqual(100 * (1 - change / 255), result['likeness_percent'])

    def test_whole_clear_and_empty_foreground(self):
        path = self.references / 'clear.png'
        self.assertEqual('pass', report.comparison(path, path, self.root / 'diff.png', 'clear')['status'])
        result = report.comparison(path, path, self.root / 'diff.png', 'triangle')
        self.assertEqual('fail', result['status'])
        self.assertIn('No foreground', result['reason'])

    def test_unmasked_stencil_fails(self):
        image = self.fixture('stencil')
        image.paste((223, 96, 32), (32, 32, 224, 208))
        actual = self.root / 'unmasked.png'
        image.save(actual)
        result = report.comparison(actual, self.references / 'stencil.png', self.root / 'diff.png', 'stencil')
        self.assertEqual('fail', result['status'])
        self.assertLess(result['likeness_percent'], 99)

    def test_advanced_stencil_references_match_expected_pixels(self):
        references = Path(report.__file__).with_name('graphics_reference')
        for case in report.CASES:
            if case.startswith('stencil_'):
                with self.subTest(case=case):
                    actual = report.read_png(references / (case + '.png'))
                    difference = report.likeness.ImageChops.difference(self.fixture(case), actual)
                    self.assertIsNone(difference.getbbox(), 'Reference does not match the expected stencil result')

    def test_advanced_stencil_faults_fail_likeness(self):
        # Representative regressions: child escapes parent, write mask ignored,
        # one operation produces no tile, depth failure skipped, back face lost.
        faults = {
            'stencil_nested': ((192, 64, 224, 176), (32, 191, 96)),
            'stencil_masks': ((32, 32, 96, 224), report.BACKGROUND),
            'stencil_ops': ((168, 168, 216, 216), report.BACKGROUND),
            'stencil_depth': ((96, 144, 160, 192), (32, 191, 96)),
            'stencil_faces': ((144, 32, 224, 112), report.BACKGROUND),
        }
        for case, (rectangle, color) in faults.items():
            with self.subTest(case=case):
                actual = self.fixture(case)
                actual.paste(color, rectangle)
                path = self.root / (case + '.png')
                actual.save(path)
                result = report.comparison(path, self.references / (case + '.png'), self.root / 'diff.png', case)
                self.assertEqual('fail', result['status'])

    def test_cubemap_reference_matches_expected_cross(self):
        path = Path(report.__file__).with_name('graphics_reference') / 'cubemap.png'
        difference = report.likeness.ImageChops.difference(cubemap_fixture(), report.read_png(path))
        self.assertIsNone(difference.getbbox(), 'Cubemap faces, labels or orientation differ from the expected cross')

    def test_cubemap_missing_swapped_and_mirrored_faces_fail(self):
        expected = cubemap_fixture()
        right_face = (128, 104, 176, 152)
        face = expected.crop(right_face)
        transpose = report.likeness.Image.Transpose
        for fault in ('missing', 'swapped', 'rotated', 'mirrored_x', 'mirrored_y'):
            with self.subTest(fault=fault):
                actual = expected.copy()
                if fault == 'missing':
                    actual.paste(report.BACKGROUND, right_face)
                elif fault == 'swapped':
                    left_face = (32, 104, 80, 152)
                    actual.paste(expected.crop(left_face), right_face)
                    actual.paste(face, left_face)
                else:
                    operation = {'rotated': transpose.ROTATE_90, 'mirrored_x': transpose.FLIP_LEFT_RIGHT,
                                 'mirrored_y': transpose.FLIP_TOP_BOTTOM}[fault]
                    actual.paste(face.transpose(operation), right_face)
                path = self.root / 'cubemap-fault.png'
                actual.save(path)
                result = report.comparison(path, self.references / 'cubemap.png', self.root / 'diff.png', 'cubemap')
                self.assertEqual('fail', result['status'])

    def test_missing_corrupt_dimensions_and_alpha(self):
        self.add_backend('metal')
        for case, damage in zip(report.CASES, ('missing', 'corrupt', 'dimensions')):
            path = self.images / 'metal' / (case + '.png')
            if damage == 'missing':
                path.unlink()
            elif damage == 'corrupt':
                path.write_bytes(b'not a PNG')
            else:
                report.likeness.Image.new('RGB', (128, 256)).save(path)
        result = self.make_report()
        self.assertEqual(3, result['counts']['fail'])
        transparent = self.root / 'transparent.png'
        report.likeness.Image.new('RGBA', report.SIZE).save(transparent)
        with self.assertRaisesRegex(ValueError, 'opaque'):
            report.read_png(transparent)

    def test_missing_reference_fails_but_report_finishes(self):
        self.add_backend('metal')
        (self.references / 'triangle.png').unlink()
        result = self.make_report()
        self.assertEqual(1, result['counts']['fail'])
        self.assertEqual(len(report.CASES), len([r for r in result['comparisons'] if r['kind'] == 'reference']))
        self.assertTrue((self.output / 'results.json').exists())
        page = (self.output / 'index.html').read_text(encoding='utf-8')
        self.assertIn('data:image/png;base64,', page)
        self.assertIn('Rebuild this report', page)
        self.assertNotIn('src="http', page)

    def test_combines_captures_from_different_systems(self):
        self.add_backend('metal')
        other = self.root / 'windows'
        self.records = []
        self.add_backend('dx12', root=other)
        result = self.make_report([self.images, other])
        self.assertEqual(0, result['counts']['fail'])
        self.assertEqual(2 * len(report.CASES), len(result['comparisons']))
        self.assertEqual({'metal', 'dx12'}, {r['backend'] for r in result['comparisons']})
        self.assertTrue(all(r['kind'] == 'reference' and r['likeness_percent'] == 100 for r in result['comparisons']))
        self.assertEqual(2 * len(report.CASES), result['counts']['pass'])

    def test_counts_final_case_outcomes_once(self):
        self.add_backend('metal')
        self.assertEqual({'pass': len(report.CASES), 'fail': 0, 'skip': 0}, self.make_report()['counts'])
        report.likeness.Image.new('RGB', report.SIZE, (255, 0, 0)).save(self.images / 'metal/clear.png')
        result = self.make_report()
        self.assertEqual({'pass': len(report.CASES) - 1, 'fail': 1, 'skip': 0}, result['counts'])
        self.assertEqual('pass', result['captures'][0]['status'])
        self.assertEqual('fail', result['captures'][0]['case_status'])

    def test_failed_repeated_render_preserves_images_and_difference(self):
        self.add_backend('metal')
        record = self.records[2]
        record.update(status='fail', exit_code=1, reason='Capture process exited with 1',
                      log='ERROR:GRAPHICS: Repeated render changed')
        (self.images / 'captures.json').write_text(json.dumps(self.records))
        original = self.images / 'metal/stencil.png'
        repeated = report.diagnostic_path(original, 'repeated')
        image = report.read_png(original)
        # A tiny difference still fails the exact continuation requirement.
        image.putpixel((0, 0), (38, 73, 109))
        image.save(repeated)
        result = self.make_report()
        self.assertEqual({'pass': len(report.CASES) - 1, 'fail': 1, 'skip': 0}, result['counts'])
        checks = [check for check in result['comparisons'] if check['case'] == 'stencil']
        self.assertEqual(['reference', 'repeated'], [check['kind'] for check in checks])
        self.assertEqual(['pass', 'fail'], [check['status'] for check in checks])
        self.assertGreater(checks[1]['likeness_percent'], 99)
        self.assertTrue(Path(checks[1]['difference']).is_file())
        page = (self.output / 'index.html').read_text(encoding='utf-8')
        for path in (original, repeated, Path(checks[1]['difference'])):
            self.assertIn(report.base64.b64encode(path.read_bytes()).decode(), page)
        self.assertIn('FAIL · likeness 100.00000%', page)
        self.assertIn('First render', page)
        self.assertIn('Repeated render', page)

    def test_continuation_diagnostics_relocate_and_count_once(self):
        self.add_backend('metal')
        original = self.images / 'metal/triangle.png'
        for kind in ('viewport', 'depth-stencil'):
            self.fixture('triangle').save(report.diagnostic_path(original, kind + '-expected'))
            self.fixture('clear').save(report.diagnostic_path(original, kind + '-actual'))
        moved = self.root / 'moved'
        self.images.rename(moved)
        result = self.make_report([moved])
        self.assertEqual({'pass': len(report.CASES) - 1, 'fail': 1, 'skip': 0}, result['counts'])
        diagnostics = [check for check in result['comparisons'] if check['kind'] != 'reference']
        self.assertEqual(2, len(diagnostics))
        self.assertTrue(all(check['status'] == 'fail' and str(moved) in check['actual'] for check in diagnostics))
        report.diagnostic_path(moved / 'metal/triangle.png', 'viewport-actual').unlink()
        result = self.make_report([moved])
        self.assertEqual(1, result['counts']['fail'])
        self.assertIn('No After readback image'.lower(), (self.output / 'index.html').read_text(encoding='utf-8').lower())

    def test_saved_backend_mismatch_and_graphics_error(self):
        self.add_backend('metal')
        self.records[0]['actual_backend'] = 'opengl'
        self.records[1]['log'] = 'ERROR:GRAPHICS: validation failed'
        (self.images / 'captures.json').write_text(json.dumps(self.records))
        self.assertEqual(2, self.make_report()['counts']['fail'])

    def test_empty_and_truncated_manifest_fail(self):
        (self.images / 'captures.json').write_text('[]')
        self.assertEqual(1, self.make_report()['counts']['fail'])
        self.add_backend('metal')
        (self.images / 'captures.json').write_text(json.dumps(self.records[:1]))
        self.assertEqual(len(report.CASES) - 1, self.make_report()['counts']['fail'])

    def test_capture_failure_removes_stale_output(self):
        self.add_backend('metal')
        output = self.images / 'metal/clear.png'
        diagnostics = [report.diagnostic_path(output, suffix)
                       for _, actual, expected, _, _ in report.DIAGNOSTICS for suffix in (actual, expected) if suffix]
        for path in diagnostics:
            self.fixture('clear').save(path)
        result = report.capture(self.root / 'missing-executable', self.images, 'metal', 'clear')
        self.assertEqual('fail', result['status'])
        self.assertFalse(output.exists())
        self.assertTrue(all(not path.exists() for path in diagnostics))

    def test_crash_timeout_identity_and_graphics_errors(self):
        outcomes = (
            subprocess.CompletedProcess([], -11, 'crashed'),
            subprocess.TimeoutExpired([], 60, output=b'partial log'),
            subprocess.CompletedProcess([], 0, 'GRAPHICS_CAPTURE_BACKEND=opengl\n'),
            subprocess.CompletedProcess([], 0, 'GRAPHICS_CAPTURE_BACKEND=metal\nERROR:GRAPHICS: invalid state'),
        )
        for outcome in outcomes:
            with self.subTest(outcome=outcome):
                effect = outcome if isinstance(outcome, Exception) else None
                with mock.patch.object(report.subprocess, 'run', side_effect=effect, return_value=outcome):
                    result = report.capture(Path('capture'), self.images, 'metal', 'clear')
                self.assertEqual('fail', result['status'])
                self.assertTrue(result['reason'])
                self.assertTrue((self.images / 'metal/clear.json').is_file())
        with mock.patch.object(report.subprocess, 'run', side_effect=subprocess.TimeoutExpired([], 60, output=b'partial')):
            self.assertIn('partial', report.capture(Path('capture'), self.images, 'metal', 'clear')['log'])

    def test_matrix_continues_after_failure(self):
        def fake_capture(executable, root, backend, case):
            return dict(backend=backend, case=case, status='fail', reason='crashed')
        with mock.patch.object(report, 'capture', side_effect=fake_capture) as capture:
            records = report.run_matrix(Path('capture'), self.images, ['metal', 'vulkan'], ['metal', 'vulkan'])
        self.assertEqual(2 * len(report.CASES), capture.call_count)
        self.assertEqual(2 * len(report.CASES), len(records))
        self.assertEqual(2 * len(report.CASES), self.make_report()['counts']['fail'])

    def test_skip_accounting_and_explicit_unavailable(self):
        self.add_backend('metal')
        diagnostic = report.diagnostic_path(self.images / 'metal/clear.png', 'repeated')
        self.fixture('clear').save(diagnostic)
        records = report.run_matrix(Path('capture'), self.images, ['metal'], [])
        self.assertTrue(all(r['status'] == 'skip' for r in records))
        self.assertEqual(len(report.CASES), self.make_report()['counts']['skip'])
        self.assertFalse((self.images / 'metal/clear.png').exists())
        self.assertFalse(diagnostic.exists())
        records = report.run_matrix(Path('capture'), self.images, ['metal'], [], explicit=True)
        self.assertTrue(all(r['status'] == 'fail' for r in records))
        records = report.run_matrix(Path('capture'), self.images, ['metal'], ['metal'], skip_all='Hosted CI policy')
        self.assertTrue(all(r['reason'] == 'Hosted CI policy' for r in records))

    def test_report_escapes_logs(self):
        self.add_backend('metal')
        self.records[0]['log'] = '<script>alert(1)</script>'
        (self.images / 'captures.json').write_text(json.dumps(self.records))
        self.make_report()
        page = (self.output / 'index.html').read_text(encoding='utf-8')
        self.assertNotIn('<script>', page)
        self.assertIn('&lt;script&gt;', page)

    @unittest.skipUnless(os.environ.get('GRAPHICS_CAPTURE_EXECUTABLE'), 'Capture executable not supplied')
    def test_capture_cli(self):
        executable = os.environ['GRAPHICS_CAPTURE_EXECUTABLE']
        process = subprocess.run([executable, '--list-cases'], capture_output=True, text=True, timeout=60)
        self.assertEqual(0, process.returncode)
        names = [line for line in process.stdout.splitlines() if not line.startswith('INFO:DLIB:')]
        self.assertEqual(list(report.CASES), names)
        for args in (
            ['--case'], ['--case', 'missing', '--backend', 'metal'],
            ['--case', 'clear', '--backend', 'unknown'],
            ['--case', 'clear', '--backend', 'metal', '--output', 'a', '--output-file', 'b'],
            ['--list-cases', '--case', 'clear'], ['--backend', 'metal', '--backend', 'vulkan'],
        ):
            with self.subTest(args=args):
                process = subprocess.run([executable, *args], capture_output=True, text=True, timeout=60)
                self.assertNotEqual(0, process.returncode)


if __name__ == '__main__':
    unittest.main()
