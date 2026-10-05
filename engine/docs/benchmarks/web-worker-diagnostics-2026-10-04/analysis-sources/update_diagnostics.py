"""Attribute recorded update intervals without treating overlapping spans as CPU usage."""
import csv
import json
import math
import statistics as st
from bisect import bisect_right
from pathlib import Path


def p99(values):
    return sorted(values)[math.ceil(len(values)*.99)-1] if values else 0


def attribute(run, trace):
    diagnostic=run['diagnostics']
    assert diagnostic['schema']==1 and diagnostic['dropped']==0
    rows=[dict(zip(diagnostic['fields'],r)) for r in diagnostic['rows']]
    assert all(r['id']==i+1 for i,r in enumerate(rows))
    start=run['result']['measurement_clock_start']*1000
    end=run['result']['measurement_clock_end']*1000
    starts=[r['wake'] for r in rows]
    by_update={}
    for frame in trace:
        t=frame['input_begin_us']/1000
        i=bisect_right(starts,t+.01)-1
        assert i>=0 and rows[i]['wake']-.01<=t<=rows[i]['update_end']+.01, 'Unmatched frame/update'
        assert rows[i]['id'] not in by_update, 'Two frames in one worker update'
        by_update[rows[i]['id']]=frame
    result=[]
    for previous,current in zip(rows,rows[1:]):
        if not start<=current['wake']<end:continue
        frame=by_update.get(previous['id'])
        item=dict(id=current['id'],browser_tick=current['browser_tick'],source=current['source'],
                  interval_ms=current['wake']-previous['wake'],
                  admission_gap_ms=current['dispatch_begin']-previous['update_end'],
                  dispatch_ms=current['dispatch_end']-current['dispatch_begin'],
                  worker_wake_ms=current['wake']-current['dispatch_end'],
                  worker_ms=previous['update_end']-previous['wake'],
                  owner_queue_ms=previous['owner_queue'],owner_execute_ms=previous['owner_execute'],
                  owner_return_ms=previous['owner_return'],graphics_calls=previous['graphics_calls'])
        if frame:
            # These are subsets/overlapping spans, not an additive CPU-time total.
            for name,a,b in [('simulation_ms','input_begin_us','update_end_us'),
                             ('preparation_ms','update_end_us','prepare_end_us'),
                             ('publication_wait_ms','prepare_end_us','publish_us'),
                             ('consumption_ms','render_begin_us','submit_end_us'),
                             ('ready_queue_ms','publish_us','render_begin_us')]:
                item[name]=max(0,(frame[b]-frame[a])/1000)
            assert item['publication_wait_ms']<=item['worker_ms']+.01
            item['worker_other_ms']=max(0,item['worker_ms']-item['publication_wait_ms'])
            item['dispatch_to_submit_ms']=frame['submit_end_us']/1000-previous['dispatch_begin']
        assert min(item[k] for k in ['worker_ms','admission_gap_ms','dispatch_ms','worker_wake_ms'])>=-.001
        assert abs(item['interval_ms']-sum(item[k] for k in ['worker_ms','admission_gap_ms','dispatch_ms','worker_wake_ms']))<.001
        result.append(item)
    assert result and all('preparation_ms' in r for r in result)
    return result


def read(root):
    root=Path(root)
    manifest=json.loads((root/'manifest.json').read_text())
    assert manifest['metrics'] and manifest['diagnostics']
    output=[]
    for key in manifest['runs']:
        run=json.loads((root/(key+'.json')).read_text())
        assert run['valid'] and not run['errors'] and run['result']['diagnostic_timing']
        if not run.get('diagnostics'):continue
        with (root/(key+'.csv')).open() as f:
            assert next(f).startswith('# dropped=0;')
            trace=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(f)]
        rows=attribute(run,trace)
        tail=sorted(rows,key=lambda r:r['interval_ms'])[-math.ceil(len(rows)*.01):]
        summary={'mode':run['mode'],'samples':len(rows),'interval_p99_ms':p99([r['interval_ms'] for r in rows]),
                 'stack_reserved':run['stack']['reserved'],'stack_touched':run['stack']['touched'],
                 'replay_signature':run['replaySignature']}
        for field in rows[0]:
            if field in ['id','browser_tick','source']:continue
            summary[field]=st.mean(r[field] for r in rows)
            summary['tail_'+field]=st.mean(r[field] for r in tail)
        output.append((summary,rows))
    return output
