"""Summarize immutable paired room/capture artifacts without changing profiles."""
import argparse
import hashlib
import json
from collections import defaultdict
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('directory',type=Path)
p.add_argument('--capture',type=Path,required=True)
p.add_argument('--active-profile',type=Path,required=True)
a=p.parse_args()
report={'profile_bytes_equal':Path(str(a.capture)+'.profile.toml').read_bytes()==a.active_profile.read_bytes(), 'active_profile_sha256':hashlib.sha256(a.active_profile.read_bytes()).hexdigest(), 'approval':False}
for side in ('before','after'):
    cells=[json.loads(v) for v in (a.directory/f'corner079-{side}-placements.jsonl').read_text().splitlines()]
    targets=[{k:v for k,v in c.items() if k!='height'} | {'local_range':[min(c['height']),max(c['height'])]} for c in cells if c['hash'] in ('a9b6cfd6b17c9385','ccf87d6be13c02a4')]
    groups=defaultdict(list)
    for line in (a.directory/f'corner079-{side}-effective.jsonl').read_text().splitlines():
        c=json.loads(line)
        if c['hash'] in ('a9b6cfd6b17c9385','ccf87d6be13c02a4'):
            groups[(c['recomputed'],c['bg'],c['x'],c['y'],c['hash'])].append(c['effective'])
    pixels=[{'recomputed':k[0],'bg':k[1],'x':k[2],'y':k[3],'hash':k[4],'pixels':len(v),'effective_range':[min(v),max(v)]} for k,v in groups.items()]
    gate=a.directory/('corner079-gate-prior' if side=='before' else 'corner079-gate-after')
    validation=json.loads((gate/'validation-v2.json').read_text())
    counts={k:validation[k] for k in ('required_walkable_placements','valid_walkable_placements','solved_wall_cells','unknown_wall_cells','conflicting_components','per_plane_walkable_denominator')}
    counts['directional_wall_continuity']={k:validation['directional_wall_continuity'][k] for k in ('conflicts','edge_denominator')}
    report[side]={'targets':targets,'visible_pixels':pixels,'validation':counts,'packet':json.loads((a.directory/f'corner079-{side}.json').read_text())}
(a.directory/'corner079-summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'profile_bytes_equal':report['profile_bytes_equal'], 'active_profile_sha256':report['active_profile_sha256'], **{s:{'visible_pixels':report[s]['visible_pixels'],'validation':report[s]['validation']} for s in ('before','after')}},indent=2))
