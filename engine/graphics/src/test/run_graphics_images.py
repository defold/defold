#!/usr/bin/env python3
"""Capture deterministic graphics cases or rebuild a report from saved captures."""
import argparse
from contextlib import nullcontext
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
CATEGORIES = (
    ('rendering', 'Basic rendering', 'Color clears and interpolated triangles.', ('clear', 'triangle')),
    ('stencil', 'Stencil', 'Masks, nesting, operations, depth and face state.',
     tuple(case for case in CASES if case.startswith('stencil'))),
    ('cubemaps', 'Cubemaps', 'Face selection, orientation and sampling.', ('cubemap',)),
    ('textures', 'Texture formats', 'Native RGBA, BC, ETC/EAC, PVRTC and ASTC textures.', TEXTURE_CASES),
    ('other', 'Other results', 'Capture and manifest errors.', ()),
)

# Match the font rendering report's palette, cards and comparison layout.
STYLE = """
:root { color-scheme: dark; font-family: system-ui, sans-serif; }
* { box-sizing: border-box; }
body { margin: 2.5rem auto; padding: 0 1.5rem; max-width: 1440px; background: #141820; color: #e8ebf0; line-height: 1.5; }
a { color: #92c5ff; text-underline-offset: .2em; } h1,h2,h3 { line-height: 1.25; overflow-wrap: anywhere; }
h1 { font-size: clamp(1.8rem,4vw,2.5rem); margin: .25rem 0; letter-spacing: -.035em; }
h2 { font-size: 1.25rem; margin: 0; } h3 { font-size: 1rem; margin: 0; }
p { margin: .6rem 0; } .muted,figcaption { color: #aebbd0; }
.eyebrow { color: #aebbd0; font-size: .75rem; letter-spacing: .12em; text-transform: uppercase; }
.title-row,.section-heading { display: flex; align-items: center; justify-content: space-between; gap: 1rem; flex-wrap: wrap; }
.section-heading { margin: 2rem 0 1rem; } .section-heading p { margin: 0; }
.badge { border: 1px solid currentColor; border-radius: 2rem; padding: .25rem .7rem; font-size: .8rem; font-weight: 600; }
.counts { display: grid; grid-template-columns: repeat(4,minmax(0,1fr)); gap: .75rem; margin: 1.5rem 0; }
.count,.platform,.category,.failure-list { background: #202733; border: 1px solid #384457; border-radius: .65rem; padding: 1.1rem; }
.count strong { display: block; font-size: 2rem; line-height: 1.2; font-variant-numeric: tabular-nums; }
.count span { display: block; margin-top: .35rem; font-size: .85rem; color: #aebbd0; }
.platforms { display: grid; grid-template-columns: repeat(auto-fit,minmax(min(100%,480px),1fr)); gap: 1rem; }
.platform h2 { font-size: 1rem; } .platform-name { overflow-wrap: anywhere; }
table { width: 100%; border-collapse: collapse; margin-top: .75rem; font-size: .9rem; font-variant-numeric: tabular-nums; }
th,td { padding: .5rem .65rem; border-top: 1px solid #384457; text-align: right; }
th:first-child,td:first-child { text-align: left; padding-left: 0; } thead th { color: #aebbd0; font-weight: 500; }
.categories { display: grid; grid-template-columns: repeat(4,minmax(0,1fr)); gap: 1rem; }
.category { display: flex; flex-direction: column; gap: .65rem; color: #e8ebf0; text-decoration: none; }
.category:hover { border-color: #92c5ff; background: #263142; }
.category-link { color: #92c5ff; font-size: .85rem; } .category:hover .category-link { text-decoration: underline; }
.category p { font-size: .85rem; margin: 0; } .category .description { flex: 1; }
.totals { display: flex; flex-wrap: wrap; gap: .35rem .8rem; font-size: .85rem; }
.result-bar { display: flex; height: .3rem; gap: 2px; border-radius: 1rem; overflow: hidden; background: #384457; }
.result-bar .pass { background: #86e3a4; } .result-bar .fail { background: #ff9b9b; } .result-bar .skip { background: #e9cd83; }
.pass { color: #86e3a4; } .fail,.error { color: #ff9b9b; } .skip { color: #e9cd83; }
.failure-list { border-color: #75494d; padding: .25rem 1.1rem; }
.failure-list li { padding: .75rem 0; } .failure-list li + li { border-top: 1px solid #384457; }
.failure-list ul { list-style: none; margin: 0; padding: 0; } .failure-list p { margin: .2rem 0 0; font-size: .85rem; }
.skipped { margin-top: 1rem; } .skipped li { margin: .5rem 0; }
.results-toolbar { display: flex; align-items: center; justify-content: space-between; gap: 1rem; flex-wrap: wrap; margin: 2rem 0 1rem; }
.results-toolbar fieldset { border: 1px solid #384457; border-radius: .5rem; margin: 0; padding: .5rem .8rem; }
.results-toolbar fieldset[hidden] { display: none; }
.results-toolbar legend { color: #aebbd0; font-size: .8rem; padding: 0 .3rem; }
.results-toolbar label { cursor: pointer; margin-right: .8rem; white-space: nowrap; }
.results-toolbar input { accent-color: #92c5ff; }
.result-group { border-left: 2px solid #384457; padding-left: 1rem; margin: 1.5rem 0; }
.result-group > .title-row { margin-bottom: .6rem; }
.comparison { margin: .6rem 0; border: 1px solid #384457; border-radius: .6rem; background: #202733; }
.comparison > summary { padding: .9rem 1rem; }
.comparison[open] > summary { border-bottom: 1px solid #384457; }
.comparison-title { font-weight: 600; overflow-wrap: anywhere; } .backend { color: #aebbd0; font-size: .85rem; margin-left: .75rem; }
.comparison-score { float: right; margin-left: 1rem; font-size: .85rem; font-variant-numeric: tabular-nums; }
article { padding: 1rem; } article > p:first-child { margin-top: 0; }
.images { display: grid; grid-template-columns: repeat(3,minmax(0,1fr)); gap: 1rem; }
figure { margin: 0; min-width: 0; } img { display: block; width: 100%; object-fit: contain; background: #202020; image-rendering: pixelated; }
figcaption { margin-top: .35rem; font-size: .85rem; } .missing { min-height: 6rem; padding: 1rem; border: 1px dashed #566479; }
pre { white-space: pre-wrap; overflow-wrap: anywhere; background: #141820; padding: 1rem; font-size: .8rem; }
summary { cursor: pointer; padding: .5rem 0; } .reason { overflow-wrap: anywhere; }
:target { scroll-margin-top: 1rem; } .comparison:target { border-color: #92c5ff; box-shadow: 0 0 0 1px #92c5ff; }
a:focus-visible,summary:focus-visible { outline: 2px solid #92c5ff; outline-offset: 4px; }
footer { border-top: 1px solid #384457; margin-top: 2rem; padding-top: 1rem; }
footer .links { margin: .5rem 0 1rem; } .back { font-size: .8rem; }
@media(max-width: 1000px) { .categories { grid-template-columns: repeat(2,minmax(0,1fr)); } }
@media(max-width: 700px) {
    body { margin: 1.5rem auto; padding: 0 1rem; }
    .counts,.categories { grid-template-columns: repeat(2,minmax(0,1fr)); } .images { grid-template-columns: 1fr; }
    .comparison-score { float: none; display: block; margin: .3rem 0 0 1rem; }
    th,td { padding: .5rem .25rem; }
}
@media(max-width: 420px) { .categories { grid-template-columns: 1fr; } }
"""

