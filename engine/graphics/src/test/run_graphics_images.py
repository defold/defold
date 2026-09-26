#!/usr/bin/env python3
"""Capture deterministic graphics cases or rebuild a report from saved captures."""
import argparse
import base64
import html
import itertools
import json
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'scripts'))
import likeness

TEXTURE_FORMATS = tuple(re.findall(r'^TEXTURE_FORMAT_CASE\((\w+),',
    Path(__file__).with_name('texture_formats').joinpath('formats.inc').read_text(encoding='utf-8'), re.MULTILINE))
TEXTURE_CASES = tuple('texture_' + suffix for suffix in TEXTURE_FORMATS)

CASES = ('clear', 'triangle', 'stencil', 'stencil_nested', 'stencil_masks',
         'stencil_ops', 'stencil_depth', 'stencil_faces', 'cubemap') + TEXTURE_CASES
CASE_DESCRIPTIONS = {
    'clear': 'Whole-target color clear.',
    'triangle': 'Asymmetric triangle with interpolated vertex colors.',
    'cubemap': 'Six uploaded RGBA cubemap faces sampled by direction into a cross: +Y above; -X, +Z, +X, -Z across; -Y below. Axis labels, white upper-left markers and dark lower-right markers expose face swaps, rotations and mirroring. Nearest filtering, mip level 0.',
    'stencil': 'Basic mask: REPLACE writes 1, then EQUAL clips a larger orange rectangle.',
    'stencil_nested': 'Three nested levels: orange outer mask, green child, blue grandchild. Children extend beyond their parents and must be clipped.',
    'stencil_masks': 'Overlapping low/high-nibble writes preserve unselected bits. Exact values produce yellow, orange, green and blue regions; magenta/cyan strips check masked reads. A zero write mask must block REPLACE.',
    'stencil_ops': 'Nine green tiles, row-major: ZERO, REPLACE, INCR, INCR at 255 (clamp), DECR, DECR at 0 (clamp), INVERT, INCR_WRAP at 255, DECR_WRAP at 0. Missing tiles identify incorrect results.',
    'stencil_depth': 'An invisible depth occluder crosses three bands. Front band: orange. Rear band: green with orange center from depth-failure INCR. Bottom band: blue from stencil-failure INVERT, including where depth also fails.',
    'stencil_faces': 'Opposite windings: top orange/cyan tiles check separate front/back operations; bottom magenta/green tiles check separate EQUAL/NOTEQUAL comparisons. Results are read with common face state to expose swapped faces.',
}
CASE_DESCRIPTIONS.update({case: 'Labelled 8×8 grid uploaded in native %s format, sampled at mip 0 with nearest filtering. Unsupported formats are skipped before upload.' % case.removeprefix('texture_') for case in TEXTURE_CASES})
REFERENCE_BACKENDS = {**dict.fromkeys(TEXTURE_CASES, 'CPU decoded texture'), 'stencil_faces': 'OpenGL', 'texture_rgba': 'Source image'}
BACKENDS = ('metal', 'opengl', 'webgpu', 'vulkan', 'dx12')
BACKGROUND = (37, 73, 109)
THRESHOLD = 99.0
SIZE = (256, 256)
GRAPHICS_ERROR = re.compile(r'(?:ERROR|FATAL):|Validation Error|VUID-', re.IGNORECASE)
DIAGNOSTICS = (
    ('repeated', 'repeated', None, 'Repeated render', 'First render'),
    ('viewport', 'viewport-actual', 'viewport-expected', 'After readback', 'Without readback'),
    ('depth-stencil', 'depth-stencil-actual', 'depth-stencil-expected', 'After readback', 'Without readback'),
)

