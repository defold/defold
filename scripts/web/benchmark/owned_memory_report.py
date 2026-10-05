"""Compare model frame ownership, keeping forced-GC diagnostics out of timings."""
import argparse
import csv
import json
import statistics as st
import tarfile
from pathlib import Path
from candidate_report import analyze_run

MIB = 1048576
LABELS = {'direct': 'Direct', 'copied': 'Threaded / copied', 'owned': 'Threaded / owned models'}


def load_collection(root, diagnostic):
    manifest = json.loads((root/'manifest.json').read_text())
    assert manifest['memoryDiagnostic'] is diagnostic
    assert not manifest['headless'] and not manifest['metrics'] and not manifest['stackMeasure']
    assert manifest['bundle'] and manifest['memoryProbe']
    expected = {f"{c['name']}-{mode}-{repeat+1}" for c in manifest['cases']
                for mode in manifest['modes'] for repeat in range(manifest['repeats'])}
    assert set(manifest['runs']) == expected and len(manifest['runs']) == len(expected)
    runs = [json.loads((root/(key+'.json')).read_text()) for key in manifest['runs']]
    for run in runs:
        assert run['valid'] and run['exit'] == 0 and not run['errors']
        assert run['memoryDiagnostic'] is diagnostic
        assert not run['result']['engine']['is_debug'] and run['focusEmulation'] is False
        assert all(m['focused'] and not m['hidden'] for m in run['memory'])
    return manifest, runs


