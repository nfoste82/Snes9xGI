"""Complete-room paired solver audit; never equates solved counts with approval."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
for name in ('corpus', 'profile', 'rom', 'before', 'after', 'output'):
    p.add_argument('--'+name, type=Path, required=True)
a = p.parse_args()
records = []
for raw in sorted(a.corpus.glob('room-*-entrance-*.raw')):
    state = json.loads(Path(str(raw)+'.state.json').read_text())
    args = [str(raw), str(raw)+'.attr', str(a.profile), *map(str,state['link_xy']), str(state['plane']), '--rom', str(a.rom)]
    pair = []
    for solver in (a.before,a.after):
        r = subprocess.run([str(solver),*args], capture_output=True, text=True)
        if r.returncode:
            pair.append({'unsupported':r.stderr.strip()})
            continue
        data = json.loads(r.stdout)
        pair.append({k:data[k] for k in ('solved_wall_cells','unknown_wall_cells','structural_conflicts')})
    records.append({'snapshot':str(raw),'room':state.get('room_id',int(raw.name.split('-')[1],16)), 'before':pair[0], 'after':pair[1]})
report = {'contexts':len(records),'rooms':len({r['room'] for r in records}), 'profile_sha256':hashlib.sha256(a.profile.read_bytes()).hexdigest(), 'rom_sha256':hashlib.sha256(a.rom.read_bytes()).hexdigest(), 'approval':False, 'unmodeled_states':'switch/chest/water/door variants not enumerated', 'records':records}
for side in ('before','after'):
    report[side] = {k:sum(r[side].get(k,0) for r in records) for k in ('solved_wall_cells','unknown_wall_cells','structural_conflicts')}
    report[side]['unsupported_contexts']=sum('unsupported' in r[side] for r in records)
report['changed_contexts']=sum(r['before']!=r['after'] for r in records)
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='records'},indent=2))
