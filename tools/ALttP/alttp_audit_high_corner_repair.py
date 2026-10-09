"""Compare exact runtime solvers on all available complete default-state contexts.

Research only; no profile replacement, movement approval or dynamic-state claim.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('corpus','profile','rom','before','after','output'):
        p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    if not a.output.is_dir():p.error('output must exist')
    profile=a.profile.read_bytes()
    pinned=a.output/'profile.toml';pinned.write_bytes(profile)
    def audit(raw):
        state_path=Path(str(raw)+'.state.json');attr=Path(str(raw)+'.attr')
        r=dict(snapshot=str(raw),approval=False)
        if not state_path.exists() or not attr.exists():return r|dict(status='missing_inputs')
        state=json.loads(state_path.read_text())
        args=[str(raw),str(attr),str(pinned),*map(str,state['link_xy']),str(state['plane']),'--rom',str(a.rom)]
        results=[]
        for solver in (a.before,a.after):
            run=subprocess.run([str(solver),*args],capture_output=True,text=True)
            if run.returncode:return r|dict(status='unsupported_solver',error=run.stderr)
            results.append(json.loads(run.stdout))
        before,after=results
        if after['structural_conflicts']>before['structural_conflicts'] or any(n>o for o,n in zip(before['missing_planes'],after['missing_planes'])):
            r['introduced_failure']=True
        old={(x['layer'],x['plane'],x['x'],x['y']):x for x in before['directional_geometry']}
        changes=[]
        for cell in after['directional_geometry']:
            key=cell['layer'],cell['plane'],cell['x'],cell['y'];prior=old[key]
            if cell['base']!=prior['base']:
                changes.append(dict(layer=key[0],plane=key[1],xy=list(key[2:]),before=prior['base'],after=cell['base'],role=cell['role']))
        return r|dict(status='audited',room=after['room_id'],changes=changes,
            before={k:before[k] for k in ('solved_wall_cells','unknown_wall_cells','structural_conflicts','missing_planes')},
            after={k:after[k] for k in ('solved_wall_cells','unknown_wall_cells','structural_conflicts','missing_planes')} |
                  {k:after.get(k,[0,0]) for k in ('persistent_support_required','persistent_support_missing')},
            persistent_reasons=dict(Counter('solved' if v['base']!=255 else 'ambiguous_body_owner' if v['component']==-2 else
                'unobserved_body' if v['component']==-3 else 'original_component_conflict' for v in after.get('persistent_support',[]))),
            snapshot_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),
            attributes_sha256=hashlib.sha256(attr.read_bytes()).hexdigest())
    with ThreadPoolExecutor(max_workers=4) as pool:
        records=list(pool.map(audit,sorted(a.corpus.glob('room-*-entrance-*.raw'))))
    report=dict(profile_sha256=hashlib.sha256(profile).hexdigest(),records=records,
        available_contexts=len(records),statuses=dict(Counter(r['status'] for r in records)),
        audited_rooms=len({r['room'] for r in records if r['status']=='audited'}),
        changed_contexts=sum(bool(r.get('changes')) for r in records),
        changed_plane_placements=sum(len(r.get('changes',[])) for r in records),approval=False,
        unsupported=['room017 unsettled','nondefault/dynamic states','complete movement graphs outside fixtures','matching live gameplay'])
    for side in ('before','after'):
        report[side]={key:sum(r.get(side,{}).get(key,0) for r in records) for key in ('solved_wall_cells','unknown_wall_cells','structural_conflicts')}
        report[side]['missing_planes']=[sum(r.get(side,{}).get('missing_planes',[0,0])[plane] for r in records) for plane in range(2)]
    for key in ('persistent_support_required','persistent_support_missing'):
        report['after'][key]=[sum(r.get('after',{}).get(key,[0,0])[plane] for r in records) for plane in range(2)]
    reasons=Counter()
    for r in records:reasons.update(r.get('persistent_reasons',{}))
    report['persistent_reasons']=dict(reasons)
    report['introduced_failure_contexts']=sum(r.get('introduced_failure',False) for r in records)
    (a.output/'corpus.json').write_text(json.dumps(report,indent=2)+'\n')
    print({k:v for k,v in report.items() if k!='records'})

if __name__=='__main__':main()