def diagnostic_row(run, serialized):
    assert run['memoryDiagnostic']
    start, end = run['gc']
    assert [start['phase'], end['phase']] == ['start', 'end']
    return dict(mode=run['mode'], serialized=serialized, seconds=run['result']['elapsed_seconds'],
                start_post_gc_mib=start['allocated_after']/MIB, end_pre_gc_mib=end['allocated_before']/MIB,
                end_post_gc_mib=end['allocated_after']/MIB,
                post_gc_growth_bytes=end['allocated_after']-start['allocated_after'],
                end_reclaimed_bytes=end['allocated_before']-end['allocated_after'],
                lua_post_gc_growth_bytes=end['lua_after']-start['lua_after'],
                lua_reclaimed_bytes=end['lua_before']-end['lua_after'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('performance', type=Path)
    parser.add_argument('diagnostics', type=Path, nargs='+')
    args = parser.parse_args()
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    manifest, runs = load_collection(args.performance, False)
    rows = [dict(analyze_run(run),
                 frame_mib=max(s.get('component_frame_capacity_bytes', 0) for s in run['snapshots'])/MIB,
                 owned_upload_bytes=max(s.get('component_owned_upload_bytes', 0) for s in run['snapshots'])) for run in runs]
    diagnostics = []
    for root in args.diagnostics:
        diagnostic_manifest, diagnostic_runs = load_collection(root, True)
        assert diagnostic_manifest['bundle'] == manifest['bundle']
        assert diagnostic_manifest['environment'] == manifest['environment']
        diagnostics += [diagnostic_row(r, diagnostic_manifest['memorySamples']) for r in diagnostic_runs]
    args.output.mkdir(parents=True, exist_ok=False)
    for filename, data in [('runs.csv', rows), ('gc.csv', diagnostics)]:
        with (args.output/filename).open('w') as file:
            writer = csv.DictWriter(file, fieldnames=list(data[0]))
            writer.writeheader(); writer.writerows(data)
    with tarfile.open(args.output/'raw-results.tar.gz', 'w:gz') as archive:
        for root in [args.performance, *args.diagnostics]:
            archive.add(root, arcname=root.name)
    (args.output/'assessment.json').write_text(json.dumps({'performance':rows, 'diagnostics':diagnostics}, indent=2))
    modes = manifest['modes']
    assert len(manifest['cases']) == 1
    def values(mode, key): return [r[key] for r in rows if r['mode']==mode]
    def mean(mode, key): return st.mean(values(mode, key))
    def cell(mode, key):
        v = values(mode, key)
        return f'{st.mean(v):.3f} ({min(v):.3f}–{max(v):.3f})'
    plt.rcParams.update({'font.family':'DejaVu Sans','axes.spines.top':False,'axes.spines.right':False})
    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    for ax, (key, title, unit) in zip(axes, [('ups','Update throughput','Updates/s'),
                                             ('live_mib','Peak sampled allocation','MiB'),
                                             ('frame_mib','Owned frame capacity','MiB')]):
        means = [mean(mode,key) for mode in modes]
        ax.bar(range(len(modes)), means, color=['#64748b','#c48653','#287d91'])
        for i, mode in enumerate(modes):
            ax.scatter([i]*len(values(mode,key)), values(mode,key), color='black', s=16)
            ax.text(i, means[i]+max(means)*.04, f'{means[i]:.2f}', ha='center')
        ax.set(xticks=range(len(modes)), xticklabels=['Direct','Copied','Owned'], title=title, ylabel=unit, ylim=(0,max(means)*1.18))
        ax.grid(axis='y',alpha=.15); ax.set_axisbelow(True)
    fig.suptitle('Web model/mesh workload — frame-owned model buffers')
    fig.text(.5,.02,f"{manifest['repeats']} foreground runs/mode · {manifest['seconds']} s/run · dots = individual runs · GC diagnostics excluded",ha='center')
    fig.tight_layout(rect=[0,.06,1,.95]); fig.savefig(args.output/'comparison.png',dpi=160); plt.close(fig)
    fig, ax = plt.subplots(figsize=(10,5))
    for i, mode in enumerate(modes):
        row = next(r for r in diagnostics if r['mode']==mode and r['serialized'])
        vals=[row['start_post_gc_mib'],row['end_pre_gc_mib'],row['end_post_gc_mib']]
        ax.plot(range(3), vals, 'o-', label=LABELS[mode])
    ax.set(xticks=range(3),xticklabels=['Start, after GC','End, before GC','End, after GC'],ylabel='Allocated MiB',title='Diagnostic only — reclaimable versus retained allocation')
    ax.grid(alpha=.2);ax.legend();fig.tight_layout();fig.savefig(args.output/'gc-boundaries.png',dpi=160);plt.close(fig)
    lines=['# Web frame-owned model buffers and memory attribution','',
           'This is a focused optimization experiment, not a production acceptance run. All modes use the same Release binary and content; direct is the modified engine with threading disabled, not clean vanilla. The worker stack remains 5 MiB.','',
           'The workload contains 1,000 groups: 2,000 meshes and 5,000 models, including CPU-skinned, GPU-skinned and instanced animation. Its tiny triangles stress component/preparation work, not realistic 3D assets.','',
           '## Performance collection','',
           f"{manifest['repeats']} fresh-process foreground runs per mode, {manifest['warmup']} seconds warmup and {manifest['seconds']} seconds measurement. Mode order rotates. No forced GC, tracing or stack watermarking in these runs. Memory is sampled every two seconds.",'',
           '| Mode | Updates/s | Update p99 ms | Allocated MiB | WASM MiB | Frame capacity MiB |',
           '| --- | ---: | ---: | ---: | ---: | ---: |']
    for mode in modes:
        lines.append('| '+LABELS[mode]+' | '+' | '.join(cell(mode,key) for key in ['ups','p99_ms','live_mib','wasm_mib','frame_mib'])+' |')
    lines += ['', 'Cells show mean (minimum–maximum). p99 is the update interval at or below which 99% of samples fall; it is not input-to-display latency. Allocated memory includes Lua garbage awaiting collection, allocator overhead and the worker stack. WASM capacity includes reusable/free space. Frame capacity is already included in allocation; do not add these columns.','',
              '![Performance and memory](comparison.png)','',
              f"Owned versus copied: throughput {(mean('owned','ups')/mean('copied','ups')-1)*100:+.2f}%; live allocation {mean('owned','live_mib')-mean('copied','live_mib'):+.3f} MiB; frame capacity {mean('owned','frame_mib')-mean('copied','frame_mib'):+.3f} MiB. The instrumented frame avoids copying {mean('owned','owned_upload_bytes'):,.0f} model upload bytes per frame.",'',
              '## GC attribution diagnostics','',
              'These separate runs collect twice at each measurement boundary on the Lua owner. Collection is outside the timing window, but their timing results are deliberately excluded above. Render/game/GUI scripts share the engine Lua context. Disabling snapshot serialization keeps external allocator sampling and the fixed timing histogram enabled.','',
              '| Mode | Snapshot serialization | Seconds | Post-GC growth bytes | End reclaimed bytes | Lua post-GC growth bytes | Lua reclaimed bytes |',
              '| --- | --- | ---: | ---: | ---: | ---: | ---: |']
    for r in diagnostics:
        lines.append(f"| {LABELS[r['mode']]} | {'on' if r['serialized'] else 'off'} | {r['seconds']:.1f} | {r['post_gc_growth_bytes']:,} | {r['end_reclaimed_bytes']:,} | {r['lua_post_gc_growth_bytes']:,} | {r['lua_reclaimed_bytes']:,} |")
    lines += ['', '![GC boundaries](gc-boundaries.png)','',
              'Short fixed-population diagnostics cannot establish leak freedom across hours, resource churn, other projects or extensions. GC may return memory to the allocator without shrinking WASM capacity. Browser/JS and GPU memory are outside the allocator metric.','',
              '[Per-run performance CSV](runs.csv), [GC CSV](gc.csv), [machine-readable assessment](assessment.json), [raw results, manifests and runner sources](raw-results.tar.gz).','']
    (args.output/'REPORT.md').write_text('\n'.join(lines))


if __name__ == '__main__': main()
