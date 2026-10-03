"""Shared per-run calculations for web rendering experiments."""
import csv
from bisect import bisect_right
import statistics as st
import numpy as np

def mean(v):return st.mean(v) if v else 0

def percentile(v,p):return float(np.percentile(v,p)) if v else 0

def analyze(d, root):
 r=d['result'];start=r['measurement_clock_start']*1e6;end=start+r['elapsed_seconds']*1e6
 row={'case':d['case']['name'],'mode':d['mode'],'repeat':d['repeat'],'ups':r['update_intervals_per_second'],
 'wasm':max(m['heap'] for m in d['memory'])/1048576,'rss':mean([m['rss']/1048576 for m in d['memory']])}
 # CPU time covers the common process set between the first and last memory samples.
 first,last=d['memory'][0],d['memory'][-1];before={p['id']:p['cpuTime'] for p in first['processes']}
 row['cpu']=sum(max(0,p['cpuTime']-before[p['id']]) for p in last['processes'] if p['id'] in before)/(max(1,last['now']-first['now'])/1000)*100
 row['cpu_per_update']=row['cpu']*10/row['ups']
 f=root/(d['key']+'.csv')
 if not f.exists():return row
 allrows=[{k:int(v) for k,v in x.items()} for x in csv.DictReader(f.read_text().splitlines()[1:])]
 trace=[x for x in allrows if start<=x['input_begin_us']<end]
 def duration(begin,end):return [(x[end]-x[begin])/1000 for x in trace if x[end] and x[begin] and x[end]>=x[begin]]
 for name,b,e in [('simulation','input_begin_us','update_end_us'),('preparation','update_end_us','prepare_end_us'),('queue','prepare_end_us','queue_drain_end_us'),('render','render_begin_us','submit_end_us'),('age','input_begin_us','submit_end_us')]:
  vals=duration(b,e);row[name]=mean(vals);row[name+'_p99']=percentile(vals,99)
 updates=[x['input_begin_us']/1000 for x in trace];ui=np.diff(updates).tolist()
 # Submission timestamps are actual render consumption, not Lua render-script calls.
 submits=sorted(x['submit_end_us']/1000 for x in allrows if start<=x['submit_end_us']<end)
 intervals=np.diff(submits).tolist();row['fps']=len(intervals)*1000/(submits[-1]-submits[0]) if len(submits)>1 else 0
 row['update_p99']=percentile(ui,99);row['submit_p99']=percentile(intervals,99)
 row['over_60']=mean([v>1000/60+0.5 for v in intervals])*100
 row['over_120']=mean([v>1000/120+0.5 for v in intervals])*100
 row['sync_ms']=row['sync_calls']=row['retries']=row['dispatch_age']=row['dispatch_age_p99']=0
 sched=d.get('schedule');row['idle']=row['wake']=row['worker']=row['raf']=row['busy_callbacks']=0
 if sched:
  w=[x for x in sched['worker'] if x[0]==2];dispatch=[x for x in sched['main'] if x[0]==1];raf=[x for x in sched['main'] if x[0]==0 and start/1000<=x[2]<end/1000]
  row['idle']=mean([dispatch[i+1][2]-w[i][3] for i in range(min(len(w),len(dispatch)-1)) if start/1000<=w[i][2]<end/1000])
  row['wake']=mean([w[i][2]-dispatch[i][3] for i in range(min(len(w),len(dispatch))) if start/1000<=w[i][2]<end/1000])
  row['worker']=mean([x[3]-x[2] for x in w if start/1000<=x[2]<end/1000])
  calls=[x for x in sched['worker'] if x[0]==3 and start/1000<=x[2]<end/1000]
  updates=sum(start/1000<=x[2]<end/1000 for x in w)
  row['sync_ms']=sum(x[3]-x[2] for x in calls)/max(1,updates)
  row['sync_calls']=len(calls)/max(1,updates)
  row['retries']=sched.get('retries',0)
  # Match each frame to the worker update containing its input marker, then
  # include the dispatch/input snapshot and pre-simulation owner waits in age.
  starts=[x[2] for x in w];ages=[]
  for frame in trace:
   if not frame['submit_end_us']:continue
   timestamp=frame['input_begin_us']/1000
   # The CSV truncates to integer microseconds; performance.now has browser
   # quantization. Allow 10 us when matching these two clock representations.
   i=bisect_right(starts,timestamp+.01)-1
   assert 0<=i<len(w) and w[i][2]-.01<=timestamp<=w[i][3]+.01, 'Frame does not match a worker update'
   assert dispatch[i][2]<=w[i][2], 'Dispatch/update ordering mismatch'
   ages.append(frame['submit_end_us']/1000-dispatch[i][2])
  row['dispatch_age']=mean(ages);row['dispatch_age_p99']=percentile(ages,99)
  row['raf']=mean(np.diff([x[2] for x in raf]).tolist())
  row['busy_callbacks']=mean([x[1]!=0 for x in raf])*100
 return row