RESULT_GROUPING_SCRIPT = """
(() => {
    const container = document.getElementById('test-results');
    const controls = document.getElementById('results-grouping');
    const results = Array.from(container.querySelectorAll('.test-result'));
    const categories = Array.from(document.querySelectorAll('a.category')).map(link => ({
        key: link.dataset.category, title: link.querySelector('h3').textContent
    }));
    const backends = Array.from(new Set(results.map(result => result.dataset.backend)));
    let currentMode = 'backend';

    function section(id, title, records, level) {
        const node = document.createElement('section');
        node.id = id;
        node.className = level === 2 ? 'result-section' : 'result-group';
        const header = document.createElement('div');
        header.className = level === 2 ? 'section-heading' : 'title-row';
        const heading = document.createElement('h' + level);
        heading.textContent = title;
        const totals = document.createElement('div');
        totals.className = 'totals';
        for (const [status, label] of [['pass', 'passed'], ['fail', 'failed']]) {
            const count = records.filter(record => record.dataset.status === status).length;
            if (!count) continue;
            const total = document.createElement('span');
            total.className = status;
            total.textContent = count + ' ' + label;
            totals.append(total);
        }
        header.append(heading, totals);
        if (level === 2) {
            const back = document.createElement('a');
            back.className = 'back';
            back.href = '#overview';
            back.textContent = 'Back to overview ↑';
            header.append(back);
        }
        node.append(header);
        return node;
    }

    function groupResults(mode) {
        currentMode = mode;
        for (const radio of controls.querySelectorAll('input')) radio.checked = radio.value === mode;
        for (const result of results) {
            for (const comparison of result.querySelectorAll('.comparison')) {
                const label = mode === 'category' ? result.dataset.backend : result.dataset.testLabel;
                const kind = comparison.dataset.kind;
                comparison.querySelector('.comparison-title').textContent =
                    label + (kind && kind !== 'reference' ? ' · ' + kind : '');
                comparison.querySelector('.backend').hidden = true;
            }
        }
        // Move existing comparisons to preserve their IDs, images and expanded state.
        container.replaceChildren();
        if (mode === 'backend') {
            for (const backend of backends) {
                const records = results.filter(result => result.dataset.backend === backend);
                const parent = section('backend-' + backend, backend, records, 2);
                for (const category of categories) {
                    const children = records.filter(result => result.dataset.category === category.key);
                    if (!children.length) continue;
                    const group = section('backend-' + backend + '-category-' + category.key, category.title, children, 3);
                    group.append(...children);
                    parent.append(group);
                }
                container.append(parent);
            }
        } else {
            for (const category of categories) {
                const records = results.filter(result => result.dataset.category === category.key);
                const parent = section('category-' + category.key, category.title, records, 2);
                const cases = Array.from(new Set(records.map(result => result.dataset.test)));
                for (const test of cases) {
                    const children = records.filter(result => result.dataset.test === test);
                    const group = section('category-' + category.key + '-test-' + test, children[0].dataset.testLabel, children, 3);
                    group.append(...children);
                    parent.append(group);
                }
                if (!records.length) {
                    const link = document.createElement('a');
                    link.href = '#skipped-tests';
                    link.textContent = 'All tests in this category were skipped. View skipped tests.';
                    parent.append(link);
                }
                container.append(parent);
            }
        }
    }

    function revealTarget() {
        const id = location.hash.slice(1);
        if (id.startsWith('category-') && currentMode !== 'category') groupResults('category');
        if (id.startsWith('backend-') && currentMode !== 'backend') groupResults('backend');
        const target = document.getElementById(id);
        if (!target) return;
        if (target.matches('details.comparison')) target.open = true;
        target.scrollIntoView();
    }
    controls.hidden = results.length === 0;
    controls.addEventListener('change', event => groupResults(event.target.value));
    window.addEventListener('hashchange', revealTarget);
    // Also handle clicking the current anchor after changing grouping manually.
    document.addEventListener('click', event => {
        const link = event.target.closest('a[href^="#"]');
        if (link && link.hash === location.hash) revealTarget();
    });
    revealTarget();
})();
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


def capture(executable, root, backend, case, timeout=60, launcher=None, target_platform=None):
    """One fresh process and output file per case, including failed runs."""
    output = root / backend / (case + '.png')
    output.parent.mkdir(parents=True, exist_ok=True)
    remove_capture_images(output)
    command = [str(executable), '--backend', backend, '--case', case, '--output-file', str(output)]
    record = dict(backend=backend, case=case, status='fail', command=command,
                  platform=platform.platform(), machine=platform.machine(), actual_backend=None)
    if target_platform:
        record['target_platform'] = target_platform
    if launcher:
        record.update(launcher.metadata)
        record['command'] = [sys.executable, str(Path(__file__).resolve()), '--executable', str(executable),
                             '--target-platform', target_platform, '--simulator', launcher.simulator.udid,
                             '--capture-dir', str(root), '--backend', backend, '--available', backend,
                             '--output', str(root.parent / 'graphics-render-report')]
    start = time.monotonic()
    try:
        run = launcher.run if launcher else subprocess.run
        process = run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                      timeout=timeout, text=True, errors='replace')
        record.update(exit_code=process.returncode, log=process.stdout)
        if launcher:
            record['launch_command'] = process.args
        platforms = re.findall(r'^GRAPHICS_CAPTURE_PLATFORM=([\w-]+)$', process.stdout, re.MULTILINE)
        if target_platform and platforms != [target_platform]:
            raise ValueError('Requested target platform identity was not confirmed')
        if len(platforms) == 1:
            record['target_platform'] = platforms[0]
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


def run_matrix(executable, root, matrix, available, explicit=False, skip_all=None, launcher=None, target_platform=None):
    root.mkdir(parents=True, exist_ok=True)
    (root / 'captures.json').unlink(missing_ok=True)
    records = []
    for backend in matrix:
        for case in CASES:
            if skip_all or backend not in available:
                record = dict(backend=backend, case=case, status='fail' if explicit and not skip_all else 'skip',
                              reason=skip_all or '%s adapter omitted from this build/configuration' % backend,
                              platform=platform.platform(), machine=platform.machine())
                if target_platform:
                    record['target_platform'] = target_platform
                if target_platform == 'arm64_sim-ios':
                    record.update(platform='iOS Simulator (not run)', machine='arm64')
                if launcher:
                    record.update(launcher.metadata)
                # A skipped/failed availability check must not reuse an old PNG.
                path = root / backend / (case + '.png')
                path.parent.mkdir(parents=True, exist_ok=True)
                remove_capture_images(path)
                path.with_suffix('.log').write_text('', encoding='utf-8')
                path.with_suffix('.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
            else:
                options = {}
                if launcher:
                    options['launcher'] = launcher
                if target_platform:
                    options['target_platform'] = target_platform
                record = capture(executable, root, backend, case, **options)
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
            record.setdefault('backend', 'unknown')
            record.setdefault('case', 'unknown')
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


def count_results(records):
    return {status: sum(record['case_status'] == status for record in records)
            for status in ('pass', 'fail', 'skip')}


def comparison_html(index, result, record, label=None):
    esc = lambda value: html.escape(str(value))
    score = '%.5f%%' % result['likeness_percent'] if 'likeness_percent' in result else 'unavailable'
    title = label or result['case']
    if result['kind'] != 'reference':
        title += ' · ' + result['kind'].title()
    description = CASE_DESCRIPTIONS[result['case']] if result['kind'] == 'reference' else (
        'Repeated render must match the first image exactly.' if result['kind'] == 'repeated' else
        'Draws with retained %s state must match the uninterrupted sequence exactly.' % result['kind'])
    status = record['case_status'] if result['kind'] == 'reference' else result['status']
    reason = record['case_reason'] if result['kind'] == 'reference' else result.get('reason', '')
    parts = ['<details class="comparison" id="comparison-%s" data-kind="%s"%s><summary><span class="comparison-title">%s</span><span class="backend"%s>%s</span><span class="comparison-score %s">%s · likeness %s</span></summary><article>' %
             (index, result['kind'], ' open' if status == 'fail' else '', esc(title), ' hidden' if label else '', esc(result['backend']), status, status.upper(), score),
             '<p>%s</p>' % esc(description)]
    if reason:
        parts.append('<p class="reason fail">%s</p>' % esc(reason))
    parts.append('<div class="images">%s%s%s</div>' % (
        image_html(result['actual_label'], result.get('actual')),
        image_html(result['reference_label'], result.get('reference')),
        image_html('Difference (contrast ×4)', result.get('difference'))))
    details = dict(comparison=result, captures=[record])
    parts.append('<details><summary>Configuration, comparison and logs</summary><pre>%s</pre></details></article></details>' %
                 esc(json.dumps(details, indent=2)))
    return '\n'.join(parts)


def capture_html(record, checks, label=None):
    if checks:
        return '\n'.join(comparison_html(index, result, record, label) for index, result in checks)
    esc = lambda value: html.escape(str(value))
    return '<details class="comparison" id="capture-%s" open><summary><span class="comparison-title">%s</span><span class="backend"%s>%s</span><span class="comparison-score fail">FAIL</span></summary><article><p class="reason fail">%s</p><details><summary>Process log and reproduction</summary><pre>%s</pre><pre>%s</pre></details></article></details>' % (
        esc(record['id']), esc(label or record['case']), ' hidden' if label else '', esc(record['backend']), esc(record['case_reason']),
        esc(shlex.join(record.get('command', []))), esc(record.get('log', '')))


def result_groups_html(categories, checks_by_capture):
    esc = lambda value: html.escape(str(value))
    records = [record for _, _, _, group, _ in categories for record in group if record['case_status'] != 'skip']
    parts = ['<div class="results-toolbar"><h2>Test results</h2>',
             '<fieldset id="results-grouping" hidden><legend>Group report by</legend><label><input type="radio" name="results-grouping" value="backend" checked> Backend</label><label><input type="radio" name="results-grouping" value="category"> Test category</label></fieldset></div>',
             '<div id="test-results">']

    def heading(title, group, level):
        totals = count_results(group)
        return '<div class="%s"><h%s>%s</h%s><div class="totals">%s</div>%s</div>' % (
            'section-heading' if level == 2 else 'title-row', level, esc(title), level,
            ' '.join('<span class="%s">%s %s</span>' % (status, totals[status], text)
                     for status, text in (('pass', 'passed'), ('fail', 'failed')) if totals[status]),
            '<a class="back" href="#overview">Back to overview ↑</a>' if level == 2 else '')

    # The default backend/category hierarchy is also usable without JavaScript.
    linked_categories = set()
    for backend in dict.fromkeys(record['backend'] for record in records):
        backend_records = [record for record in records if record['backend'] == backend]
        parts.append('<section class="result-section" id="backend-%s">%s' % (esc(backend), heading(backend, backend_records, 2)))
        for key, title, _, group, _ in categories:
            children = [record for record in group if record['backend'] == backend and record['case_status'] != 'skip']
            if not children:
                continue
            parts.append('<section class="result-group" id="backend-%s-category-%s">' % (esc(backend), key))
            if key not in linked_categories:
                parts.append('<span id="category-%s"></span>' % key)
                linked_categories.add(key)
            parts.append(heading(title, children, 3))
            for record in children:
                case = record['case']
                label = case.removeprefix('texture_').replace('_', ' ').upper() if key == 'textures' else case.replace('_', ' ').capitalize()
                parts.append('<div class="test-result" data-category="%s" data-test="%s" data-test-label="%s" data-backend="%s" data-status="%s">%s</div>' % (
                    key, esc(case), esc(label), esc(backend), esc(record['case_status']),
                    capture_html(record, checks_by_capture.get(record['id'], []), label)))
            parts.append('</section>')
        parts.append('</section>')
    parts.append('</div>')
    return '\n'.join(parts)


def write_report_html(report, output):
    captures, comparisons, counts = report['captures'], report['comparisons'], report['counts']
    esc = lambda value: html.escape(str(value))
    status = 'fail' if counts['fail'] else 'pass' if counts['pass'] else 'skip'
    status_label = {'fail': 'Failures found', 'pass': 'All tests passed', 'skip': 'No tests run'}[status]
    if status == 'pass' and counts['skip']:
        status_label = 'Passed with skips'
    tested_platforms = list(dict.fromkeys(record.get('target_platform') or record.get('platform') or 'Unknown platform'
                                        for record in captures))
    platform_title = ' · '.join(tested_platforms) or 'No platform recorded'
    platforms = {}
    for record in captures:
        key = (record.get('platform') or 'Platform not recorded', record.get('machine') or 'Architecture not recorded')
        platforms.setdefault(key, []).append(record)
    known_cases = {case for _, _, _, cases in CATEGORIES for case in cases}
    categories = []
    for key, title, description, cases in CATEGORIES:
        records = [record for record in captures if record['case'] in cases or
                   (key == 'other' and record['case'] not in known_cases)]
        if records:
            categories.append((key, title, description, records, count_results(records)))
    checks_by_capture = {}
    section_by_capture = {}
    for index, result in enumerate(comparisons):
        checks_by_capture.setdefault(result['capture'], []).append((index, result))
        previous = section_by_capture.get(result['capture'])
        if previous is None or (comparisons[previous]['status'] != 'fail' and result['status'] == 'fail'):
            section_by_capture[result['capture']] = index

    parts = ['<!doctype html><html lang="en"><head><meta charset="utf-8">',
             '<meta name="viewport" content="width=device-width,initial-scale=1"><title>Graphics likeness tests</title>',
             '<style>' + STYLE + '</style></head><body><header id="overview">',
             '<div class="eyebrow">Graphics · Regression tests</div>',
             '<div class="title-row"><h1>Graphics likeness tests · %s</h1><span class="badge %s">%s</span></div>' % (esc(platform_title), status, status_label),
             '<p class="muted">Rendering results across graphics backends.</p><div class="counts">']
    for key, label, value in (('', 'Total tests', len(captures)), ('pass', 'Passed', counts['pass']),
                              ('fail', 'Failed', counts['fail']), ('skip', 'Skipped', counts['skip'])):
        parts.append('<div class="count"><strong class="%s">%s</strong><span>%s</span></div>' % (key, value, label))
    parts.append('</div><div class="platforms">')
    for (name, machine), records in platforms.items():
        short_name = re.match(r'^(macOS|Windows|Linux)-([^-]+)', name)
        display_name = ' '.join(short_name.groups()) if short_name else name
        parts.append('<section class="platform"><div class="title-row"><h2 class="platform-name" title="%s">%s</h2><span class="muted">%s</span></div>' % (esc(name), esc(display_name), esc(machine)))
        parts.append('<table><thead><tr><th scope="col">Backend</th><th scope="col">Passed</th><th scope="col">Failed</th><th scope="col">Skipped</th></tr></thead><tbody>')
        for backend in dict.fromkeys(record['backend'] for record in records):
            backend_records = [record for record in records if record['backend'] == backend]
            backend_counts = count_results(backend_records)
            target = 'backend-' + backend if any(record['case_status'] != 'skip' for record in backend_records) else 'skipped-tests'
            parts.append('<tr><th scope="row"><a href="#%s">%s</a></th>%s</tr>' % (esc(target), esc(backend), ''.join(
                '<td class="%s">%s</td>' % (outcome if backend_counts[outcome] else 'muted', backend_counts[outcome])
                for outcome in ('pass', 'fail', 'skip'))))
        parts.append('</tbody></table></section>')
    parts.append('</div></header><main>')

    parts.append('<section aria-labelledby="categories-heading"><div class="section-heading"><h2 id="categories-heading">Test categories</h2><p class="muted">Select a category to browse its results.</p></div><div class="categories">')
    for key, title, description, records, totals in categories:
        target = 'category-' + key if totals['pass'] or totals['fail'] else 'skipped-tests'
        parts.append('<a class="category" data-category="%s" href="#%s" aria-label="View %s results"><h3>%s</h3><p class="muted description">%s</p>' % (key, target, title, title, description))
        sizes = ((len({record['case'] for record in records}), 'case'),
                 (len({record['backend'] for record in records}), 'backend'), (len(records), 'test'))
        parts.append('<p class="muted">%s</p>' % ' · '.join(
            '%s %s%s' % (size, label, '' if size == 1 else 's') for size, label in sizes))
        parts.append('<div class="result-bar" aria-hidden="true">%s</div>' % ''.join(
            '<span class="%s" style="flex:%s"></span>' % (outcome, totals[outcome])
            for outcome in ('pass', 'fail', 'skip') if totals[outcome]))
        parts.append('<div class="totals">%s</div><span class="category-link">View results ↓</span></a>' % ' '.join(
            '<span class="%s">%s %s</span>' % (outcome, totals[outcome], label)
            for outcome, label in (('pass', 'passed'), ('fail', 'failed'), ('skip', 'skipped')) if totals[outcome]))
    parts.append('</div></section>')

    failures = [record for record in captures if record['case_status'] == 'fail']
    parts.append('<section aria-labelledby="failures-heading"><div class="section-heading"><h2 id="failures-heading">Failed tests (%s)</h2></div>' % len(failures))
    if failures:
        parts.append('<div class="failure-list"><ul>')
        for record in failures:
            section = section_by_capture.get(record['id'])
            target = 'comparison-%s' % section if section is not None else 'capture-' + record['id']
            parts.append('<li><a href="#%s">%s / %s →</a><p class="muted reason">%s</p></li>' %
                         (esc(target), esc(record['backend']), esc(record['case']), esc(record['case_reason'])))
        parts.append('</ul></div>')
    else:
        parts.append('<p class="muted">No failed tests.</p>')
    parts.append('</section>')
    skipped = [record for record in captures if record['case_status'] == 'skip']
    if skipped:
        parts.append('<details class="skipped" id="skipped-tests"><summary>Skipped tests (%s)</summary><ul>' % len(skipped))
        for record in skipped:
            parts.append('<li class="skip">%s / %s — %s</li>' %
                         (esc(record['backend']), esc(record['case']), esc(record['case_reason'])))
        parts.append('</ul></details>')

    parts.append(result_groups_html(categories, checks_by_capture))

    data = base64.b64encode((output / 'results.json').read_bytes()).decode('ascii')
    parts.extend(['</main><footer><div class="links"><a download="results.json" href="data:application/json;base64,%s">All results (JSON)</a></div>' % data,
                  '<details><summary>Comparison rules</summary><p>Each backend/case is counted once. Required: ≥99% RGB likeness. Clear: whole image. All other cases: foreground union. Readback diagnostics require identical pixels. No alignment, resizing or color correction.</p></details>',
                  '<details><summary>Rebuild this report</summary><pre>' + esc(shlex.join(report['reproduction'])) + '</pre></details>',
                  '<details><summary>Full JSON results</summary><pre>' + esc(json.dumps(report, indent=2)) + '</pre></details></footer>',
                  '<script>' + RESULT_GROUPING_SCRIPT + '</script></body></html>'])
    (output / 'index.html').write_text('\n'.join(parts), encoding='utf-8')


def make_report(roots, references, output):
    output.mkdir(parents=True, exist_ok=True)
    differences = output / 'differences'
    differences.mkdir(exist_ok=True)
    for stale in differences.glob('*.png'):
        stale.unlink()
    # Exit 77 marks unsupported texture formats. Keep them in the capture
    # manifest, but omit them from report entries and totals.
    captures = [record for record in load_captures(roots)
                if not (record['status'] == 'skip' and record.get('exit_code') == 77
                        and record['case'] in TEXTURE_CASES)]
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
    counts = count_results(captures)
    report = dict(threshold=THRESHOLD, dimensions=SIZE, captures=captures, comparisons=comparisons, counts=counts,
                  reproduction=[sys.executable, str(Path(__file__).resolve()),
                      *itertools.chain.from_iterable(('--images', str(root)) for root in roots),
                      '--references', str(references), '--output', str(output)])
    (output / 'results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    write_report_html(report, output)
    print('Graphics likeness: %s; report: %s' % (counts, output / 'index.html'))
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--target-platform', help='Defold platform tuple; arm64_sim-ios launches in the iOS simulator')
    parser.add_argument('--simulator', help='Simulator name or UDID (defaults to IOS_SIMULATOR_ID or the booted device)')
    parser.add_argument('--capture-dir', type=Path)
    parser.add_argument('--images', type=Path, action='append', default=[])
    parser.add_argument('--matrix', choices=BACKENDS, nargs='+', default=list(BACKENDS))
    parser.add_argument('--available', choices=BACKENDS, nargs='*', default=[])
    parser.add_argument('--backend', choices=BACKENDS, action='append', help='Explicit backend request; unavailable is a failure')
    parser.add_argument('--skip-all', help='CMake hosted-CI policy skip reason')
    parser.add_argument('--references', type=Path, default=Path(__file__).with_name('graphics_reference'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    if args.simulator and (not args.executable or args.target_platform != 'arm64_sim-ios'):
        parser.error('--simulator requires --executable and --target-platform arm64_sim-ios')
    if args.executable:
        if not args.capture_dir or args.images:
            parser.error('--executable requires --capture-dir and cannot be combined with --images')
        matrix = args.backend or args.matrix
        session = nullcontext()
        if args.target_platform == 'arm64_sim-ios' and not args.skip_all and set(matrix).intersection(args.available):
            from graphics_capture_simulator import simulator_capture
            session = simulator_capture(args.executable.resolve(), args.simulator)
        with session as launcher:
            run_matrix(args.executable.resolve(), args.capture_dir.resolve(), matrix,
                       args.available, explicit=bool(args.backend), skip_all=args.skip_all,
                       launcher=launcher, target_platform=args.target_platform)
        args.images = [args.capture_dir]
    elif not args.images:
        parser.error('Use --executable with --capture-dir, or one or more --images directories')
    report = make_report([root.resolve() for root in args.images], args.references.resolve(), args.output.resolve())
    return 1 if report['counts']['fail'] else 0


if __name__ == '__main__':
    sys.exit(main())
