"""Publish anonymous Android benchmark aggregates without copying raw evidence."""
import argparse
import csv
import json
import math
import statistics as st
from pathlib import Path

MIB = 1048576
MODES = {'vanilla': 'Vanilla', 'poc_direct': 'PoC direct', 'threaded': 'Threaded',
         'threaded_ready': 'Threaded + ready', 'threaded_budget': 'Ready + budget'}
CASES = {'space_game': 'Space game', 'bunny10k': '10k sprites', 'bunny20k': '20k sprites',
         'bunny30k': '30k sprites', 'simulation': 'Simulation heavy', 'balanced': 'Balanced',
         'fill': 'Fill heavy', 'geometry100': '200 meshes + 500 models', 'geometry1000': '2,000 meshes + 5,000 models'}
COLORS = {'vanilla': '#64748b', 'poc_direct': '#397cc2', 'threaded': '#258477', 'threaded_ready': '#a16abb', 'threaded_budget': '#c56d2b'}


def summarize(run):
    """Explicit field selection prevents private replay contents entering exports."""
    assert run['valid'] and run['exit'] == 0 and not run['errors']
    assert run['result']['valid'] and not run['result']['engine']['is_debug']
    case, mode = run['case']['name'], run['mode']
    assert case in CASES and mode in MODES, 'Supply an approved anonymous case/mode name'
    assert run['focusEmulation'] is False
    timing=run['timing']; overflow=timing['overflow_samples']; p99=timing.get('p99_ms_upper_bound')
    if overflow:
        assert run.get('timingOverflowAllowed') is True and run['case'].get('scene')=='geometry'
    if p99 is None:
        assert overflow>timing['samples']-math.ceil(timing['samples']*.99)
        assert timing['max_ms']>timing['histogram_limit_ms']>0
    else:
        assert math.isfinite(p99) and 0<p99<=timing['histogram_limit_ms']
    samples = [m for m in run['memory'] if m['phase'] == 'measure']
    assert len(samples) >= 2 and all(m['focused'] and not m['hidden'] for m in samples)
    assert len({tuple(m['canvas']) for m in samples}) == 1
    live = [m['allocator']['allocated']/MIB for m in samples]
    replay = run['case'].get('scene') == 'replay'
    cleanup = None
    if replay:
        assert run['result']['cleanup_verified'] and run['replaySignature']
        mem = run['result']['memory']
        live += [m['allocated_bytes']/MIB for m in mem['samples']+[mem['measurement_start'], mem['measurement_end']]]
        cleanup = mem['after_delete']['allocated_bytes']/MIB
    thermals = [run['thermalBefore'], *run['thermal'], run['thermalAfter']]
    skin = [t['celsius'] for entry in thermals for t in entry['temperatures'] if t['type'] == 3]
    assert run['thermalBefore']['status'] in (0,1)
    assert run['thermalBefore']['idleCpu']['idleFraction'] >= 0.9
    assert run['power']['powered'] and run['powerAfter']['powered']
    return dict(case=case, mode=mode, repeat=run['repeat'],
                seconds=run['result']['elapsed_seconds'], updates_per_second=run['result']['update_intervals_per_second'],
                p99_ms=p99, p99_lower_bound_ms=timing['histogram_limit_ms'] if p99 is None else None,
                timing_overflow_samples=overflow, timing_samples=timing['samples'], histogram_limit_ms=timing['histogram_limit_ms'],
                peak_live_mib=max(live),
                wasm_mib=max(m['heap'] for m in samples)/MIB, cleanup_live_mib=cleanup,
                width=samples[0]['canvas'][0], height=samples[0]['canvas'][1], dpr=samples[0]['dpr'],
                start_thermal_status=run['thermalBefore']['status'], max_thermal_status=max(t['status'] for t in thermals), peak_skin_c=max(skin),
                start_skin_c=max(t['celsius'] for t in run['thermalBefore']['temperatures'] if t['type']==3),
                measurement_start_skin_c=max(t['celsius'] for t in run['thermal'][0]['temperatures'] if t['type']==3),
                battery_before_cooldown_c=run['power']['temperatureC'], battery_end_c=run['powerAfter']['temperatureC'],
                start_battery_c=max((t['celsius'] for t in run['thermalBefore']['temperatures'] if t['type']==2),default=None),
                idle_cpu_fraction_before=run['thermalBefore']['idleCpu']['idleFraction'],
                replay_validated=replay)


