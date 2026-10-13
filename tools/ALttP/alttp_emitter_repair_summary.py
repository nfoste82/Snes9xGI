"""Summarize existing pinned repair/gate artifacts, including room-wide gaps."""
import argparse
import json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('review',type=Path);a=p.parse_args()
for name in ('emitter007-gate-before','emitter007-gate-after','emitter007-fixtures-before','emitter007-fixtures'):
    path=a.review/name
    for f in sorted(path.glob('*runtime.json')):
        r=json.loads(f.read_text());print(name,f.name,{k:r.get(k) for k in ('required_planes','missing_planes','solved_wall_cells','unknown_wall_cells','structural_conflicts')})
        if r['room_id']==80:
            print('mounts',[{k:v.get(k) for k in ('x','y','floor_base','planes','local_source')} for v in r['structural_placements'] if v.get('local_source')=='placement_wall_mount_backing'])
    for f in sorted(path.glob('*validation-v2.json')):
        r=json.loads(f.read_text());print(name,'directional',{k:v for k,v in r.get('directional_wall_continuity',{}).items() if k!='edges'})
