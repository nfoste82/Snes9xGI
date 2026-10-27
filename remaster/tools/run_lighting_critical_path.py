#!/usr/bin/env python3
"""Offline actual-renderer correlated ABBA, no GUI launch. Retain every sample."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

def pin(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def stats(values):
    v=sorted(values)
    return dict(n=len(v), **{k:v[min(len(v)-1,math.ceil(q*len(v))-1)] for k,q in
                            (("p50",.5),("p95",.95),("p99",.99),("max",1))})

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--reference',required=True);p.add_argument('--final',required=True)
    p.add_argument('--output',required=True);p.add_argument('--frames',type=int,default=61)
    p.add_argument('--corpus',required=True);p.add_argument('--events',action='store_true')
    a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
    corpus=json.loads(Path(a.corpus).read_text())
    workloads=[(name,entry['capture'],None,None) for name,entry in corpus.items()]
    base=corpus['027']['capture']
    workloads += [('synthetic_64_spheres',base,64,None),('synthetic_128_spheres',base,128,None)]
    if a.events:
        workloads += [('controlled_'+e,base,64 if e=='light_motion' else None,e) for e in
                      ('cold','profile','camera','plane','collision','map','graphics','light_motion')]
    report={'scope':'actual EndFrame + actual production renderer; offscreen drawable; replayed observations, not live gameplay',
            'binaries':{a.reference:pin(a.reference),a.final:pin(a.final)},'workloads':{}}
    for name,capture,spheres,event in workloads:
        runs=[]
        for leg,binary in enumerate((a.reference,a.final,a.final,a.reference)):
            stem=out/f'{name}-{leg}'
            env=dict(os.environ,S9X_REMASTER_DISABLE_STAGE_COUNTERS='1',LIGHTING_REPLAY_ENDFRAME='1',
                     S9X_REMASTER_LIGHTING_TRACE=str(stem)+'.jsonl',LIGHTING_OUTPUT=str(stem))
            if event:env['LIGHTING_REPLAY_EVENT']=event
            if name in ('055','061'):env['LIGHTING_PATCH_BYTE_CHECK']='1'
            command=[binary,capture,str(a.frames)]+([] if spheres is None else [str(spheres)])
            with open(str(stem)+'.log','w') as log:
                result=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
            rows=[json.loads(x) for x in Path(str(stem)+'.jsonl').read_text().splitlines()]
            run=dict(command=command,exit=result.returncode,samples=len(rows),cold=rows[0],distributions={})
            for group,selected in [('all',rows),('hot',rows[1:])]:
                run['distributions'][group]={key:stats([r[key] for r in selected]) for key in
                    ('cpu_service_ms','gpu_ms','service_sum_ms','wall_to_gpu_end_ms','queue_ms','drawable_ms','submit_to_gpu_ms')}
                run['distributions'][group]['exclusive_ms']={key:stats([r['exclusive_ms'][key] for r in selected]) for key in rows[0]['exclusive_ms']}
            runs.append(run)
        parity={suffix:all(Path(str(out/f'{name}-0')+suffix).read_bytes()==Path(str(out/f'{name}-{leg}')+suffix).read_bytes()
                           for leg in (1,2,3)) for suffix in ('.s9xrmf','.direct','.composite')}
        report['workloads'][name]=dict(capture=capture,capture_sha256=pin(capture),synthetic_spheres=spheres,
                                        controlled_event=event,runs=runs,direct_byte_parity=parity)
        (out/'summary.json').write_text(json.dumps(report,indent=2))
        print(name,parity,flush=True)
    return 0 if all(all(w['direct_byte_parity'].values()) and all(r['exit']==0 for r in w['runs']) for w in report['workloads'].values()) else 1

if __name__=='__main__':raise SystemExit(main())
