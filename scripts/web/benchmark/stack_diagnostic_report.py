"""Report opt-in worker stack comparison with separate update-timing diagnostics."""
import argparse
import csv
import json
import statistics as st
import tarfile
from pathlib import Path
from placement_report import load
from update_diagnostics import read as read_diagnostics

MIB=1048576
LABELS={'direct':'Direct (same PoC engine)','deferred_5m':'Threaded, 5 MiB stack','deferred_2m':'Threaded, 2 MiB stack'}
NAMES={'gameplay_long':'Underwatermelon replay','bunny30k':'30,000 bunnies','geometry1000':'1,000 geometry groups'}
COLORS=['#64748b','#c48653','#287d91']


def memory_breakdown(run):
    samples=run['snapshots']
    def capacity(key):return st.median(s.get(key,0) for s in samples)/MIB
    component=capacity('component_frame_capacity_bytes')
    uploads=capacity('component_upload_capacity_bytes')
    constants=capacity('component_constant_capacity_bytes')
    assert component+.0001>=uploads+constants
    return dict(case=run['case']['name'],mode=run['mode'],repeat=run['repeat'],
        worker_stack_mib=(run.get('stack') or {}).get('reserved',0)/MIB,
        sprite_frames_mib=capacity('all_sprite_frame_capacity_bytes'),
        sprite_scratch_mib=capacity('all_sprite_renderer_cpu_capacity_bytes'),
        component_metadata_mib=max(0,component-uploads-constants),
        component_uploads_mib=uploads,component_constants_mib=constants,
        gpu_sprite_logical_mib=capacity('all_sprite_gpu_logical_bytes'),
        sprite_worlds=st.median(s['all_sprite_worlds'] for s in samples))


