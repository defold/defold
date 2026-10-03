"""Report the independently toggled window-cache and dispatch experiments."""
import argparse
import csv
import json
import tarfile
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from analysis import analyze, mean

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('data', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--control', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)

names = {
    'direct': 'Direct', 'threaded': 'Original rAF', 'scheduled': 'Original completion',
    'cached_raf': 'Cached rAF', 'cached_completion': 'Cached completion',
    'cached_retry': 'Cached rAF + retry',
}
colors = ['#66788a', '#a18cac', '#c88741', '#478fcc', '#22966f', '#b94e6c']

def read_runs(root):
    manifest = json.loads((root / 'manifest.json').read_text())
    assert len(manifest['runs']) == len(manifest['cases']) * len(manifest['modes']) * manifest['repeats'], 'Incomplete collection'
    runs = [json.loads((root / (key + '.json')).read_text()) for key in manifest['runs']]
    assert all(r['valid'] and r['exit'] == 0 and not r['result']['engine']['is_debug'] for r in runs)
    return manifest, runs, [analyze(r, root) for r in runs]

manifest, runs, rows = read_runs(args.data)
modes = manifest['modes']
cases = [c['name'] for c in manifest['cases']]
for case in cases:
    assert len({tuple(m['canvas']) for r in runs if r['case']['name'] == case for m in r['memory']}) == 1
with (args.output / 'runs.csv').open('w') as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

def values(case, mode, key):
    return [r[key] for r in rows if r['case'] == case and r['mode'] == mode]

def metric(case, mode, key):
    return mean(values(case, mode, key))

plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False})

def chart(key, title, unit, filename, selected=None):
    fig, ax = plt.subplots(figsize=(13, 5.6))
    x = np.arange(len(cases))
    selected = modes if selected is None else selected
    width = .82 / len(selected)
    for i, mode in enumerate(selected):
        samples = [values(c, mode, key) for c in cases]
        means = [mean(v) for v in samples]
        errors = [[max(0, y-min(v)) for y, v in zip(means, samples)], [max(0, max(v)-y) for y, v in zip(means, samples)]]
        ax.bar(x + (i-(len(selected)-1)/2)*width, means, width, label=names[mode], color=colors[modes.index(mode)], yerr=errors, capsize=2)
    ax.set(xticks=x, xticklabels=cases, ylabel=unit, ylim=(0, None))
    ax.grid(axis='y', alpha=.18)
    ax.set_axisbelow(True)
    fig.suptitle(title, y=.98, fontsize=14)
    fig.legend(*ax.get_legend_handles_labels(), ncol=3, fontsize=9, loc='upper center', bbox_to_anchor=(.5, .925), frameon=False)
    fig.text(.5, .015, f"Visible Chrome / Apple M1 Pro · {manifest['repeats']} repeats; bars = means, whiskers = run ranges", ha='center', fontsize=9)
    fig.tight_layout(rect=[0, .045, 1, .82])
    fig.savefig(args.output / filename, dpi=160)
    plt.close(fig)

for key, title, unit, filename in [
    ('ups', 'Simulation throughput', 'Updates / second', 'throughput.png'),
    ('age', 'Simulation start → CPU render submission', 'Mean milliseconds (not physical input latency)', 'age.png'),
    ('submit_p99', 'Render submission interval p99', 'Milliseconds (lower is better)', 'p99.png'),
    ('wasm', 'Peak sampled WASM capacity, diagnostics enabled', 'MiB (capacity, not live allocations)', 'memory.png'),
    ('sync_ms', 'Synchronous graphics-owner calls', 'Elapsed milliseconds / worker update', 'sync.png'),
    ('cpu_per_update', 'Browser CPU time per simulation update', 'CPU milliseconds / update', 'cpu.png'),
]:
    chart(key, title, unit, filename)
chart('dispatch_age', 'Input dispatch → CPU render submission (threaded modes)', 'Mean milliseconds (includes pre-simulation waits)', 'dispatch-age.png', [m for m in modes if m != 'direct'])

lines = [
    '# Web component threading: window cache and dispatch retry', '',
    f"Measured {manifest['started'][:10]}. {len(rows)} instrumented runs, same PoC Release binary and content in all modes. Direct uses its legacy path; it is not a separate vanilla-engine build.", '',
    '## Implementation', '',
    '- Window cache: browser main captures OPENED together with input/window state while the worker is idle, then publishes it with the next tick. The game thread reads that owned snapshot instead of making a synchronous main-thread call. Close events are still applied before simulation. Default remains off.',
    '- Dispatch retry: browser main tries again after consuming a frame only if it did not dispatch at callback start. At most one update is admitted per callback. No extra frame slots, dropped updates, or adaptive prediction were introduced. Default remains the original rAF scheduler.',
    '- Diagnostics now separately record synchronous graphics-owner calls. These spans include owner execution and waiting, and overlap the worker-total span. They are not additional CPU time to add to the other columns.', '',
    '| Mode | Window cache | Schedule |', '| --- | ---: | --- |',
    '| Direct | — | Existing main-thread flow |',
    '| Original rAF | 0 | 0: dispatch before consumption |',
    '| Original completion | 0 | 1: also dispatch on worker completion |',
    '| Cached rAF | 1 | 0 |',
    '| Cached completion | 1 | 1 |',
    '| Cached rAF + retry | 1 | 2: retry after consumption if not already dispatched |', '',
    'Use `render.poc_pipeline=component`, `render.poc_threaded=1`, `render.poc_web_cache_window=1`, and the chosen `render.poc_web_schedule` value. Scheduling experiments require the overlapping sprite path (`poc_web_2d=0`).', '',
    '## Throughput', '',
    '| Workload | ' + ' | '.join(names[m] for m in modes) + ' |',
    '| --- | ' + ' | '.join('---:' for _ in modes) + ' |',
]
for c in cases:
    lines.append('| ' + c + ' | ' + ' | '.join(f'{metric(c,m,"ups"):.2f}' for m in modes) + ' |')
