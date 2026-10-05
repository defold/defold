"""Summarize the same-build web preparation-overlap comparison and raw evidence."""
import argparse
import csv
import json
import statistics
import tarfile
from pathlib import Path

NAMES = {'direct': 'Direct', 'serialized': 'Serialized', 'barrier': 'Preparation barrier', 'overlap': 'Full overlap'}
CASES = {'bunny30k': '30k bunnies', 'render50k': '50k sprites', 'balanced': 'Balanced',
         'geometry200': '200 geometry groups', 'geometry1000': '1,000 geometry groups'}
MIB = 1024 * 1024


def analyze(run):
    assert run['valid'] and run['exit'] == 0 and run['result']['valid']
    assert not run['result']['engine']['is_debug']
    assert not run['focusEmulation']
    assert run['timing']['overflow_samples'] == 0
    samples, snapshots = run['memory'], run['snapshots']
    assert all(m['focused'] and not m['hidden'] for m in samples)
    first, last = snapshots[0], snapshots[-1]
    return dict(case=run['case']['name'], mode=run['mode'], repeat=run['repeat'],
        ups=run['result']['update_intervals_per_second'],
        p99_ms=run['timing']['p99_ms_upper_bound'],
        allocator_peak_mib=max(m['allocator']['allocated'] for m in samples)/MIB,
        wasm_peak_mib=max(m['heap'] for m in samples)/MIB,
        browser_rss_peak_mib=max(m['rss'] for m in samples)/MIB,
        frame_capacity_mib=max(s.get('component_frame_capacity_bytes', 0) for s in snapshots)/MIB,
        preparation_overlaps=last.get('thread_preparations_during_render', 0)-first.get('thread_preparations_during_render', 0),
        simulation_overlaps=last.get('thread_simulations_during_render', 0)-first.get('thread_simulations_during_render', 0),
        consumed_frames=last.get('thread_completed', 0)-first.get('thread_completed', 0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('artifacts', type=Path)
    args = parser.parse_args()
    manifest = json.loads((args.data/'manifest.json').read_text())
    assert not manifest['headless'] and not manifest['metrics'] and manifest['memoryProbe']
    assert not manifest['stackMeasure']
    modes, cases = manifest['modes'], [c['name'] for c in manifest['cases']]
    assert modes == list(NAMES), 'Use MODES=direct,serialized,barrier,overlap for this comparison'
    repeats = manifest['repeats']
    assert len(manifest['runs']) == len(modes)*len(cases)*manifest['repeats']
    assert len(set(manifest['runs'])) == len(manifest['runs'])
    runs = [json.loads((args.data/(key+'.json')).read_text()) for key in manifest['runs']]
    rows = [analyze(r) for r in runs]
    for case in cases:
        assert len({(tuple(m['canvas']), m['dpr']) for r in runs if r['case']['name'] == case for m in r['memory']}) == 1
    args.artifacts.mkdir(parents=True, exist_ok=True)
    with (args.artifacts/'runs.csv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    with tarfile.open(args.artifacts/'raw-results.tar.gz', 'w:gz') as tar:
        tar.add(args.data, arcname='raw-results')

    def values(case, mode, key):
        v = [r[key] for r in rows if r['case'] == case and r['mode'] == mode]
        assert len(v) == manifest['repeats']
        return v

    def mean(case, mode, key): return statistics.mean(values(case, mode, key))

    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    colors = ['#56697e', '#e39b3e', '#8274ba', '#168a78']
    plt.rcParams.update({'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False})

    def chart(ax, key, title, ylabel):
        x = np.arange(len(cases))
        for i, mode in enumerate(modes):
            centers = x+(i-(len(modes)-1)/2)*0.2
            ys = [mean(c, mode, key) for c in cases]
            ax.bar(centers, ys, 0.18, label=NAMES[mode], color=colors[i])
            for center, case in zip(centers, cases):
                v = values(case, mode, key)
                ax.plot([center, center], [min(v), max(v)], color='#17202b', lw=1.3)
                ax.scatter([center]*len(v), v, color='#17202b', s=8, zorder=3)
        ax.set_xticks(x, [CASES[c].replace(' geometry', '\ngeometry') for c in cases])
        ax.set_ylabel(ylabel); ax.set_title(title, loc='left', weight='bold')
        ax.set_axisbelow(True); ax.grid(axis='y', alpha=0.18); ax.set_ylim(bottom=0)
        ax.margins(y=0.15)

    fig, ax = plt.subplots(figsize=(12, 5))
    chart(ax, 'ups', 'Web update throughput · higher is better', 'Updates per second')
    ax.legend(ncol=4, loc='upper center', bbox_to_anchor=(0.5, 1.16), frameon=False)
    fig.text(0.5, 0.01, f'Bars: mean of {repeats} runs. Dots and whiskers: individual runs and range. Same Release build; browser pacing enabled.', ha='center', fontsize=9)
    fig.tight_layout(rect=[0, 0.05, 1, 1]); fig.savefig(args.artifacts/'throughput.png', dpi=160); plt.close(fig)
    fig, ax = plt.subplots(figsize=(12, 5))
    chart(ax, 'p99_ms', 'p99 update interval · lower is better', 'Milliseconds')
    ax.legend(ncol=4, loc='upper center', bbox_to_anchor=(0.5, 1.16), frameon=False)
    fig.text(0.5, 0.01, 'Bars: mean of per-run p99 values, not a pooled percentile. Includes scheduling, waits and stalls; 0.05 ms histogram bins.', ha='center', fontsize=9)
    fig.tight_layout(rect=[0, 0.05, 1, 1]); fig.savefig(args.artifacts/'p99.png', dpi=160); plt.close(fig)
    fig, axes = plt.subplots(2, 1, figsize=(12, 9))
    chart(axes[0], 'allocator_peak_mib', 'Live WASM allocations · mean of sampled run peaks', 'MiB')
    chart(axes[1], 'wasm_peak_mib', 'WASM linear-memory capacity · mean of sampled run peaks', 'MiB')
    axes[0].legend(ncol=4, loc='upper center', bbox_to_anchor=(0.5, 1.2), frameon=False)
    fig.text(0.5, 0.01, 'Memory sampled every 2 seconds. These overlapping accounting domains must not be added. Browser RSS is provided in runs.csv.', ha='center', fontsize=9)
    fig.tight_layout(rect=[0, 0.05, 1, 1]); fig.savefig(args.artifacts/'memory.png', dpi=160); plt.close(fig)

    lines = ['| Workload | Direct UPS | Serialized UPS | Barrier UPS | Overlap UPS | Overlap vs barrier | Overlap vs direct |', '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for c in cases:
        v = [mean(c, m, 'ups') for m in modes]
        lines.append('| '+CASES[c]+' | '+' | '.join(f'{x:.2f}' for x in v)+f' | {(v[3]/v[2]-1)*100:+.1f}% | {(v[3]/v[0]-1)*100:+.1f}% |')
    lines += ['', '| Workload / mode | UPS range | Mean p99 (ms) | Live allocations (MiB) | WASM capacity (MiB) | Owned frame capacity (MiB) | Complete preparations during consumption¹ |', '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for c in cases:
        for m in modes:
            ups = values(c, m, 'ups')
            lines.append(f'| {CASES[c]} / {NAMES[m]} | {min(ups):.2f}–{max(ups):.2f} | '+
                ' | '.join(f'{mean(c,m,k):.2f}' for k in ['p99_ms','allocator_peak_mib','wasm_peak_mib','frame_capacity_mib'])+
                f' | {sum(values(c,m,"preparation_overlaps"))} |')
    lines += ['', f'¹ Sum of counter differences between the first and last measurement samples, across {repeats} runs. This excludes the warmup and undersamples the edges of measurement. Partial overlap does not count. Zero is not proof of no overlap.']
    (args.artifacts/'tables.md').write_text('\n'.join(lines)+'\n')
    (args.artifacts/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__': main()
