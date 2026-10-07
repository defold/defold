"""Compare quiet Pixel pacing runs; publish only approved aggregate fields."""
import argparse
import csv
import json
import math
import statistics as st
from pathlib import Path
from android_report import load, CASES, MODES, COLORS
from update_diagnostics import attribute

CAPACITIES = ('all_sprite_frame_capacity_bytes', 'component_frame_capacity_bytes',
              'component_upload_capacity_bytes', 'component_constant_capacity_bytes',
              'mixed_packet_capacity_bytes', 'mixed_upload_capacity_bytes', 'thread_queue_bytes')


def validate_pairing(manifests):
    seen=set()
    for manifest in manifests:
        assert not manifest['statusReports'] and not manifest['memorySamples']
        bundles=manifest['bundle']
        assert len({json.dumps(bundles[mode],sort_keys=True) for mode in manifest['modes']})==1, 'Modes must use identical engine/content bundles'
        keys={(case['name'],mode,n+1) for case in manifest['cases'] for mode in manifest['modes'] for n in range(manifest['repeats'])}
        assert not seen.intersection(keys), 'Repeated evidence across collections'
        seen.update(keys)


def gates(rows):
    groups = {}
    for row in rows:
        groups.setdefault((row['case'], row['mode']), []).append(row)
    result = []
    for case in dict.fromkeys(r['case'] for r in rows):
        if (case, 'threaded_budget') not in groups:
            continue
        candidate = groups[case, 'threaded_budget']
        if case == 'space_game' and len(candidate) < 3:
            continue # A single compatibility replay is not a repeated performance gate.
        direct = groups[case, 'poc_direct']
        reference = groups[case, 'threaded_ready']
        ratio = st.mean(r['updates_per_second'] for r in candidate) / st.mean(r['updates_per_second'] for r in reference)
        p99_ratio = None
        if all(r['p99_ms'] is not None for r in candidate + direct):
            p99_ratio = st.mean(r['p99_ms'] for r in candidate) / st.mean(r['p99_ms'] for r in direct)
        result.append(dict(case=case, throughput_ratio_to_ready=ratio,
                           throughput_90_percent_pass=ratio >= .9,
                           p99_ratio_to_direct=p99_ratio,
                           sprite_p99_105_percent_pass=(p99_ratio <= 1.05 if p99_ratio is not None else False) if case.startswith('bunny') else None))
    return result


def diagnostics(root):
    manifest = json.loads((root/'manifest.json').read_text())
    assert manifest['metrics'] and manifest['diagnostics']
    output = []
    for key in manifest['runs']:
        run = json.loads((root/(key+'.json')).read_text())
        assert run['valid'] and not run['errors']
        if not run.get('diagnostics'):
            continue
        assert run['case']['name'] in CASES and run['mode'] in MODES
        run['result']['measurement_clock_end'] = run['result']['measurement_clock_start'] + run['result']['elapsed_seconds']
        with (root/(key+'.csv')).open() as stream:
            assert next(stream).startswith('# dropped=0;')
            trace = [{k:int(v) for k,v in row.items()} for row in csv.DictReader(stream)]
        intervals = attribute(run, trace)
        tail = sorted(intervals, key=lambda row:row['interval_ms'])[-math.ceil(len(intervals)*.01):]
        row = dict(case=run['case']['name'], mode=run['mode'], samples=len(intervals),
                   quiet=manifest.get('statusReports') is False and manifest['memorySamples'] is False,
                   updates_per_second=run['result']['update_intervals_per_second'],
                   p99_ms=run['timing'].get('p99_ms_upper_bound'))
        for field in ('interval_ms','worker_ms','admission_gap_ms','worker_wake_ms','simulation_ms',
                      'preparation_ms','publication_wait_ms','consumption_ms','ready_queue_ms'):
            row[field] = st.mean(r[field] for r in intervals)
            row['tail_'+field] = st.mean(r[field] for r in tail)
        output.append(row)
    return output


def capacities(collections):
    result=[]
    for root in collections:
        manifest=json.loads((root/'manifest.json').read_text())
        for key in manifest['runs']:
            run=json.loads((root/(key+'.json')).read_text())
            if run['case'].get('scene') == 'replay' or run['mode']=='poc_direct':
                continue
            sample=run['finalSnapshot']
            assert sample['thread_slot_count']==2 and sample['thread_max_outstanding']<=1
            assert run['case']['name'] in CASES and run['mode'] in MODES
            row=dict(case=run['case']['name'],mode=run['mode'],repeat=run['repeat'],slots=2,max_outstanding=sample['thread_max_outstanding'])
            for field in CAPACITIES:
                row[field]=sample[field]
            admission=run['renderAdmission']
            assert admission['retired']<=admission['browserTicks']+1
            row.update(ready_consumptions=admission['readyRenders'],retired=admission['retired'],
                       budget_ms=admission['budgetMs'],over_budget_callbacks=admission['budgetDeferrals'])
            result.append(row)
    return result