def load(collections):
    rows, environments, browsers, gpus, timelines = [], [], set(), set(), []
    for root in collections:
        manifest = json.loads((root/'manifest.json').read_text())
        assert manifest['environment']['platform'] == 'android'
        assert not manifest['headless'] and not manifest['metrics'] and manifest['memoryProbe']
        assert not manifest['stackMeasure'] and not manifest['diagnostics'] and not manifest['memoryDiagnostic']
        expected = {(c['name'], m, n+1) for c in manifest['cases'] for m in manifest['modes'] for n in range(manifest['repeats'])}
        runs = [json.loads((root/(key+'.json')).read_text()) for key in manifest['runs']]
        assert len(runs) == len(expected) and {(r['case']['name'],r['mode'],r['repeat']) for r in runs} == expected
        for case in manifest['cases']:
            subset = [r for r in runs if r['case']['name'] == case['name']]
            if case.get('scene') == 'replay':
                assert len({r['replaySignature'] for r in subset}) == 1
            assert len({(tuple(m['canvas']),m['dpr']) for r in subset for m in r['memory']}) == 1
        environments.append(manifest['environment'])
        for run in runs:
            rows.append(summarize(run)); browsers.add(run['browser'])
            if run['case'].get('scene') == 'replay':
                samples=run['result']['memory']['samples']
                for a,b in zip(samples,samples[10:]):
                    timelines.append(dict(case=run['case']['name'],mode=run['mode'],repeat=run['repeat'],
                                          simulated_seconds=(a['tick']+b['tick'])/120,
                                          updates_per_second=(b['tick']-a['tick'])/(b['seconds']-a['seconds'])))
            gpus.update(g['deviceString'] for g in run['gpu'])
    assert all(e == environments[0] for e in environments)
    return rows, environments[0], sorted(browsers), sorted(gpus), timelines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('collections', type=Path, nargs='+')
    args = parser.parse_args()
    rows, env, browsers, gpus, timelines = load(args.collections)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')
    with (args.output/'runs.csv').open('w') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    groups = {}
    for row in rows: groups.setdefault((row['case'],row['mode']),[]).append(row)
    cases = list(dict.fromkeys(r['case'] for r in rows))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.size':10, 'axes.spines.top':False,'axes.spines.right':False})
    for keys, filename, title, units in [(['updates_per_second','p99_ms'],'performance.png','Android Chrome throughput and pacing',['Updates/s','ms']),
                                         (['peak_live_mib','wasm_mib'],'memory.png','Android Chrome memory',['MiB','MiB'])]:
        fig, axes = plt.subplots(len(cases),2,figsize=(12,max(4,3.2*len(cases))),squeeze=False)
        for i,case in enumerate(cases):
            modes=[m for m in MODES if (case,m) in groups]
            for ax,key,unit in zip(axes[i],keys,units):
                for j,mode in enumerate(modes):
                    group=groups[(case,mode)]; vals=[r[key] for r in group if r[key] is not None]
                    censored=[r['p99_lower_bound_ms'] for r in group if r[key] is None]
                    if censored:
                        ax.scatter([j]*len(censored),censored,c=COLORS[mode],marker='^',s=50,zorder=3)
                        label=f'>{max(censored):.0f} in {len(censored)}/{len(group)} runs'
                    else:
                        avg=st.mean(vals);ax.bar(j,avg,color=COLORS[mode]);label=f'{avg:.2f}'
                    ax.scatter([j]*len(vals),vals,c='#17202b',s=18,zorder=3)
                    ax.annotate(label,(j,max(vals+censored)),xytext=(0,6),textcoords='offset points',ha='center',fontsize=9)
                ax.set(xticks=range(len(modes)),xticklabels=[MODES[m].replace(' + ','\n+ ') for m in modes],title=('2k meshes + 5k models' if case=='geometry1000' else CASES[case])+' — '+{'updates_per_second':'updates/s','p99_ms':'p99 interval','peak_live_mib':'peak live allocation','wasm_mib':'WASM capacity'}[key],ylabel=unit,ylim=(0,None))
                ceiling=max(r[key] if r[key] is not None else r['p99_lower_bound_ms'] for mode in modes for r in groups[(case,mode)])
                ax.set_ylim(0,ceiling*1.22); ax.grid(axis='y',alpha=.2); ax.set_axisbelow(True)
        fig.suptitle(title,y=1-.08/fig.get_figheight());fig.tight_layout(rect=(0,0,1,1-.55/fig.get_figheight()));fig.savefig(args.output/filename,dpi=140);plt.close(fig)
    fig,ax=plt.subplots(figsize=(12,4))
    for mode in MODES:
        points=[(i+1,r['peak_skin_c']) for i,r in enumerate(rows) if r['mode']==mode]
        if points: ax.scatter(*zip(*points),label=MODES[mode],color=COLORS[mode])
    ax.set(xlabel='Accepted run in collection order',ylabel='Peak skin temperature (°C)',title='Device temperature during accepted runs');ax.legend();ax.grid(alpha=.2)
    fig.tight_layout();fig.savefig(args.output/'temperature.png',dpi=160);plt.close(fig)
    if timelines:
        (args.output/'throughput-timeline.json').write_text(json.dumps(timelines,indent=2)+'\n')
        fig,ax=plt.subplots(figsize=(12,4.5))
        for mode in MODES:
            repeats=sorted({r['repeat'] for r in timelines if r['mode']==mode})
            for n,repeat in enumerate(repeats):
                points=[r for r in timelines if r['mode']==mode and r['repeat']==repeat]
                ax.plot([r['simulated_seconds'] for r in points],[r['updates_per_second'] for r in points],color=COLORS[mode],alpha=.65,label=MODES[mode] if n==0 else None)
        ax.set(xlabel='Simulated seconds',ylabel='Updates/s',title='Space game throughput over matched replay progression (10-sample windows)')
        ax.legend();ax.grid(alpha=.2);fig.tight_layout();fig.savefig(args.output/'throughput-timeline.png',dpi=160);plt.close(fig)
    lines=['# '+env['device']+' web rendering comparison — sustained plugged-in runs','',
           'Device: **'+env['device']+'**, Android '+env['os']+'; Chrome '+', '.join(browsers)+'. GPU: '+', '.join(gpus)+'.','',
           'Bars show run means; dots show individual repetitions. Measurements were collected on the physical phone, without CPU emulation or manually imposed CPU throttling. Normal Android thermal throttling remained active.','',
           '| Workload | Mode | Repeats | Updates/s (range) | Mean p99 ms | Peak live MiB | WASM capacity MiB |',
           '| --- | --- | ---: | ---: | ---: | ---: | ---: |']
    for case in cases:
        for mode in MODES:
            group=groups.get((case,mode))
            if not group: continue
            ups=[r['updates_per_second'] for r in group]
            censored=[r for r in group if r['p99_ms'] is None]
            p99=(f'Unavailable; {len(censored)}/{len(group)} runs >{max(r["p99_lower_bound_ms"] for r in censored):.0f} ms' if censored else f'{st.mean(r["p99_ms"] for r in group):.2f}')
            lines.append(f'| {CASES[case]} | {MODES[mode]} | {len(group)} | {st.mean(ups):.2f} ({min(ups):.2f}–{max(ups):.2f}) | {p99} | '+ ' | '.join(f'{st.mean(r[k] for r in group):.2f}' for k in ['peak_live_mib','wasm_mib'])+' |')
    lines += ['', '## Changes relative to PoC direct', '', '| Workload | Mode | Throughput | p99 interval | Live allocation |','| --- | --- | ---: | ---: | ---: |']
    for case in cases:
        control=groups[(case,'poc_direct')]
        for mode in ['threaded','threaded_ready']:
            group=groups.get((case,mode))
            if not group: continue
            values=[None if any(r[k] is None for r in group+control) else (st.mean(r[k] for r in group)/st.mean(r[k] for r in control)-1)*100 for k in ['updates_per_second','p99_ms','peak_live_mib']]
            lines.append(f'| {CASES[case]} | {MODES[mode]} | '+' | '.join('Unavailable' if v is None else f'{v:+.1f}%' for v in values)+' |')
    lines += ['', '![Throughput and pacing](performance.png)', '', '![Memory](memory.png)', '',
              '## Method and definitions','',
              '- **Vanilla:** unmodified engine source at `5f5706cb09a646358da203c7c59b95269c54910f`, sharing the optimized pthread-capable platform and common replay probes. This is not the ordinary shipping non-pthread bundle.',
              '- **PoC direct:** modified engine with threading disabled.',
              '- **Threaded:** simulation and preparation on a pthread; browser main consumes component snapshots and prepares deferred sprite geometry. Two slots and a 2 MiB worker stack.',
              '- **Threaded + ready:** same component model with experimental scheduler 4, allowing completion callbacks to use an unused browser-tick render credit.',
              '- **Updates/s:** completed update intervals per wall second. This includes browser scheduling and is not isolated CPU time or confirmed displayed FPS.',
              '- **p99:** 99% of measured update intervals fall at or below this duration. Tables average per-run histogram p99 upper bounds; lower is better. This is not input-to-photon latency.',
              '- **Live allocation:** sampled WASM allocator bytes, including engine, game, worker stack and instrumentation. Short peaks can be missed. **WASM capacity** also includes free allocator space and does not shrink when allocations are freed. Do not add these measures together.',
              '- Geometry stress runs may exceed the 1,000 ms histogram range. A missing p99 is exported as null with an explicit lower bound; triangles mark censored values, and no mean p99 or p99 percentage is computed for an affected group. Throughput and allocator measurements remain separate. With only a few dozen updates in a stress run, p99 is a sparse tail observation.',
              '- Process RSS, physical GPU memory, energy and physical input latency were not measured.', '',
              'Each mode starts a fresh installed Chrome process. The phone stays on external power with Battery Saver off, fixed landscape orientation and screen sleep inhibited. Real focus/visibility checks remain active. Mode order rotates between repeats. No CPU tracing, stack painting or forced-GC diagnostic runs enter the comparison.', '',
              'Before each run, Android must report thermal status 0 or 1 (none/light) and at least 90% aggregate CPU idle with Chrome stopped. This is a sustained plugged-in comparison: the powered device did not consistently return to a cold-start temperature, so there is no fixed Celsius cutoff. Starting temperature is recorded and can vary. Brightness is dimmed only while waiting, then restored before launch. Thermal samples are taken about every ten seconds; later thermal changes remain in the results. The OS thermal protections remain active. These warm, powered results do not isolate raw CPU capacity, and a status of 0 does not prove fixed CPU/GPU frequencies.', '',
              '![Temperature](temperature.png)', '',
              f'Maximum Android thermal status observed: **{max(r["max_thermal_status"] for r in rows)}**. Peak sampled skin temperature: **{max(r["peak_skin_c"] for r in rows):.1f}°C**.', '',
              '## Workloads and validity', '']
    if 'space_game' in cases:
        lines += ['![Throughput over replay progression](throughput-timeline.png)', '', 'Each line is one repeat, using differences between memory-probe ticks and wall times across ten sample intervals (approximately ten seconds). The workload changes over the replay, so declining throughput alone is not proof of thermal throttling.', '']
        lines += ['The anonymous space game uses the same frozen project and engine bundles as the Mac comparison: 600 warmup ticks followed by 14,400 measured updates at a fixed simulation step. All accepted modes/repeats must match deterministic checkpoints and pass scene cleanup. Running WebAudio is checked during measurement. Project-specific gameplay, code, dependencies, identifiers and raw evidence remain private.', '']
    for case in cases:
        if case.startswith('bunny'): lines.append(f'- **{CASES[case]}:** moving sprite workload; actual drawing buffer '+str(groups[(case,'poc_direct')][0]['width'])+'×'+str(groups[(case,'poc_direct')][0]['height'])+'.')
        elif case=='simulation': lines.append('- **Simulation heavy:** 1,000 objects with 1,000,000 synthetic simulation iterations per update.')
        elif case=='balanced': lines.append('- **Balanced:** 10,000 moving objects with 100,000 synthetic simulation iterations per update.')
        elif case=='fill': lines.append('- **Fill heavy:** 10,000 sprites, 64-pixel size, four rendering passes; intended to expose fill/bandwidth limits.')
        elif case=='geometry1000': lines.append('- **Model/mesh:** 1,000 groups containing 2,000 meshes and 5,000 models, including CPU/GPU-skinned and instanced animated models, exercising geometry preparation and rendering.')
    lines += ['', 'Synthetic timings use 10 seconds of warmup and 30 seconds of measurement per run and are compared only within the same workload/build; their gameplay paths are not deterministic real-game replays. The synthetic group does not include a clean-vanilla control. Three repetitions on one phone are evidence for this device, not a universal production guarantee.', '',
              '## Rejected setup attempts','',
              'Initial setup attempts and the earlier incomplete collection are excluded. A busy Play Store process was found during the initial setup; later the powered device failed its thermal readiness wait. Those attempts and separate vanilla reference observations remain outside these means. After the user requested a retry, the game comparison started again from scratch with the same frozen bundles and the unchanged status 0/1 and CPU-idle admission policy. The completed retry and synthetic collection retain all accepted runs, including later thermal throttling. Starting temperatures vary and external-power heat remains a limitation; this is not a battery-only comparison.', '',
              '## Published evidence','',
              '[Anonymous numeric run data](runs.csv) is also available as [JSON](runs.json). No raw archives, game screenshots, replay state, project sources or content hashes are exported by this report. Reproducing the external-project replay requires separately authorized private inputs.']
    if not any(r['mode']=='vanilla' for r in rows):
        lines=[line for line in lines if not line.startswith('- **Vanilla:**')]
    if not any(c.startswith('geometry') for c in cases):
        lines=[line for line in lines if not line.startswith('- Geometry stress runs')]
    if cases==['space_game']:
        lines=[line for line in lines if not line.startswith('Synthetic timings use')]
    else:
        lines += ['', 'Geometry stress measurements use an explicitly separate collection that records histogram overflow. The earlier strict-policy geometry attempt is excluded; the six other synthetic workloads use the original frozen runner and unchanged validation. No engine binary was changed for the timing policy.']
    (args.output/'REPORT.md').write_text('\n'.join(lines)+'\n')

if __name__=='__main__': main()
