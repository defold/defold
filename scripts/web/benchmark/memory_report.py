"""Report live allocator usage separately from WASM capacity and snapshot storage."""
import argparse
import csv
import json
import statistics
import tarfile
from pathlib import Path

MIB = 1024 * 1024
NAMES = {
    'before_direct': 'Direct · before',
    'before_threaded': 'Threaded · before / 5 MiB stack',
    'after_direct': 'Direct · after',
    'after_threaded': 'Compact snapshots / 5 MiB stack',
    'after_stack2': 'Compact snapshots / 2 MiB stack',
    'after_linear_stack2': 'Compact / 2 MiB stack / 4 MiB growth',
}


def analyze_memory(run):
    assert run['valid'] and run['exit'] == 0 and run['result']['valid']
    assert not run['result']['engine']['is_debug']
    assert run['timing']['samples'] > 0 and run['timing']['overflow_samples'] == 0
    assert run['memory'] and run['snapshots']
    assert all(not m['hidden'] and m['focused'] for m in run['memory'])
    samples = run['memory']
    # These domains overlap: never sum allocator, snapshot, stack or resource sizes.
    return {
        'case': run['case']['name'], 'mode': run['mode'], 'repeat': run['repeat'],
        'ups': run['result']['update_intervals_per_second'],
        'p99_ms': run['timing']['p99_ms_upper_bound'],
        'allocator_peak_mib': max(m['allocator']['allocated'] for m in samples) / MIB,
        'allocator_free_last_mib': samples[-1]['allocator']['free'] / MIB,
        'allocator_arena_peak_mib': max(m['allocator']['arena'] for m in samples) / MIB,
        'wasm_peak_mib': max(m['heap'] for m in samples) / MIB,
        'browser_rss_peak_mib': max(m['rss'] for m in samples) / MIB,
        'snapshot_used_mib': max(s['slots_payload_used_bytes'] for s in run['snapshots']) / MIB,
        'snapshot_capacity_mib': max(s['frame_capacity_bytes'] for s in run['snapshots']) / MIB,
        'snapshot_growth_budget_mib': max(s['frame_growth_peak_bytes'] for s in run['snapshots']) / MIB,
        'renderer_cpu_mib': max(s['renderer_cpu_capacity_bytes'] for s in run['snapshots']) / MIB,
        'renderer_gpu_logical_mib': max(s['renderer_gpu_logical_bytes'] for s in run['snapshots']) / MIB,
        'worker_stack_mib': run.get('stack', {}).get('reserved', 0) / MIB,
        'record_bytes': run['snapshots'][-1]['record_bytes'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--evidence', nargs='*', type=Path, default=[])
    args = parser.parse_args()
    manifest = json.loads((args.data / 'manifest.json').read_text())
    modes = manifest['modes']
    cases = [c['name'] for c in manifest['cases']]
    assert manifest['memoryProbe'] and not manifest['metrics'] and not manifest['headless']
    assert not manifest['stackMeasure'], 'Do not mix watermark runs with performance measurements'
    assert len(manifest['runs']) == len(cases) * len(modes) * manifest['repeats']
    assert len(set(manifest['runs'])) == len(manifest['runs'])
    runs = [json.loads((args.data / (k + '.json')).read_text()) for k in manifest['runs']]
    rows = [analyze_memory(r) for r in runs]
    for case in cases:
        assert len({tuple(m['canvas']) for r in runs if r['case']['name'] == case for m in r['memory']}) == 1
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / 'runs.csv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    def values(case, mode, key):
        result = [r[key] for r in rows if r['case'] == case and r['mode'] == mode]
        assert len(result) == manifest['repeats']
        return result

    def mean(case, mode, key):
        return statistics.mean(values(case, mode, key))

    def range_text(case, mode, key):
        v = values(case, mode, key)
        return f'{statistics.mean(v):.2f} ({min(v):.2f}–{max(v):.2f})'

    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10,
                         'axes.spines.top': False, 'axes.spines.right': False})
    colors = ['#788696', '#ad7756', '#a0aab6', '#357eaa', '#31957a', '#816cb3']

    def chart(key, title, unit, filename):
        fig, axes = plt.subplots(1, len(cases), figsize=(13, 5.1), sharey=True, sharex=True)
        largest = max(r[key] for r in rows)
        for ax, case in zip(np.atleast_1d(axes), cases):
            samples = [values(case, mode, key) for mode in modes]
            means = [statistics.mean(v) for v in samples]
            errors = [[max(0, y-min(v)) for y, v in zip(means, samples)],
                      [max(0, max(v)-y) for y, v in zip(means, samples)]]
            bars = ax.barh(np.arange(len(modes)), means, color=colors[:len(modes)], xerr=errors, capsize=3)
            ax.set(yticks=np.arange(len(modes)), yticklabels=[NAMES.get(m,m) for m in modes],
                   xlabel=unit, title={'bunny30k':'30,000 animated bunnies','render50k':'50,000 synthetic sprites'}.get(case,case))
            for i, (v, y) in enumerate(zip(samples, means)):
                ax.text(max(v) + largest*.012, i, f'{y:.2f}', va='center', fontsize=9)
            ax.set_xlim(0, largest*1.18)
            ax.grid(axis='x', alpha=.18)
            ax.set_axisbelow(True)
        np.atleast_1d(axes)[0].invert_yaxis()
        fig.suptitle(title, fontsize=14)
        fig.text(.5,.015,f"Visible Chrome / Apple M1 Pro · {manifest['repeats']} repeats · bars = means; whiskers = run ranges",ha='center',fontsize=9)
        fig.tight_layout(rect=[0,.05,1,.94])
        fig.savefig(args.output / filename, dpi=160)
        plt.close(fig)

    for spec in [
        ('allocator_peak_mib','Live allocator usage — peak sampled during measurement','MiB','memory-live.png'),
        ('wasm_peak_mib','WASM capacity — includes unused capacity','MiB','memory-capacity.png'),
        ('snapshot_capacity_mib','Two snapshot slots — allocated capacity','MiB','snapshots.png'),
        ('ups','Simulation throughput','Updates / second','throughput.png'),
        ('p99_ms','Simulation update interval p99','Milliseconds (lower is better)','p99.png'),
    ]:
        chart(*spec)

    lines = ['# Web component threading: memory optimization', '',
        f"Measured {manifest['started'][:10]}. {len(rows)} valid Release runs; {manifest['warmup']} s warmup + {manifest['seconds']} s measurement, {manifest['repeats']} repeats per configuration/workload. Large CPU traces and stack watermarking are off. Sparse allocator/snapshot samples and a bounded timing histogram are on in every configuration.", '',
        '## Assessment', '']
    assessment = args.output / 'ASSESSMENT.md'
    if assessment.exists(): lines += [assessment.read_text().strip(), '']
    lines += ['## Changes and controls', '',
        '- **Before:** preserved pre-optimization sprite layout (128 bytes), doubling array growth, and the same new memory instrumentation used after optimization. Threaded uses the previously tested window cache plus completion dispatch.',
        '- **Compact:** 96-byte sprite records; all three affine transform axes preserved; slice-9 values stored only when used; material tags shared per captured binding. Active sprite/bound storage is reserved before capture with 12.5% headroom on later growth. Two immutable slots remain.',
        '- **2 MiB stack:** opt-in worker stack size, down from 5 MiB. The production/default setting remains 5 MiB. Correctness/stack watermark pilots are separate from this collection.',
        '- **4 MiB growth:** opt-in CMake linear WASM growth experiment; default retains Emscripten’s geometric growth. This changes capacity policy, not live data size.',
        '- **Direct before/after:** main-thread legacy rendering in each PoC binary. These are controls for the optimization, not independently built vanilla Defold engines.', '',
        'All threaded configurations use `render.poc_pipeline=component`, `render.poc_threaded=1`, `render.poc_web_cache_window=1`, and `render.poc_web_schedule=1`. Smaller stack: `render.poc_web_stack_kb=2048`. Linear growth build: `DEFOLD_WEB_POC_MEMORY_GROWTH_LINEAR_STEP=4194304` with the web PoC build option enabled.', '',
        '## Measurements', '',
        'Cells show mean (minimum–maximum) across runs. Memory cells first take the maximum sampled value within each run. p99 is the update interval at or below which 99% of sampled intervals fall, rounded upward in a 0.05 ms histogram. It is not displayed FPS, render-submission p99, or input latency.', '',
        '| Workload | Configuration | Updates/s | Update p99 ms | Live allocator MiB | WASM capacity MiB |',
        '| --- | --- | ---: | ---: | ---: | ---: |']
    for case in cases:
        for mode in modes:
            lines.append('| '+case+' | '+NAMES.get(mode,mode)+' | '+' | '.join(range_text(case,mode,k) for k in ['ups','p99_ms','allocator_peak_mib','wasm_peak_mib'])+' |')
    for title, image in [('Live allocation','memory-live.png'),('WASM capacity','memory-capacity.png'),('Snapshot storage','snapshots.png'),('Throughput','throughput.png'),('Tail update intervals','p99.png')]:
        lines += ['',f'### {title}','',f'![{title}]({image})']
    lines += ['', '## Accounting detail', '',
        '| Workload | Configuration | Snapshot used MiB | Snapshot capacity MiB | Slot growth admission peaks MiB | Renderer CPU MiB | Logical GPU buffers MiB |',
        '| --- | --- | ---: | ---: | ---: | ---: | ---: |']
    for case in cases:
        for mode in modes:
            lines.append('| '+case+' | '+NAMES.get(mode,mode)+' | '+' | '.join(f'{mean(case,mode,k):.3f}' for k in ['snapshot_used_mib','snapshot_capacity_mib','snapshot_growth_budget_mib','renderer_cpu_mib','renderer_gpu_logical_mib'])+' |')
    lines += ['', '## Method and limitations', '',
        '- Visible Chrome on Apple M1 Pro, AC power checked before/after every run, Low Power Mode off; temporary display/system sleep prevention. Variant order rotates between repeats. Fresh browser process/profile per run; no DevTools. All bundles use the identical compiled content archive, with hashes retained in the manifest.',
        '- 30k uses animated Bunnymark at a 720×720 drawing buffer. 50k uses the existing synthetic sprite scene at 1280×720, including its incremental population ramp. Both retain the shared 60k object/sprite limits. Canvas size, focus and visibility are checked; sampled visibility is not an independent OS occlusion measurement.',
        '- `mallinfo.uordblks` reports allocated allocator chunks including allocator overhead and the worker stack. It excludes static storage/main stack, JavaScript/browser allocations and GPU allocations. Free allocator space can be reused; WASM capacity may remain above live usage. Peaks are sampled every ~2 seconds during measurement, not exhaustive startup peaks.',
        '- Snapshot used bytes include both slots at capture before retirement. Snapshot capacity includes capture maps; the sum of per-slot growth high-water values is a conservative admission estimate, not a measured simultaneous process peak. Snapshot storage, renderer CPU buffers and the worker stack are already in allocator totals; do not sum those columns again. Retained texture/resource descriptors refer to shared storage, not duplicate texture copies.',
        '- Browser process RSS samples are preserved in `runs.csv` and raw JSON, but shared pages may be counted in more than one process. These sums are not unique resident memory and are not used to attribute engine savings. Logical GPU buffer sizes are not actual driver allocation measurements.',
        '- Allocator scans acquire a lock; snapshot JSON and the fixed timing histogram also have overhead. Instrumentation is identical across configurations but not free. Three short steady-state repeats do not prove absence of growth stalls during extended play or safety for arbitrary native extensions.',
        '- The 2 MiB stack was validated on separate sprite and broad-2D lifecycle fixtures. Watermarking measures deepest writes after worker entry, excludes main-thread initialization/finalization and JS stack, and can miss unwritten stack reservations. No default stack reduction or default WASM growth change is made.', '',
        '## Evidence', '',
        '- [Per-run measurements](runs.csv)',
        '- [Raw JSON and manifest](raw-results.tar.gz)',
        '- [Correctness and build evidence](validation.json)',
        '- [Runner workflow](../../../../scripts/web/benchmark/README.md#memory-comparison)', '']
    (args.output/'REPORT.md').write_text('\n'.join(lines))
    with tarfile.open(args.output/'raw-results.tar.gz','w:gz') as archive:
        archive.add(args.data, arcname='matrix')
        for evidence in args.evidence:
            archive.add(evidence, arcname='evidence/' + evidence.name)
    print(f'Wrote {len(rows)} runs to {args.output}')


if __name__ == '__main__':
    main()
