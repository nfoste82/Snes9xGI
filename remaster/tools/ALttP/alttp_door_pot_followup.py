import json
import sys
import subprocess
import os
from pathlib import Path
p=Path(sys.argv[1])
c=json.loads((p/'offline-corpus/corpus.json').read_text())
for r in c['records']:
    if r.get('status')=='audited' and r.get('room') in (0xc0,0xc2,0xcb,0xcc,0xdc):
        print('regression',r['snapshot'],r['before'],r['after'])
        raw=Path(r['snapshot']);state=json.loads(Path(str(raw)+'.state.json').read_text())
        args=[str(raw),str(raw)+'.attr',str(p/'replay-input-profile.toml'),*map(str,state['link_xy']),str(state['plane']),'--rom','build/remaster-geometry/room-preview-engine-v21/zelda3.sfc']
        for name,solver,env in [('before','/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room062-live-followup/solver',{}),('after',str(p/'solver-after'),{}),('no-pots',str(p/'solver-after'),{'ALTTP_AUDIT_NO_PERSISTENT_POTS':'1'})]:
            v=json.loads(subprocess.check_output([solver,*args],env=os.environ|env))
            (p/(raw.stem+'-'+name+'.json')).write_text(json.dumps(v)+'\n')
            print(name,{k:v[k] for k in ('required_planes','missing_planes','structural_conflicts','geometry_component_issues')})
        before=json.loads((p/(raw.stem+'-before.json')).read_text());after=json.loads((p/(raw.stem+'-after.json')).read_text())
        for key in ('required_planes','missing_planes','structural_conflicts','geometry_component_issues','constraint_issues'):
            assert before[key]==after[key], (raw,key)
        if r['room']==0xc0:
            issues=before['constraint_issues']
            assert issues==after['constraint_issues'] and len(issues)==64
            print('0c0 preserved signed constraint residuals',sorted({v[2]-v[3] for v in issues}))
r=json.loads((p/'after016.json').read_text())['layers'][1]
for y in range(59,60):
    for x in range(12,20):
        i=y*64+x;t=r['cells'][i];s=r['local_samples'][i]
        print('south',t,'template',s['template'],'height edges',s['height'][:8],s['height'][-8:])
