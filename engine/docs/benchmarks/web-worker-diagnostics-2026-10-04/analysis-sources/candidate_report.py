"""Evaluate frozen web candidate gates and retain missing evidence explicitly."""
import argparse
import csv
import json
import statistics as st
import tarfile
from pathlib import Path

MIB = 1048576


def analyze_run(run):
    assert not run.get('result', {}).get('diagnostic_timing'), 'Replay diagnostic runs cannot be used as acceptance timings'
    assert not run.get('diagnostics'), 'Update diagnostic runs cannot be used as acceptance timings'
    assert not run.get('memoryDiagnostic'), 'GC diagnostic runs cannot be used as acceptance timings'
    assert run.get('valid') and run['exit'] == 0 and not run['errors']
    assert run['result']['valid'] and not run['result']['engine']['is_debug']
    assert run['focusEmulation'] is False
    assert run['timing']['overflow_samples'] == 0 and run['timing']['samples'] > 0
    samples = run['memory']
    assert len(samples) >= 2 and all(m['focused'] and not m['hidden'] for m in samples)
    assert len({(tuple(m['canvas']), m['dpr']) for m in samples}) == 1
    allocated = [m['allocator']['allocated']/MIB for m in samples]
    # Compare medians of the first/last quarters to avoid one noisy sample.
    # Growing gameplay populations are not a steady-state leak experiment.
    quarter = max(1, len(allocated)//4)
    return dict(case=run['case']['name'], mode=run['mode'], repeat=run['repeat'],
                seconds=run['result']['elapsed_seconds'], ups=run['result']['update_intervals_per_second'],
                p99_ms=run['timing']['p99_ms_upper_bound'], live_mib=max(allocated),
                wasm_mib=max(m['heap'] for m in samples)/MIB,
                growth_mib=st.median(allocated[-quarter:])-st.median(allocated[:quarter]),
                replay_signature=run.get('replaySignature', ''),
                replay=run['case'].get('scene') == 'replay')


def evaluate(rows, gates):
    results = []
    for case in sorted({r['case'] for r in rows}):
        baseline = [r for r in rows if r['case'] == case and r['mode'] == gates['control_mode']]
        candidate = [r for r in rows if r['case'] == case and r['mode'] == gates['candidate_mode']]
        assert baseline and candidate, 'Missing paired control/candidate for ' + case
        signatures = {r['replay_signature'] for r in baseline+candidate if r['replay']}
        assert not signatures or (len(signatures) == 1 and '' not in signatures), 'Replay mismatch'
        ratio = lambda key: st.mean(r[key] for r in candidate)/st.mean(r[key] for r in baseline)
        ups, p99, memory = [(ratio(k)-1)*100 for k in ['ups', 'p99_ms', 'live_mib']]
        # Compare duration at centisecond reporting precision: a fixed-tick replay
        # may end at 119.996 s rather than nominal 120 s due to callback phase.
        sufficiently_long = round(min(r['seconds'] for r in baseline+candidate), 2) >= gates['minimum_measurement_seconds']
        enough_repeats = all(len(group) >= gates['minimum_repeats'] for group in [baseline, candidate])
        throughput_limit = gates['target_throughput_gain_percent'] if case in gates['target_workloads'] else -5
        checks = {'duration': sufficiently_long, 'repeats': enough_repeats,
                  'throughput': ups >= throughput_limit,
                  'p99': p99 <= gates['maximum_p99_regression_percent'],
                  'live_memory': memory <= gates['maximum_live_memory_increase_percent']}
        checks['steady_state_memory'] = (None if any(r['replay'] for r in candidate) else
            max(r['growth_mib'] for r in candidate) <= gates['steady_state_growth_tolerance_mib'])
        results.append(dict(case=case, throughput_change_percent=ups, p99_change_percent=p99,
                            live_memory_change_percent=memory, checks=checks))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('collections', type=Path, nargs='+')
    args = parser.parse_args()
    rows, manifests, raw_runs = [], [], []
    for root in args.collections:
        manifest = json.loads((root/'manifest.json').read_text())
        assert not manifest['headless'] and not manifest['metrics'] and manifest['memoryProbe']
        assert manifest.get('bundle') and manifest.get('gates'), 'Freeze build hashes and gates before collection'
        assert not manifest.get('diagnostics'), 'Update diagnostic runs cannot be used as acceptance timings'
        assert not manifest.get('memoryDiagnostic'), 'GC diagnostic runs cannot be used as acceptance timings'
        assert not manifest['stackMeasure']
        expected = {(c['name'], mode, repeat+1) for c in manifest['cases']
                    for mode in manifest['modes'] for repeat in range(manifest['repeats'])}
        runs = [json.loads((root/(key+'.json')).read_text()) for key in manifest['runs']]
        assert len(runs) == len(expected) and {(r['case']['name'],r['mode'],r['repeat']) for r in runs} == expected
        assert len(manifest['runs']) == len(set(manifest['runs']))
        if manifests:
            assert manifest['gates'] == manifests[0]['gates']
            assert manifest['environment'] == manifests[0]['environment'], 'Report each device separately'
        manifests.append(manifest); raw_runs += runs
        rows += [analyze_run(r) for r in runs]
    for case in {r['case']['name'] for r in raw_runs}:
        assert len({(tuple(m['canvas']),m['dpr']) for r in raw_runs if r['case']['name']==case for m in r['memory']}) == 1
    assert len({(r['case'],r['mode'],r['repeat']) for r in rows}) == len(rows), 'Duplicate collection'
    gates = manifests[0]['gates']
    results = evaluate(rows, gates)
    missing = list(gates['required_external_evidence'])
    missing += ['target workload: '+c for c in gates['target_workloads'] if c not in {r['case'] for r in rows}]
    args.output.mkdir(parents=True, exist_ok=False)
    failed = any(value is False for result in results for value in result['checks'].values())
    outcome = dict(status='NOT_READY', reason=('Local gates failed; production evidence remains incomplete' if failed else 'Production evidence remains incomplete'),
                   provisional_gates=gates, duration_comparison_precision_seconds=0.01, workloads=results, missing=missing)
    (args.output/'assessment.json').write_text(json.dumps(outcome, indent=2)+'\n')
    with (args.output/'runs.csv').open('w') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    with tarfile.open(args.output/'raw-results.tar.gz','w:gz') as archive:
        for root in args.collections: archive.add(root, arcname=root.name)

    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    cases = list(dict.fromkeys(r['case'] for r in rows))
    modes = list(dict.fromkeys(r['mode'] for r in rows))
    colors=['#526b85','#138c7a','#d39133','#886bad']
    case_names={'bunny30k':'30k bunnies','geometry1000':'1,000 geometry groups','gameplay_long':'Offline Underwatermelon'}
    mode_names={'direct':'Direct','overlap_completion':'Candidate (broad + completion)'}
    lines=['# Web production-candidate evaluation','',
           '**Status: not ready for production.** Gates are provisional and frozen before collection.','',
           'Direct is the same modified Release build with threading disabled, not clean vanilla. Each bar is the mean of run values; dots show individual runs.','']
    for metric, title, unit in [('ups','Update throughput','Updates/s'),('p99_ms','p99 update interval','ms'),
                                 ('live_mib','Sampled peak live WASM allocations','MiB'),('wasm_mib','WASM capacity','MiB')]:
        fig, ax=plt.subplots(figsize=(11,4.8)); width=.8/len(modes)
        for i,mode in enumerate(modes):
            for j,case in enumerate(cases):
                values=[r[metric] for r in rows if r['case']==case and r['mode']==mode]
                if not values: continue
                x=j+(i-(len(modes)-1)/2)*width
                ax.bar(x,st.mean(values),width*.9,color=colors[i%len(colors)],label=mode_names.get(mode,mode) if j==0 else None)
                ax.scatter([x]*len(values),values,s=12,c='#17202b',zorder=3)
        ax.set(xticks=range(len(cases)),xticklabels=[case_names.get(c,c) for c in cases],ylabel=unit,title=title,ylim=(0,None))
        ax.legend(); ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True);fig.tight_layout()
        fig.savefig(args.output/(metric+'.png'),dpi=150);plt.close(fig)
        lines += [f'![{title}]({metric}.png)','']
    fig, axes=plt.subplots(1,len(cases),figsize=(15,4.5),squeeze=False)
    for ax,case in zip(axes[0],cases):
        for i,mode in enumerate(modes):
            selected=[r for r in raw_runs if r['case']['name']==case and r['mode']==mode]
            for j,run in enumerate(selected):
                samples=run['memory']; start=samples[0]['now']
                ax.plot([(m['now']-start)/1000 for m in samples],
                        [m['allocator']['allocated']/MIB for m in samples],
                        color=colors[i%len(colors)],alpha=.7,label=mode_names.get(mode,mode) if j==0 else None)
        ax.set(title=case_names.get(case,case),xlabel='Seconds since first sample',ylabel='Live allocations (MiB)')
        ax.grid(alpha=.2)
    axes[0][0].legend(fontsize=8);fig.tight_layout();fig.savefig(args.output/'memory_timeline.png',dpi=150);plt.close(fig)
    lines += ['![Allocation timelines for all repeats](memory_timeline.png)',
              'Each line is one run; axes have separate memory scales. Gameplay samples can include final cleanup.','']
    lines += ['| Workload / mode | Updates/s | Mean p99 ms | Live MiB | WASM MiB | Largest sampled growth MiB |',
              '| --- | ---: | ---: | ---: | ---: | ---: |']
    for case in cases:
        for mode in modes:
            group=[r for r in rows if r['case']==case and r['mode']==mode]
            if group:
                lines.append(f'| {case} / {mode} | '+ ' | '.join(f'{st.mean(r[k] for r in group):.2f}' for k in ['ups','p99_ms','live_mib','wasm_mib'])+f' | {max(r["growth_mib"] for r in group):.2f} |')
    lines += ['', '| Workload | UPS change | p99 change | Live memory change | Gate results |', '| --- | ---: | ---: | ---: | --- |']
    for r in results:
        checks=', '.join(k+': '+('unproven' if v is None else 'pass' if v else 'FAIL') for k,v in r['checks'].items())
        lines.append('| '+r['case']+' | '+' | '.join(f'{r[k]:+.1f}%' for k in ['throughput_change_percent','p99_change_percent','live_memory_change_percent'])+' | '+checks+' |')
    lines += ['', 'Duration is compared at 0.01-second precision (fixed-tick runs here can finish a few milliseconds before nominal 120 seconds); exact durations remain in the CSV. Memory growth compares medians of the first and last quarters of samples. It is a screening signal, not proof of a leak or of leak freedom. Growing gameplay populations cannot pass a steady-state memory gate. p99 measures update intervals, not physical input latency. Memory domains overlap and must not be added.','',
              'External evidence still required: '+ '; '.join(missing)+'.','',
              '[Per-run measurements](runs.csv), [machine-readable gates](assessment.json), [raw evidence](raw-results.tar.gz).','']
    (args.output/'REPORT.md').write_text('\n'.join(lines))


if __name__ == '__main__': main()