# Match the font rendering report's palette, cards and comparison layout.
STYLE = """
:root { color-scheme: dark; font-family: system-ui, sans-serif; }
body { margin: 2rem auto; padding: 0 1rem; max-width: 1500px; background: #141820; color: #e8ebf0; }
a { color: #92c5ff; } h1,h2,h3 { line-height: 1.3; overflow-wrap: anywhere; }
.counts,.links { display: flex; flex-wrap: wrap; gap: 1rem; margin: 1rem 0; }
.count,article { background: #202733; border: 1px solid #384457; border-radius: .6rem; padding: 1rem; }
article { margin: 1rem 0; } .pass { color: #86e3a4; } .fail,.error { color: #ff9b9b; } .skip { color: #e9cd83; }
.images { display: grid; grid-template-columns: repeat(3,minmax(0,1fr)); gap: 1rem; }
figure { margin: 0; min-width: 0; } img { width: 100%; object-fit: contain; background: #202020; image-rendering: pixelated; }
figcaption { margin-top: .35rem; color: #bac5d5; } .missing { min-height: 6rem; padding: 1rem; border: 1px dashed #566479; }
pre { white-space: pre-wrap; overflow-wrap: anywhere; background: #141820; padding: 1rem; }
summary { cursor: pointer; padding: .5rem 0; } .reason { overflow-wrap: anywhere; }
@media(max-width: 700px) { .images { grid-template-columns: 1fr; } }
"""


def passed(score):
    return score >= THRESHOLD


def read_png(path):
    with likeness.Image.open(path) as image:
        if image.format != 'PNG' or image.size != SIZE:
            raise ValueError('Expected a 256x256 PNG: %s' % path)
        image.load()
        rgba = image.convert('RGBA')
        if rgba.getchannel('A').getextrema() != (255, 255):
            raise ValueError('Capture must be fully opaque: %s' % path)
        return rgba.convert('RGB')


def diagnostic_path(output, suffix):
    return Path(str(output) + '.' + suffix + '.png') if suffix else output


def remove_capture_images(output):
    output.unlink(missing_ok=True)
    for _, actual, expected, _, _ in DIAGNOSTICS:
        for suffix in (actual, expected):
            if suffix:
                diagnostic_path(output, suffix).unlink(missing_ok=True)


