"""Split appended core-only sessions and inventory world-space source power."""
import argparse, collections, hashlib, json, math
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('trace',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();blob=a.trace.read_bytes();sessions=[]
for line in blob.splitlines():
    r=json.loads(line)
    if r['sample']==0:sessions.append([])
    sessions[-1].append(r)
report=dict(trace_sha256=hashlib.sha256(blob).hexdigest(),sessions=[])
for session in sessions:
    records=[]
    for r in session:
        sources=[dict(world_xy=[l[0]+(r['scroll'][0]&511),l[1]+(r['scroll'][1]&511)],z=l[2],radius=l[3],intensity=l[4],area=4*math.pi*l[3]**2) for l in r['lights']]
        records.append(dict(sample=r['sample'],submodule=r['submodule'],phase=r['construction'],epoch=r['epoch'],map_key=r['map_key'],visible=r['visible_emitting_pixels'],sources=sources))
    report['sessions'].append(dict(frames=len(session),states=[dict(state=list(k),frames=v) for k,v in collections.Counter((r['submodule'],r['construction'],len(r['lights']),r['visible_emitting_pixels']) for r in session).items()],area_collapse_frames=sum(len(r['sources'])==20 and sum(v['area'] for v in r['sources'])<200 for r in records),records=records))
a.output.write_text(json.dumps(report,indent=2)+'\n')
print('sessions', [s['frames'] for s in report['sessions']], 'sha256',report['trace_sha256'])
