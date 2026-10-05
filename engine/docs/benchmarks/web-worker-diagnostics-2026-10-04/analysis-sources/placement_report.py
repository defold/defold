"""Report independently selected sprite placement and browser pacing experiments."""
import argparse
import csv
import json
import math
import statistics as st
import tarfile
from pathlib import Path
from candidate_report import analyze_run

LABELS = {'direct': 'Direct', 'current': 'Current threading', 'deferred': 'Sprite geometry on main',
          'paced': 'Paced threading', 'combined': 'Geometry on main + pacing'}
COLORS = ['#64748b', '#c48653', '#287d91', '#8465ad', '#30975b']


def load(root):
    manifest = json.loads((root/'manifest.json').read_text())
    assert not manifest['headless'] and not manifest['metrics'] and manifest['memoryProbe']
    assert manifest.get('bundle') and not manifest.get('stackMeasure')
    expected = {(c['name'], m, n+1) for c in manifest['cases'] for m in manifest['modes']
                for n in range(manifest['repeats'])}
    runs = [json.loads((root/(key+'.json')).read_text()) for key in manifest['runs']]
    assert len(runs) == len(expected)
    assert {(r['case']['name'],r['mode'],r['repeat']) for r in runs} == expected
    rows = [analyze_run(r) for r in runs]
    return manifest, runs, rows