def capacity_gates(rows):
    result=[]
    for case in dict.fromkeys(r['case'] for r in rows):
        reference=[r for r in rows if r['case']==case and r['mode']=='threaded_ready']
        candidate=[r for r in rows if r['case']==case and r['mode']=='threaded_budget']
        assert reference and candidate
        increases={field:max(r[field] for r in candidate)-max(r[field] for r in reference)
                   for field in CAPACITIES if max(r[field] for r in candidate)>max(r[field] for r in reference)}
        result.append(dict(case=case,no_extra_capacity_pass=not increases,increased_bytes=increases))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('collections',type=Path,nargs='+')
    parser.add_argument('--diagnostic',type=Path,action='append',default=[])
    args=parser.parse_args()
    rows,env,browsers,_,_=load(args.collections)
    validate_pairing([json.loads((root/'manifest.json').read_text()) for root in args.collections])
    cap=capacities(args.collections)
    checks=gates(rows)
    memory_checks=capacity_gates(cap)
    diag=[dict(trace_set=i+1,**row) for i,root in enumerate(args.diagnostic) for row in diagnostics(root)]
    args.output.mkdir(parents=True,exist_ok=True)
    for name,data in [('runs',rows),('gates',checks),('capacity-gates',memory_checks),('capacities',cap),('diagnostics',diag)]:
        (args.output/(name+'.json')).write_text(json.dumps(data,indent=2)+'\n')
    groups={}
    for row in rows:groups.setdefault((row['case'],row['mode']),[]).append(row)
    cases=list(dict.fromkeys(r['case'] for r in rows))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})
    for filename,columns,title in [
        ('performance',[('updates_per_second','Updates/s'),('p99_ms','p99 interval (ms)')],'Pixel 4a — quiet update throughput and pacing'),
        ('memory',[('peak_live_mib','Peak live (MiB)'),('wasm_mib','WASM capacity (MiB)')],'Pixel 4a — allocator memory and WASM capacity')]:
        fig,axes=plt.subplots(len(cases),2,figsize=(12,len(cases)*3.2),squeeze=False)
        for i,case in enumerate(cases):
            modes=[m for m in MODES if (case,m) in groups]
            for ax,(field,label) in zip(axes[i],columns):
                ceiling=0
                for j,mode in enumerate(modes):
                    values=[r[field] for r in groups[case,mode] if r[field] is not None]
                    bounds=[r['p99_lower_bound_ms'] for r in groups[case,mode] if r[field] is None]
                    if bounds:
                        ax.scatter([j]*len(bounds),bounds,color=COLORS[mode],marker='^',s=60)
                        text=f'>1,000: {len(bounds)}/{len(groups[case,mode])} runs'
                    else:
                        avg=st.mean(values);ax.bar(j,avg,color=COLORS[mode]);text=f'{avg:.2f}'
                    ax.scatter([j]*len(values),values,color='#17202b',s=16,zorder=3)
                    top=max(values+bounds);ceiling=max(ceiling,top)
                    ax.annotate(text,(j,top),xytext=(0,5),textcoords='offset points',ha='center',fontsize=9)
                ax.set(title=CASES[case],ylabel=label,xticks=range(len(modes)),xticklabels=[MODES[m].replace(' + ','\n+ ') for m in modes],ylim=(0,ceiling*1.22))
                ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
        fig.suptitle(title,y=1-.08/fig.get_figheight());fig.tight_layout(rect=(0,0,1,1-.55/fig.get_figheight()))
        fig.savefig(args.output/(filename+'.png'),dpi=140);plt.close(fig)
    fig,ax=plt.subplots(figsize=(12,4))
    for mode in MODES:
        points=[(i+1,r) for i,r in enumerate(rows) if r['mode']==mode]
        if not points:continue
        ax.scatter([i for i,r in points],[r['start_skin_c'] for i,r in points],color=COLORS[mode],marker='o',facecolors='none')
        ax.scatter([i for i,r in points],[r['peak_skin_c'] for i,r in points],color=COLORS[mode],marker='x',label=MODES[mode])
    ax.set(xlabel='Accepted run in collection order',ylabel='Skin temperature (°C)',title='Start (circles) and peak (crosses) device temperature')
    ax.legend(loc='center left',bbox_to_anchor=(1.01,.5));ax.grid(alpha=.2);fig.tight_layout();fig.savefig(args.output/'temperature.png',dpi=140);plt.close(fig)
    lines=['# Pixel web frame-pacing milestone','',
           f"Device: {env['device']}, Android {env['os']}; Chrome {', '.join(browsers)}. {len(rows)} accepted tracing-off runs.",'',
           'Within each workload, the comparison uses identical Release PoC engine/content bundles for every mode. Direct disables threading; it is not a fresh vanilla engine comparison. Threaded uses completion dispatch with rAF consumption. Ready permits consumption on worker completion using an unused browser-tick credit. Budgeted ready additionally defers outside-rAF consumption when credit age plus the previous consumption duration exceeds 32 ms. The budget is an estimate, not a preemptive deadline or GPU timing. Threaded variants use overlapping component preparation, deferred sprite geometry on browser main and 2 MiB worker stacks. All threading and scheduling switches remain opt-in.','',
           'Periodic Bunnymark status output and synthetic snapshot-output sampling are disabled in this collection. The previous benchmark emitted those messages during measurement. Worker stdout synchronously proxies to browser main, so logging could wait behind rendering. The quiet control changes measurement overhead, not the gameplay work. Do not attribute that improvement to the readiness budget. Sparse browser allocator sampling and the bounded timing histogram remain enabled.','',
           'Bars are means; dots retain every accepted repetition. Modes rotate between repetitions. A space-game row with one run is a compatibility check, not a repeated performance verdict, and is excluded from the target gates. The phone remains connected to power with normal thermal management; runs begin at thermal status 0 or 1 and at least 90% aggregate CPU idle. Later throttling is retained. No energy or physical input-latency measurement is implied.','',
           '| Workload | Mode | Runs | Updates/s | p99 ms | Peak live MiB | WASM MiB |',
           '|---|---|---:|---:|---:|---:|---:|']
    for (case,mode),group in groups.items():
        percentile='unavailable (censored)' if any(r['p99_ms'] is None for r in group) else f"{st.mean(r['p99_ms'] for r in group):.2f}"
        lines.append(f"| {CASES[case]} | {MODES[mode]} | {len(group)} | {st.mean(r['updates_per_second'] for r in group):.2f} | {percentile} | {st.mean(r['peak_live_mib'] for r in group):.2f} | {st.mean(r['wasm_mib'] for r in group):.2f} |")
    lines+=['','![Throughput and p99](performance.png)','','![Live allocation and WASM capacity](memory.png)','',
            '![Start and peak device temperature](temperature.png)','',
            '## Acceptance checks','',
            'These are engineering targets on run means, not statistical confidence intervals. The sprite targets are at least 90% of unbudgeted-ready throughput and p99 no greater than 105% of direct. Other workloads also need review for regressions.','',
            '| Workload | Budgeted / ready throughput | ≥90% | Budgeted / direct p99 | Sprite p99 target |',
            '|---|---:|---|---:|---|']
    for c in checks:
        ratio='unavailable' if c['p99_ratio_to_direct'] is None else f"{c['p99_ratio_to_direct']*100:.1f}%"
        status='not a sprite gate' if c['sprite_p99_105_percent_pass'] is None else ('pass' if c['sprite_p99_105_percent_pass'] else 'FAIL')
        lines.append(f"| {CASES[c['case']]} | {c['throughput_ratio_to_ready']*100:.1f}% | {'pass' if c['throughput_90_percent_pass'] else 'FAIL'} | {ratio} | {status} |")
    lines+=['','Retained-capacity checks compare the largest final value of each accounting counter against unbudgeted ready in the same collection:','']
    for c in memory_checks:
        lines.append(f"- {CASES[c['case']]}: {'pass — no increased frame/queue capacity' if c['no_extra_capacity_pass'] else 'FAIL — increased counters: '+str(c['increased_bytes'])}.")
    lines+=['','## What the numbers mean','',
            '- Updates/s counts simulation-update intervals, not confirmed displayed frames. p99 is the 99th percentile of wall-clock update intervals, rounded upward to a 0.05 ms histogram bin. It is not CPU utilization or physical input latency.',
            '- Live allocation is sampled WASM allocator usage including the worker stack. WASM capacity includes unused space; the two columns overlap and must not be added. Neither includes all JavaScript/browser memory or GPU allocations. Android process RSS was not available.',
            '- Geometry100 contains 200 meshes and 500 models. Geometry1000 contains 2,000 meshes and 5,000 models; its one-second histogram limit can censor p99. Few measured frames make that stress case unsuitable for precise tail comparisons.',
            '- The queue retains two frame slots with at most one submitted frame outstanding. Capacity counters sampled after synthetic measurement are in [capacities.json](capacities.json); they are accounting categories, not an additive process-memory total.',
            '- The generic space-game entry, when present, verifies deterministic replay checkpoints and cleanup. Raw project content, replay state, logs, adapters and bundle identities remain private and are not copied into this report.','',
            '## Diagnostic evidence','',
            'Traces are separate from acceptance runs. Their spans measure elapsed time, including waits; overlapping spans must not be summed as CPU usage. Tail columns refer to the longest 1% of worker-start intervals and do not imply the previous frame caused the entire delay. Quiet diagnostic controls are investigative single runs, not repeated performance verdicts.','',
            'Trace-set numbers follow the command-line input order; they do not identify private source paths.','',
            '| Trace set | Workload | Mode | Quiet | Mean worker ms | Tail simulation ms | Tail publish wait ms | Mean consumption ms |',
            '|---|---|---|---|---:|---:|---:|---:|']
    for d in diag:
        lines.append(f"| {d['trace_set']} | {CASES[d['case']]} | {MODES[d['mode']]} | {d['quiet']} | {d['worker_ms']:.2f} | {d['tail_simulation_ms']:.2f} | {d['tail_publication_wait_ms']:.2f} | {d['consumption_ms']:.2f} |")
    lines+=['','[Per-run anonymous metrics](runs.json) · [Target calculations](gates.json) · [Diagnostic aggregates](diagnostics.json)','']
    (args.output/'REPORT.md').write_text('\n'.join(lines))


if __name__=='__main__': main()