def write_csv(path,rows):
    with path.open('w') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('output',type=Path);p.add_argument('collections',nargs='+',type=Path)
    p.add_argument('--diagnostics',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    manifests=[];runs=[];rows=[]
    for root in a.collections:
        manifest,selected,values=load(root)
        assert not manifest.get('diagnostics') and manifest['repeats']>=3
        manifests.append(manifest);runs+=selected;rows+=values
    assert all(m['environment']==manifests[0]['environment'] for m in manifests)
    for case in {r['case'] for r in rows}:
        signatures={r['replay_signature'] for r in rows if r['case']==case and r['replay']}
        assert not signatures or len(signatures)==1
        assert len({(tuple(m['canvas']),m['dpr']) for r in runs if r['case']['name']==case for m in r['memory']})==1, 'Canvas/DPR differs between modes'
    memory=[memory_breakdown(r) for r in runs]
    diagnostics=read_diagnostics(a.diagnostics)
    gameplay_signatures={r['replay_signature'] for r in rows if r['replay']}
    assert all(s['replay_signature'] in gameplay_signatures for s,_ in diagnostics)
    diagnostic_manifest=json.loads((a.diagnostics/'manifest.json').read_text())
    assert any(m['bundle']==diagnostic_manifest['bundle'] for m in manifests), 'Diagnostic and performance builds differ'
    write_csv(a.output/'runs.csv',rows);write_csv(a.output/'memory-breakdown.csv',memory)
    write_csv(a.output/'diagnostics.csv',[s for s,_ in diagnostics])
    for summary,detail in diagnostics:write_csv(a.output/(summary['mode']+'-update-attribution.csv'),detail)
    with tarfile.open(a.output/'raw-results.tar.gz','w:gz') as archive:
        for root in a.collections+[a.diagnostics]:archive.add(root,arcname=root.name)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.family':'DejaVu Sans','axes.spines.top':False,'axes.spines.right':False})
    cases=list(dict.fromkeys(r['case'] for r in rows));modes=list(LABELS)
    def avg(case,mode,field,source=rows):return st.mean(r[field] for r in source if r['case']==case and r['mode']==mode)
    for filename,metrics in [('performance',[('ups','Updates / second'),('p99_ms','p99 update interval (ms)')]),
                             ('memory',[('live_mib','Peak live WASM allocations (MiB)'),('wasm_mib','WASM capacity (MiB)')])]:
        fig,axes=plt.subplots(1,2,figsize=(12,4.5))
        for ax,(field,label) in zip(axes,metrics):
            for i,mode in enumerate(modes):
                x=[j+(i-1)*.24 for j in range(len(cases))]
                values=[avg(c,mode,field) for c in cases]
                ax.bar(x,values,.23,label=LABELS[mode],color=COLORS[i])
                for j,c in enumerate(cases):
                    ax.scatter([x[j]]*3,[r[field] for r in rows if r['case']==c and r['mode']==mode],s=13,c='#172033',zorder=3)
            ax.set(xticks=range(len(cases)),xticklabels=[NAMES[c].replace(' ','\n',1) for c in cases],ylabel=label)
            ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
        axes[0].legend(fontsize=8);fig.tight_layout();fig.savefig(a.output/(filename+'.png'),dpi=160);plt.close(fig)
    fig,axes=plt.subplots(1,len(cases),figsize=(13,4.6))
    parts=[('worker_stack_mib','Worker stack'),('sprite_frames_mib','Sprite frames'),('sprite_scratch_mib','Sprite scratch'),
           ('component_metadata_mib','Component frame metadata'),('component_uploads_mib','Component uploads'),('component_constants_mib','Component constants')]
    for ax,case in zip(axes,cases):
        bottom=[0]*len(modes)
        for field,label in parts:
            values=[avg(case,m,field,memory) for m in modes]
            ax.bar(range(len(modes)),values,bottom=bottom,label=label)
            bottom=[b+v for b,v in zip(bottom,values)]
        ax.set(xticks=range(len(modes)),xticklabels=['Direct','Threaded\n5 MiB','Threaded\n2 MiB'],title=NAMES[case],ylabel='Tracked CPU capacity (MiB)')
        ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
    handles,labels=axes[0].get_legend_handles_labels();fig.legend(handles,labels,loc='lower center',ncol=3,fontsize=8);fig.tight_layout(rect=(0,.14,1,1));fig.savefig(a.output/'tracked-capacity.png',dpi=160);plt.close(fig)
    fig,axes=plt.subplots(1,2,figsize=(11,4.3))
    stages=[('publication_wait_ms','Waiting to publish'),('worker_other_ms','Other worker time'),('admission_gap_ms','Admission gap'),('dispatch_ms','Dispatch'),('worker_wake_ms','Worker wake-up')]
    for ax,prefix,title in zip(axes,['','tail_'],['All intervals (mean)','Slowest 1% of intervals (mean)']):
        bottom=[0]*len(diagnostics)
        for field,label in stages:
            values=[max(0,s[prefix+field]) for s,_ in diagnostics]
            ax.bar(range(len(diagnostics)),values,bottom=bottom,label=label)
            bottom=[b+v for b,v in zip(bottom,values)]
        ax.set(xticks=range(len(diagnostics)),xticklabels=[LABELS[s['mode']] for s,_ in diagnostics],ylabel='Worker wake interval (ms)',title=title)
        ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
    handles,labels=axes[0].get_legend_handles_labels();fig.legend(handles,labels,loc='lower center',ncol=5,fontsize=8);fig.tight_layout(rect=(0,.12,1,1));fig.savefig(a.output/'update-attribution.png',dpi=160);plt.close(fig)
    lines=['# Web worker diagnostics and stack-memory comparison','',
        'Measured on Apple M1 Pro, macOS, Chrome '+runs[0]['browser']+' with ANGLE Metal. The executable is `dmengine_release` (`is_debug=false`), built using CMake `RelWithDebInfo` and Emscripten 4.0.6. All compared modes use identical engine binaries; runtime assertions and default stack cookies are enabled for the PoC.', '',
        'Direct uses the same modified Release PoC engine with threading disabled. Threaded modes use component snapshots, overlapping preparation, sprite geometry on browser main, and completion scheduling (`render.poc_web_schedule=1`). Only worker stack reservation differs between the two threaded modes. All switches remain opt-in; the default stack remains 5 MiB.','',
        'Performance runs use foreground Chrome, AC power, hardware graphics, no focus emulation, no CPU/update tracing and no stack painting. Bars show means of three runs; dots show individual runs. Underwatermelon measures 14,400 updates after 600 warm-up updates (about 120 seconds). The replay forces a 1/60-second simulation step in every mode; simulated time advances faster than wall time at 120 updates/s. Synthetic runs measure 30 seconds after 10 seconds of warm-up. These short synthetic runs are regression checks, not production acceptance.','',
        'Workloads: Underwatermelon retains fruit physics, merging, scoring, GUI and sound in the approved offline copy. Bunnymark moves 30,000 sprites. The geometry case renders 1,000 groups containing 2,000 meshes and 5,000 models, including CPU/GPU skinning and instancing; its small triangles emphasize component and draw overhead.', '',
        '| Workload | Mode | Updates/s | p99 ms | Peak live MiB | WASM capacity MiB |',
        '|---|---|---:|---:|---:|---:|']
    for c in cases:
        for m in modes:lines.append('| '+NAMES[c]+' | '+LABELS[m]+' | '+' | '.join(f'{avg(c,m,k):.2f}' for k in ['ups','p99_ms','live_mib','wasm_mib'])+' |')
    lines+=['','![Update throughput and tail intervals](performance.png)','','p99 is the interval below which 99% of recorded update intervals fall. The Lua histogram reports its upper bin bound. Higher updates/s is better; lower p99 is better. Neither metric measures photon latency or energy.','',
        '![Live WASM allocations and capacity](memory.png)','','Live allocation comes from the WASM allocator. WASM capacity is its backing memory, including free space, and can stay unchanged when live allocation falls. A smaller stack primarily frees allocator reservation; its untouched pages may never have been resident. This does not imply an equal drop in operating-system RSS. Browser process RSS and physical GPU memory are different quantities.','',
        '| Workload | 2 MiB versus 5 MiB: throughput | p99 | Live allocation | WASM capacity |', '|---|---:|---:|---:|---:|']
    for c in cases:
        lines.append(f"| {NAMES[c]} | {(avg(c,'deferred_2m','ups')/avg(c,'deferred_5m','ups')-1)*100:+.2f}% | {avg(c,'deferred_2m','p99_ms')-avg(c,'deferred_5m','p99_ms'):+.2f} ms | {avg(c,'deferred_2m','live_mib')-avg(c,'deferred_5m','live_mib'):+.2f} MiB | {avg(c,'deferred_2m','wasm_mib')-avg(c,'deferred_5m','wasm_mib'):+.2f} MiB |")
    lines+=['','![Tracked CPU capacity breakdown](tracked-capacity.png)','','The capacity graph shows per-run median tracked allocations, averaged across runs. It is a partial breakdown, not total live memory: Lua, resources, other component scratch, allocator overhead and engine bookkeeping remain outside it. Sprite counters include every registered collection world. Deferred render scratch is sampled from retired slots and can lag up to two frames. Component upload/constants are disjoint subsets of component-frame capacity; sprite constants are already included in scratch. GPU logical sprite buffers are separate in `memory-breakdown.csv` and are not stacked here.','',
        '## Separate timing diagnostics','','![Wake interval attribution](update-attribution.png)','','These instrumented runs are excluded from the performance figures. Sequential IDs and timestamps partition each wake interval into the preceding worker update, admission gap, main dispatch and next worker wake-up. Graphics-owner calls and publication waits are subsets of worker time. Render consumption overlaps it and must not be added to the critical-path total. Other JavaScript/extension rendezvous are not individually attributed.','',
        '| Mode | Worker mean / tail ms | Admission gap mean / tail ms | Wake mean / tail ms | Graphics queue+execute+return mean ms | Publication wait mean / tail ms | Stack touched KiB |',
        '|---|---:|---:|---:|---:|---:|---:|']
    for s,_ in diagnostics:
        pair=lambda f:f"{s[f]:.3f} / {s['tail_'+f]:.3f}"
        calls=sum(s[k] for k in ['owner_queue_ms','owner_execute_ms','owner_return_ms'])
        lines.append(f"| {LABELS[s['mode']]} | {pair('worker_ms')} | {pair('admission_gap_ms')} | {pair('worker_wake_ms')} | {calls:.3f} | {pair('publication_wait_ms')} | {s['stack_touched']/1024:.2f} |")
    lines+=['','The traced gameplay is dominated by publication backpressure: the worker waits for the previous frame to retire before publishing its prepared frame. The long ready-to-consume waits and short consumption spans are consistent with waiting on the browser render cadence in this light workload, not evidence that simulation consumes an entire core. The measured worker wake-up and graphics-owner paths do not explain the long intervals. The next targeted experiment is render admission/consumption timing within the existing bounded pipeline; adding queue depth could increase latency and requires separate justification.','','Tail columns average the same slowest 1% of wake intervals; they are not sums of independent percentiles. Absolute clock doubles can differ by one ULP across owners; validation permits at most one microsecond of rounding and preserves the raw timestamps. Stack painting observes deepest writes in these runs, not the maximum possible stack requirement of arbitrary extensions or deeper gameplay call paths.','',
        '## Evidence and limits','','Every accepted replay completed the same fixed event stream and cleanup contract. All per-case signatures matched. The 2 MiB worker stack also passed the broad fixture covering sprites, GUI, labels, tilemaps, particles, physics, sound, cameras, proxies/factories, CPU/GPU-skinned models, mesh updates, instancing, pause/resume and context-loss shutdown.','',
        'This collection does not establish clean-vanilla performance, slower-device behavior, other browsers, physical input latency, GPU completion timing or energy consumption. The two larger supplied games are tracked in [the compatibility notes](ADVANCED_PROJECTS.md); their smoke checks are excluded from these performance results.','',
        'Raw JSON/CSV, manifests, runner snapshots and build hashes: [raw-results.tar.gz](raw-results.tar.gz). Calculations: [runs.csv](runs.csv), [memory-breakdown.csv](memory-breakdown.csv), [diagnostics.csv](diagnostics.csv). Per-update attribution CSVs retain the update IDs for investigation.','', 'Additional evidence: [build provenance](provenance.json), [validation logs](validation/), [preserved diagnostic pilots](DIAGNOSTIC_PILOTS.md), and [analysis sources](analysis-sources/).','']
    (a.output/'REPORT.md').write_text('\n'.join(lines))


if __name__=='__main__':main()