def diagnostic_rows(root):
    manifest = json.loads((root/'manifest.json').read_text())
    assert manifest['metrics'] and not manifest['headless']
    output = []
    for key in manifest['runs']:
        run = json.loads((root/(key+'.json')).read_text())
        assert run['valid'] and not run['errors']
        start = run['result']['measurement_clock_start'] * 1000000
        end = start + run['result']['elapsed_seconds'] * 1000000
        with (root/(key+'.csv')).open() as f:
            assert next(f).startswith('# dropped=0;')
            frames = [{k:int(v) for k,v in r.items()} for r in csv.DictReader(f)]
        frames = [r for r in frames if start <= r['input_begin_us'] <= end and r['submit_end_us'] > r['input_begin_us']]
        assert frames
        row = dict(mode=run['mode'], samples=len(frames))
        for label,a,b in [('update_ms','input_begin_us','update_end_us'),
                          ('prepare_ms','update_end_us','prepare_end_us'),
                          ('consume_ms','render_begin_us','submit_end_us'),
                          ('prepared_to_consume_ms','prepare_end_us','render_begin_us'),
                          ('input_to_submit_ms','input_begin_us','submit_end_us')]:
            values=[(r[b]-r[a])/1000 for r in frames]
            assert min(values)>=0
            row[label]=st.mean(values)
        ages=sorted((r['submit_end_us']-r['input_begin_us'])/1000 for r in frames)
        row['input_to_submit_p99_ms']=ages[math.ceil(len(ages)*.99)-1]
        output.append(row)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('collections', nargs='+', type=Path)
    parser.add_argument('--pilot', type=Path)
    args = parser.parse_args()
    manifests, runs, rows = [], [], []
    for root in args.collections:
        manifest, selected, values = load(root)
        if manifests:
            assert manifest['environment'] == manifests[0]['environment']
        manifests.append(manifest); runs += selected; rows += values
    for case in {r['case'] for r in rows}:
        signatures = {r['replay_signature'] for r in rows if r['case']==case and r['replay']}
        assert not signatures or (len(signatures)==1 and '' not in signatures)
        assert len({(tuple(m['canvas']),m['dpr']) for r in runs if r['case']['name']==case for m in r['memory']})==1
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'assessment.json').write_text(json.dumps(rows, indent=2)+'\n')
    with (args.output/'runs.csv').open('w') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    with tarfile.open(args.output/'raw-results.tar.gz','w:gz') as archive:
        for root in args.collections + ([args.pilot] if args.pilot else []): archive.add(root, arcname=root.name)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    cases=list(dict.fromkeys(r['case'] for r in rows))
    modes=[m for m in LABELS if any(r['mode']==m for r in rows)]
    names={'bunny30k':'30,000 bunnies','geometry1000':'1,000 geometry groups','gameplay_long':'Offline Underwatermelon'}
    lines=['# Sprite placement and paced browser scheduling', '',
           'These are opt-in experiments in the broad component snapshot path. Direct uses the same modified Release engine with threading disabled; it is not clean vanilla.', '',
           'All collections use foreground Chrome, AC power, a temporarily disabled screen saver, no focus emulation, and no CPU tracing. Every run checks visibility, focus, build identity, errors and clean exit. Dots show individual runs; bars show means.', '',
           '| Collection | Repeats per mode | Measurement |', '| --- | ---: | --- |']
    for manifest in manifests:
        measurement = ('600 warmup updates; 14,400 measured updates (~120 s)' if any(c.get('scene')=='replay' for c in manifest['cases']) else f"{manifest['seconds']} s after {manifest['warmup']} s warmup")
        lines.append(f"| {', '.join(c['name'] for c in manifest['cases'])} | {manifest['repeats']} | {measurement} |")
    lines += ['', '| Mode | Difference |', '| --- | --- |',
              '| Direct | Simulation and rendering run together on browser main. |',
              '| Current threading | Simulation and component preparation on worker; captured draws execute on main; completion scheduler (1). |',
              '| Sprite geometry on main | Sprite snapshots and batch indices cross the boundary. Main generates and uploads sprite vertices at each original draw position. Culling, sorting and batch selection stay on worker. |',
              '| Paced threading | Scheduler 3 grants at most one update credit per visible rAF. A busy worker may use an unused credit on completion; credits do not accumulate. |',
              '| Geometry on main + pacing | Both independent changes enabled. |', '',
              'All modes keep the separate model-owned-buffer experiment disabled. Worker stack reservation remains 5 MiB.', '']
    for metric,title,unit in [('ups','Update throughput','Updates/s'),('p99_ms','p99 update interval','ms'),
                               ('live_mib','Peak sampled live allocation','MiB'),('wasm_mib','WASM memory capacity','MiB')]:
        fig,axes=plt.subplots(1,len(cases),figsize=(15,5),squeeze=False)
        for ax,case in zip(axes[0],cases):
            for i,mode in enumerate(modes):
                values=[r[metric] for r in rows if r['case']==case and r['mode']==mode]
                if not values:
                    ax.text(i,.03,'Not run',transform=ax.get_xaxis_transform(),ha='center',va='bottom',rotation=90,color='#64748b',fontsize=8)
                    continue
                ax.bar(i,st.mean(values),color=COLORS[i],label=LABELS[mode])
                ax.scatter([i]*len(values),values,c='#111827',s=15,zorder=3)
            ax.set(title=names.get(case,case),ylabel=unit,xticks=[],ylim=(0,None))
            ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
        handles,labels=axes[0][0].get_legend_handles_labels()
        fig.legend(handles,labels,loc='lower center',ncol=3,fontsize=9)
        fig.suptitle(title);fig.tight_layout(rect=[0,.13,1,.94]);fig.savefig(args.output/(metric+'.png'),dpi=160);plt.close(fig)
        lines += [f'![{title}]({metric}.png)','']
    lines += ['| Workload / mode | Updates/s (range) | Mean p99 ms | Peak allocation MiB | WASM MiB |',
              '| --- | ---: | ---: | ---: | ---: |']
    for case in cases:
        for mode in modes:
            group=[r for r in rows if r['case']==case and r['mode']==mode]
            if not group:continue
            ups=[r['ups'] for r in group]
            lines.append(f'| {case} / {LABELS[mode]} | {st.mean(ups):.2f} ({min(ups):.2f}–{max(ups):.2f}) | '+
                         ' | '.join(f'{st.mean(r[k] for r in group):.2f}' for k in ['p99_ms','live_mib','wasm_mib'])+' |')
    lines += ['', '| Workload / change versus current threading | Throughput | p99 | Allocations |',
              '| --- | ---: | ---: | ---: |']
    for case in cases:
        base=[r for r in rows if r['case']==case and r['mode']=='current']
        for mode in ['deferred','paced','combined']:
            group=[r for r in rows if r['case']==case and r['mode']==mode]
            if not group:continue
            ratios=[100*(st.mean(r[k] for r in group)/st.mean(r[k] for r in base)-1) for k in ['ups','p99_ms','live_mib']]
            lines.append(f'| {case} / {LABELS[mode]} | '+' | '.join(f'{v:+.1f}%' for v in ratios)+' |')
    lines += ['', 'p99 is the 99th-percentile interval between simulation updates, reported as a histogram-bin upper bound. It measures update pacing, not physical input latency or displayed FPS. Higher throughput is better; lower p99 is better.', '',
              'Allocation is the maximum sampled live WASM allocator usage per run. WASM capacity includes free space and cannot shrink. These domains overlap and must not be added. Browser/GPU memory and energy are not established by these plots. No forced GC runs are mixed into timing results.', '',
              'Bunnymark exercises moving sprites; geometry groups exercise mesh/model preparation and act as a non-sprite control. The gameplay replay keeps fruit physics, merging, scoring, GUI and sound, excluding the optional streamed-music loader. All gameplay runs must have the same deterministic replay signature.', '',
              'This is a single-device experiment, not production acceptance. Clean vanilla, slower physical devices, representative model-heavy projects, input latency, energy, extensions and context recovery remain separate evidence requirements. Raw runs, build hashes, environment and runner sources are preserved in raw-results.tar.gz.']
    if args.pilot:
        diagnostics=diagnostic_rows(args.pilot)
        with (args.output/'diagnostic-spans.csv').open('w') as f:
            writer=csv.DictWriter(f,fieldnames=list(diagnostics[0]));writer.writeheader();writer.writerows(diagnostics)
        fig,ax=plt.subplots(figsize=(10,5))
        for i,(metric,label) in enumerate([('update_ms','Worker update'),('prepare_ms','Worker preparation'),('consume_ms','Main consumption')]):
            ax.bar([j+(i-1)*.23 for j in range(len(diagnostics))],[r[metric] for r in diagnostics],.22,label=label)
        ax.set(xticks=range(len(diagnostics)),xticklabels=[LABELS[r['mode']] for r in diagnostics],ylabel='Mean wall-time span (ms)',title='Diagnostic only: where the sprite work executes')
        ax.legend();ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True);fig.tight_layout();fig.savefig(args.output/'diagnostic-spans.png',dpi=160);plt.close(fig)
        lines += ['', '![Instrumented CPU-stage wall-time spans](diagnostic-spans.png)', '',
                  'Separate 10-second instrumented Bunnymark diagnostics, filtered to the measured window. These spans overlap; their sum is not CPU usage or energy consumption. They are excluded from performance comparison means.', '',
                  '| Mode | Update ms | Preparation ms | Consumption ms | Prepared-to-consume ms | Input-to-submit mean / p99 ms |',
                  '| --- | ---: | ---: | ---: | ---: | ---: |']
        for r in diagnostics:
            lines.append(f"| {LABELS[r['mode']]} | "+' | '.join(f'{r[k]:.3f}' for k in ['update_ms','prepare_ms','consume_ms','prepared_to_consume_ms'])+f" | {r['input_to_submit_ms']:.3f} / {r['input_to_submit_p99_ms']:.3f} |")
        lines += ['', 'Input-to-submit is an internal engine interval ending at graphics submission; it excludes GPU completion, display scanout and physical input delivery. It is not end-to-end input latency. Prepared-to-consume includes any publication wait and browser scheduling delay.']
    (args.output/'REPORT.md').write_text('\n'.join(lines)+'\n')


if __name__=='__main__':
    main()