def capture(executable, root, backend, case, timeout=60):
    """One fresh process and output file per case, including failed runs."""
    output = root / backend / (case + '.png')
    output.parent.mkdir(parents=True, exist_ok=True)
    remove_capture_images(output)
    command = [str(executable), '--backend', backend, '--case', case, '--output-file', str(output)]
    record = dict(backend=backend, case=case, status='fail', command=command,
                  platform=platform.platform(), machine=platform.machine(), actual_backend=None)
    start = time.monotonic()
    try:
        process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 timeout=timeout, text=True, errors='replace')
        record.update(exit_code=process.returncode, log=process.stdout)
        identities = re.findall(r'^GRAPHICS_CAPTURE_BACKEND=(\w+)$', process.stdout, re.MULTILINE)
        if identities == [backend]:
            record['actual_backend'] = backend
        if process.returncode not in (0, 77):
            raise ValueError('Capture process exited with %s' % process.returncode)
        if record['actual_backend'] != backend:
            raise ValueError('Requested backend identity was not confirmed')
        if GRAPHICS_ERROR.search(process.stdout):
            raise ValueError('Graphics error in capture log')
        skips = re.findall(r'^GRAPHICS_CAPTURE_SKIP=(.+)$', process.stdout, re.MULTILINE)
        if process.returncode == 77 and case in TEXTURE_CASES and len(skips) == 1:
            record.update(status='skip', reason=skips[0])
        elif process.returncode:
            raise ValueError('Capture process exited with %s' % process.returncode)
        else:
            read_png(output)
            record['status'] = 'pass'
    except subprocess.TimeoutExpired as error:
        log = error.stdout or ''
        record.update(log=log.decode(errors='replace') if isinstance(log, bytes) else log,
                      reason='Capture timed out after %s seconds' % timeout)
    except (OSError, ValueError) as error:
        record['reason'] = str(error)
    record['seconds'] = time.monotonic() - start
    output.with_suffix('.log').write_text(record.get('log', ''), encoding='utf-8')
    output.with_suffix('.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
    return record


def run_matrix(executable, root, matrix, available, explicit=False, skip_all=None):
    root.mkdir(parents=True, exist_ok=True)
    (root / 'captures.json').unlink(missing_ok=True)
    records = []
    for backend in matrix:
        for case in CASES:
            if skip_all or backend not in available:
                record = dict(backend=backend, case=case, status='fail' if explicit and not skip_all else 'skip',
                              reason=skip_all or '%s adapter omitted from this build/configuration' % backend)
                # A skipped/failed availability check must not reuse an old PNG.
                path = root / backend / (case + '.png')
                path.parent.mkdir(parents=True, exist_ok=True)
                remove_capture_images(path)
                path.with_suffix('.log').write_text('', encoding='utf-8')
                path.with_suffix('.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
            else:
                record = capture(executable, root, backend, case)
            records.append(record)
    (root / 'captures.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    return records


def load_captures(roots):
    records = []
    for index, root in enumerate(roots):
        try:
            saved = json.loads((root / 'captures.json').read_text(encoding='utf-8'))
            if not isinstance(saved, list) or not saved:
                raise ValueError('Empty or invalid capture manifest')
        except (OSError, ValueError) as error:
            records.append(dict(id=str(index), status='fail', reason=str(error), backend='unknown', case='unknown'))
            continue
        seen = set()
        for original in saved:
            record = dict(original) if isinstance(original, dict) else {}
            record.pop('image', None)
            backend, case = record.get('backend'), record.get('case')
            record['id'] = '%s-%s-%s' % (index, backend, case)
            if backend not in BACKENDS or case not in CASES or (backend, case) in seen:
                record.update(status='fail', reason='Invalid or duplicate manifest entry')
            else:
                seen.add((backend, case))
                record['image'] = str(root / backend / (case + '.png'))
                if record.get('status') == 'pass':
                    try:
                        if record.get('actual_backend') != backend or record.get('exit_code') != 0:
                            raise ValueError('Saved capture lacks successful backend identity/provenance')
                        if GRAPHICS_ERROR.search(record.get('log', '')):
                            raise ValueError('Graphics error in saved capture log')
                        read_png(Path(record['image']))
                    except (OSError, ValueError) as error:
                        record.update(status='fail', reason=str(error))
                elif record.get('status') not in ('fail', 'skip'):
                    record.update(status='fail', reason='Invalid capture status')
            records.append(record)
        # Missing cases are failures even when a manifest was truncated.
        for backend in sorted({b for b, _ in seen}):
            for case in CASES:
                if (backend, case) not in seen:
                    records.append(dict(id='%s-%s-%s' % (index, backend, case), backend=backend,
                                        case=case, status='fail', reason='Missing case in saved manifest'))
    return records


def comparison(actual, reference, output, case, exact=False, **metadata):
    result = dict(status='fail', case=case, actual=str(actual), reference=str(reference), exact_pixels=exact, **metadata)
    try:
        actual_image, reference_image = read_png(actual), read_png(reference)
        metrics = likeness.compare(actual_image, reference_image, output,
                                   background=BACKGROUND, foreground=not exact and case != 'clear')
        result.update(metrics, difference=str(output))
        matches = actual_image.tobytes() == reference_image.tobytes() if exact else passed(metrics['likeness_percent'])
        result['status'] = 'pass' if matches else 'fail'
        if result['status'] == 'fail':
            result['reason'] = 'Pixels changed across readback' if exact else 'Likeness below 99%'
    except (OSError, ValueError) as error:
        result['reason'] = str(error)
    return result


def image_html(label, path):
    caption = '<figcaption>%s</figcaption></figure>' % html.escape(label)
    if not path or not Path(path).is_file():
        return '<figure><div class="missing">No %s image</div>%s' % (html.escape(label.lower()), caption)
    try:
        read_png(Path(path))
    except (OSError, ValueError):
        return '<figure><div class="error">Invalid PNG</div>' + caption
    data = base64.b64encode(Path(path).read_bytes()).decode('ascii')
    return '<figure><img loading="lazy" src="data:image/png;base64,%s" alt="%s">%s' % (data, html.escape(label), caption)


def make_report(roots, references, output):
    output.mkdir(parents=True, exist_ok=True)
    differences = output / 'differences'
    differences.mkdir(exist_ok=True)
    for stale in differences.glob('*.png'):
        stale.unlink()
    captures = load_captures(roots)
    comparisons = []
    for record in captures:
        checks = []
        if record['status'] != 'skip' and record.get('image'):
            case = record['case']
            actual = Path(record['image'])
            # Failed processes can still leave useful evidence. Their case
            # remains failed even when the first image matches its reference.
            if actual.is_file():
                checks.append(comparison(actual, references / (case + '.png'),
                    differences / ('reference-' + record['id'] + '.png'), case,
                    kind='reference', backend=record['backend'], capture=record['id'],
                    reference_backend=REFERENCE_BACKENDS.get(case, 'Metal'),
                    actual_label='Actual · ' + record['backend'],
                    reference_label=REFERENCE_BACKENDS.get(case, 'Metal') + ' reference'))
            for kind, actual_suffix, expected_suffix, actual_label, reference_label in DIAGNOSTICS:
                diagnostic = diagnostic_path(actual, actual_suffix)
                expected = diagnostic_path(actual, expected_suffix)
                if diagnostic.is_file() or (expected_suffix and expected.is_file()):
                    checks.append(comparison(diagnostic, expected,
                        differences / (kind + '-' + record['id'] + '.png'), case, exact=True,
                        kind=kind, backend=record['backend'], capture=record['id'],
                        actual_label=actual_label, reference_label=reference_label))
        record['case_status'] = 'fail' if any(check['status'] == 'fail' for check in checks) else record['status']
        reasons = ([record['reason']] if record.get('reason') else [])
        reasons.extend(check['reason'] for check in checks if check.get('reason'))
        record['case_reason'] = '; '.join(dict.fromkeys(reasons))
        comparisons.extend(checks)
    counts = {status: sum(record['case_status'] == status for record in captures)
              for status in ('pass', 'fail', 'skip')}
    report = dict(threshold=THRESHOLD, dimensions=SIZE, captures=captures, comparisons=comparisons, counts=counts,
                  reproduction=[sys.executable, str(Path(__file__).resolve()),
                      *itertools.chain.from_iterable(('--images', str(root)) for root in roots),
                      '--references', str(references), '--output', str(output)])
    (output / 'results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    esc = lambda value: html.escape(str(value))
    status = 'fail' if counts['fail'] else 'pass' if counts['pass'] else 'skip'
    parts = ['<!doctype html><html lang="en"><head><meta charset="utf-8">',
             '<meta name="viewport" content="width=device-width,initial-scale=1"><title>Graphics likeness tests</title>',
             '<style>' + STYLE + '</style></head><body><h1>Graphics likeness tests</h1>',
             '<p class="%s">%s</p><div class="counts">' % (status, status.upper())]
    for key, label in (('pass', 'Passed'), ('fail', 'Failed'), ('skip', 'Skipped')):
        parts.append('<div class="count">%s: <strong>%s</strong></div>' % (label, counts[key]))
    data = base64.b64encode((output / 'results.json').read_bytes()).decode('ascii')
    parts.extend(['</div><div class="links"><a download="results.json" href="data:application/json;base64,%s">All results (JSON)</a></div>' % data,
                  '<p>Each backend/case is counted once. Required: ≥99% RGB likeness. Clear: whole image. All other cases: foreground union. Readback diagnostics require identical pixels. No alignment, resizing or color correction.</p>'])
    incomplete = [record for record in captures if record['case_status'] != 'pass']
    section_by_capture = {}
    for index, result in enumerate(comparisons):
        previous = section_by_capture.get(result['capture'])
        if previous is None or (comparisons[previous]['status'] != 'fail' and result['status'] == 'fail'):
            section_by_capture[result['capture']] = index
    if incomplete:
        parts.append('<details%s><summary>Failed and skipped cases</summary><ul>' % (' open' if counts['fail'] else ''))
        for record in incomplete:
            label = '%s / %s' % (esc(record.get('backend', 'unknown')), esc(record.get('case', 'unknown')))
            section = section_by_capture.get(record['id'])
            if section is not None:
                label = '<a href="#comparison-%s">%s</a>' % (section, label)
            parts.append('<li class="%s">%s: %s — %s' %
                         (record['case_status'], label,
                          record['case_status'].upper(), esc(record['case_reason'])))
            if record.get('log'):
                parts.append('<details><summary>Process log and reproduction</summary><pre>%s</pre><pre>%s</pre></details>' %
                             (esc(shlex.join(record.get('command', []))), esc(record['log'])))
            parts.append('</li>')
        parts.append('</ul></details>')
    capture_by_id = {record['id']: record for record in captures}
    parts.append('<h2>Comparisons</h2>')
    for index, result in enumerate(comparisons):
        record = capture_by_id[result['capture']]
        score = '%.5f%%' % result['likeness_percent'] if 'likeness_percent' in result else 'unavailable'
        reference_label = result['reference_label']
        title = ' · '.join((result['case'], result['kind'].title(), result['backend']))
        description = CASE_DESCRIPTIONS[result['case']] if result['kind'] == 'reference' else (
            'Repeated render must match the first image exactly.' if result['kind'] == 'repeated' else
            'Draws with retained %s state must match the uninterrupted sequence exactly.' % result['kind'])
        card_status = record['case_status'] if result['kind'] == 'reference' else result['status']
        reason = record['case_reason'] if result['kind'] == 'reference' else result.get('reason', '')
        parts.append('<article id="comparison-%s"><h3><a href="#comparison-%s">%s</a></h3><p>%s</p><p class="%s">%s · likeness %s</p><p class="reason">%s</p><div class="images">%s%s%s</div>' %
                     (index, index, esc(title), esc(description), card_status, card_status.upper(), score, esc(reason),
                      image_html(result['actual_label'], result.get('actual')),
                      image_html(reference_label, result.get('reference')),
                      image_html('Difference (contrast ×4)', result.get('difference'))))
        details = dict(comparison=result, captures=[capture_by_id[result['capture']]])
        parts.append('<details><summary>Configuration, comparison and logs</summary><pre>%s</pre></details></article>' %
                     esc(json.dumps(details, indent=2)))
    parts.extend(['<details><summary>Rebuild this report</summary><pre>' + esc(shlex.join(report['reproduction'])) + '</pre></details>',
                  '<details><summary>Full JSON results</summary><pre>' + esc(json.dumps(report, indent=2)) + '</pre></details></body></html>'])
    (output / 'index.html').write_text('\n'.join(parts), encoding='utf-8')
    print('Graphics likeness: %s; report: %s' % (counts, output / 'index.html'))
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--capture-dir', type=Path)
    parser.add_argument('--images', type=Path, action='append', default=[])
    parser.add_argument('--matrix', choices=BACKENDS, nargs='+', default=list(BACKENDS))
    parser.add_argument('--available', choices=BACKENDS, nargs='*', default=[])
    parser.add_argument('--backend', choices=BACKENDS, action='append', help='Explicit backend request; unavailable is a failure')
    parser.add_argument('--skip-all', help='CMake hosted-CI policy skip reason')
    parser.add_argument('--references', type=Path, default=Path(__file__).with_name('graphics_reference'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    if args.executable:
        if not args.capture_dir or args.images:
            parser.error('--executable requires --capture-dir and cannot be combined with --images')
        run_matrix(args.executable.resolve(), args.capture_dir.resolve(), args.backend or args.matrix,
                   args.available, explicit=bool(args.backend), skip_all=args.skip_all)
        args.images = [args.capture_dir]
    elif not args.images:
        parser.error('Use --executable with --capture-dir, or one or more --images directories')
    report = make_report([root.resolve() for root in args.images], args.references.resolve(), args.output.resolve())
    return 1 if report['counts']['fail'] else 0


if __name__ == '__main__':
    sys.exit(main())