for title, filename in [('Updates per second', 'throughput.png'), ('Frame age', 'age.png'), ('Dispatch-to-submit age', 'dispatch-age.png'), ('Tail intervals', 'p99.png'), ('Memory capacity', 'memory.png'), ('Owner roundtrips', 'sync.png'), ('CPU cost', 'cpu.png')]:
    lines += ['', f'### {title}', '', f'![{title}]({filename})']
lines += ['', '## Per-mode evidence', '',
          'p99 is the interval at or below which 99% of observations fall; this table averages the per-run p99 values. Submission is a CPU event, not confirmed display presentation. Frame age begins at input/simulation processing, after the old window query. The additional dispatch-to-submit graph matches frames to worker updates and includes input preparation plus that pre-simulation wait. Use it to compare latency between schedulers fairly; neither metric is physical input-to-display latency. Direct has no worker dispatch and is omitted from that graph.', '',
          '| Case | Mode | Updates/s | Submit/s | Submit p99 ms | Mean age ms | Queue wait ms | Sync ms/update | Sync calls/update | WASM MiB | CPU ms/update |',
          '| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
for c in cases:
    for m in modes:
        lines.append('| '+c+' | '+names[m]+' | '+' | '.join(f'{metric(c,m,k):.3f}' for k in ['ups','fps','submit_p99','age','queue','sync_ms','sync_calls','wasm','cpu_per_update'])+' |')
lines += ['', '### Dispatch-to-submit detail (threaded modes)', '',
          '| Case | Mode | Mean ms | p99 ms | Successful retries per entire run |',
          '| --- | --- | ---: | ---: | ---: |']
for c in cases:
    for m in modes:
        if m != 'direct':
            lines.append('| '+c+' | '+names[m]+' | '+' | '.join(f'{metric(c,m,k):.3f}' for k in ['dispatch_age','dispatch_age_p99','retries'])+' |')
lines += ['', '### Isolated changes', '',
          'These comparisons use this collection only. Cache effect keeps the rAF scheduler fixed; retry effect keeps the cache fixed. Small differences relative to run ranges are inconclusive.', '',
          '| Case | Cache vs original rAF UPS | Retry vs cached rAF UPS | Cached completion vs direct UPS |',
          '| --- | ---: | ---: | ---: |']
for c in cases:
    gain = lambda a, b: (metric(c, a, 'ups')/metric(c, b, 'ups')-1)*100
    lines.append(f"| {c} | {gain('cached_raf','threaded'):+.1f}% | {gain('cached_retry','cached_raf'):+.1f}% | {gain('cached_completion','direct'):+.1f}% |")
lines += ['', '## Workloads and method', '',
          '- bunny10k and bunny30k: fixed populations of animated bouncing sprites, using Bunnymark assets in the shared benchmark harness.',
          '- balanced: 10,000 sprites, scripted movement of every sprite, and 100,000 arithmetic iterations per update.',
          '- render50k: 50,000 small stationary sprites, one pass, no added scripted simulation.',
          '- fill: 10,000 sprites scaled to 64 pixels, four passes; GPU time is not independently measured.',
          f"- {manifest['warmup']} seconds warm-up, {manifest['seconds']} seconds measurement, {manifest['repeats']} repeats per workload/mode. Mode order rotates by repeat. Fresh visible Chrome, DevTools closed, AC power and focus/visibility checked; no headless performance claims.",
          '- Hardware/browser: '+runs[0]['gpu'][0]['deviceString']+'; Chrome '+runs[0]['browser']+'.',
          '- All cases use the shared 60,000-sprite/object capacity. Drawing buffers: 720×720 for bunnies, 1280×720 for synthetic scenes; identical dimensions checked across modes. These are not the original standalone Bunnymark bundle settings.',
          '- WASM figures are maximum sampled buffer capacity per run, averaged across repeats. Growth is chunked; this is not live allocated memory. Diagnostics add fixed buffers. Browser RSS and CPU figures are retained in runs.csv; process CPU time is not a power/energy measurement.',
          '- The timing trace uses bounded buffers and rejects overflow. Synchronous-call records are filtered separately from worker updates. No successful runs are discarded. Failed setup/correctness pilots are retained outside the performance collection.',
          '- Browser scheduling remains display-paced. Only Chrome on this Apple M1 Pro is measured. Broader 2D compatibility is serialized and excluded; lower-powered/mobile targets, a representative game replay, physical latency, and energy remain untested.', '']
failed = list((args.data / 'failed').glob('*.json'))
lines += ['## Collection interruptions and validation', '',
          f'- {len(failed)} invalid attempts are retained under `failed/` in the raw archive. The runner stopped at failed focus checks, then resumed without replacing any successful run.',
          '- Low Power Mode was already off. Display sleep was configured after 10 minutes, and no display-sleep assertion was active initially. After the interruptions, `caffeinate -diu` held display/system sleep off for the remaining collection; macOS confirmed both assertions. Display sleep is a plausible cause of focus loss, not independently proved by the samples.',
          '- Playwright focus emulation is disabled after navigation for this collection. Its default emulation affected the prior report’s visibility checks. Sampling focus still does not independently detect every kind of OS window occlusion.',
          '- Native graphics: 65 tests / 17,324 assertions passed, including a new test that checks cached open state, snapshot immutability, refresh to closed, and the uncached fallback. Native engine admission: 1 test / 16 assertions passed; the native test engine and web Release engine rebuilt.',
          '- Browser correctness: all seven configurations matched simulation checksum 82980080 and PNG SHA-256 `26f4afe9718d074214a91e407447f339c873989cc8e695ca012b2463115b9d45`. Input, render/update pause/resume and context-loss shutdown passed.',
          '- Injected hidden-state tests passed for all cached schedules; with animation callbacks paused, the hidden service completed context-loss shutdown. Real tab visibility was not validated: automation continued to expose `document.hidden=false` during attempted tab/window backgrounding. This limitation is recorded rather than treating injection as a real hide/show test.',
          '- See [validation.json](validation.json) for correctness evidence. The raw archive includes engine artifact hashes and the engine/graphics working-tree patch against the recorded revision.', '']
if args.control:
    cm, cr, control = read_runs(args.control)
    lines[2] += f" Another {len(control)} diagnostics-off runs check normal memory/throughput at 30k."
    lines += ['## Diagnostics-off memory/throughput check', '',
              '| Mode | Updates/s mean | Run range | Peak WASM capacity range MiB |', '| --- | ---: | --- | --- |']
    for m in cm['modes']:
        g = [r for r in control if r['mode'] == m]
        ups = [r['ups'] for r in g]
        mem = [r['wasm'] for r in g]
        lines.append(f'| {names[m]} | {mean(ups):.2f} | {min(ups):.2f}–{max(ups):.2f} | {min(mem):.2f}–{max(mem):.2f} |')
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.7))
    for ax, key, unit in zip(axes, ['ups', 'wasm'], ['Updates / second', 'Peak sampled WASM capacity, MiB']):
        samples = [[r[key] for r in control if r['mode'] == m] for m in cm['modes']]
        means = [mean(v) for v in samples]
        errors = [[max(0, y-min(v)) for y, v in zip(means, samples)], [max(0, max(v)-y) for y, v in zip(means, samples)]]
        ax.bar(range(len(cm['modes'])), means, color=[colors[modes.index(m)] for m in cm['modes']], yerr=errors, capsize=3)
        ax.set(xticks=range(len(cm['modes'])), xticklabels=[names[m].replace(' ', '\n', 1) for m in cm['modes']], ylabel=unit, title='30k bunnies: diagnostics disabled')
        ax.tick_params(axis='x', labelsize=9)
        ax.grid(axis='y', alpha=.18)
        ax.set_axisbelow(True)
    fig.tight_layout()
    fig.savefig(args.output / 'control.png', dpi=160)
    plt.close(fig)
    lines += ['', '![Diagnostics-off throughput and memory](control.png)', '']
    lines += ['', 'Bars show means and whiskers show run ranges; memory uses each run’s maximum sampled capacity. This later collection checks the same 30k scene without trace buffers. Capacity grows in chunks and can differ across runs. Differences from the earlier instrumented collection mix diagnostic overhead with temporal variation; they do not isolate instrumentation cost.', '']
lines += ['## Reproduction and evidence', '',
          'See `scripts/web/benchmark/README.md`. Use `MODES=direct,threaded,scheduled,cached_raf,cached_completion,cached_retry` and `CASES=bunny10k,bunny30k,balanced,render50k,fill` with the collector, then run `improvements_report.py RAW_DIRECTORY REPORT_DIRECTORY`. Optional `--control` adds a diagnostics-off collection. Raw JSON/CSV, configuration banners and per-run samples are preserved in [raw-results.tar.gz](raw-results.tar.gz); calculated metrics are in [runs.csv](runs.csv).', '']
assessment = args.output / 'ASSESSMENT.md'
if assessment.exists():
    lines[4:4] = assessment.read_text().splitlines() + ['']
(args.output / 'REPORT.md').write_text('\n'.join(lines))
with tarfile.open(args.output / 'raw-results.tar.gz', 'w:gz') as archive:
    archive.add(args.data, arcname=args.data.name)
    if args.control:
        archive.add(args.control, arcname=args.control.name)
print(args.output / 'REPORT.md')
