#!/usr/bin/env python3
"""Audit every available complete live packet; keep unsupported denominators explicit."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--captures',type=Path,required=True)
p.add_argument('--probe',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline-solver',type=Path)
p.add_argument('--rom',type=Path)
p.add_argument('--fixtures',type=Path)
p.add_argument('--solver',type=Path)
a=p.parse_args()
if not a.output.is_dir():
    p.error('output directory must exist')
records=[]
for capture in sorted(a.captures.glob('*.s9xrmf')):
    missing=[s for s in ('.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin','.profile.toml') if not Path(str(capture)+s).exists()]
    if missing:
        records.append(dict(capture=capture.name,status='unsupported_missing_complete_evidence',missing=missing))
        continue
    prefix=a.output/capture.stem
    r=subprocess.run([str(a.probe),str(capture),str(prefix)],capture_output=True,text=True)
    if r.returncode:
        records.append(dict(capture=capture.name,status='unsupported_replay',error=r.stderr))
        continue
    report=json.loads(prefix.with_suffix('.json').read_text())
    layers=[]
    for layer in report['layers']:
        reasons=Counter()
        for c in layer['cells']:
            if not c['has_local']:
                reasons['missing_local_metadata']+=1
            elif c['role']==2:
                reasons['authored_wall_solved' if c['base']!=255 else 'authored_wall_unknown']+=1
            elif c['fallback']:
                reasons['placement_fallback']+=1
            elif c['base']==255:
                reasons['unresolved_or_unobserved_nonwall']+=1
            elif c['role']==0:
                reasons['support_under_unclassified_art']+=1
            else:
                reasons['classified_nonwall_with_base']+=1
        layers.append({k:v for k,v in layer.items() if k!='cells'}|dict(placement_denominator=4096,reasons=dict(reasons)))
    record=dict(capture=capture.name,status='replayed',room=report['room'],profile_pin=report['profile_pin'],layers=layers)
    if a.baseline_solver:
        if not a.rom:
            p.error('--baseline-solver requires --rom')
        baseline=a.output/(capture.stem+'-baseline')
        baseline.mkdir(exist_ok=True)
        subprocess.run([sys.executable,'-B','tools/ALttP/alttp_replay_live_height_evidence.py',str(capture),
                        '--solver',str(a.baseline_solver),'--rom',str(a.rom),'--output',str(baseline)],check=True)
        prior=json.loads((baseline/'live-runtime.json').read_text())
        differences=Counter()
        changed=[]
        for old in prior['directional_geometry']:
            layer=next(l for l in report['layers'] if l['bg']==old['layer'])
            cell=layer['cells'][old['y']*64+old['x']]
            new=cell['structural'][old['plane']]
            if new==255:
                new=cell['support'][old['plane']]
            if new==255 and cell['fallback']:
                # Adapter only supplies fallback bases on solid placements.
                inp=Path(str(capture)+'.dungeon-input.bin').read_bytes()
                attr=inp[16392+old['plane']*4096+old['y']*64+old['x']]
                if attr in (1,2):
                    new=cell['base']
            if new!=old['base']:
                reason='restored_base' if old['base']==255 else 'removed_unowned_base' if new==255 else 'changed_base'
                differences[f"BG{old['layer']+1}/plane{old['plane']}/{reason}"]+=1
                changed.append(dict(layer=old['layer'],plane=old['plane'],xy=[old['x'],old['y']],role=old['role'],before=old['base'],after=new))
        record['runtime_base_changes']=dict(differences)
        (a.output/(capture.stem+'-base-changes.json')).write_text(json.dumps(changed,indent=2)+'\n')
    records.append(record)
linked=[]
if a.fixtures:
    if not a.solver or not a.rom:
        p.error('--fixtures requires --solver and --rom')
    # Use one explicitly pinned metadata context, not each reference capture's
    # historical candidate. These results are corpus rule checks, not approval.
    profile=a.captures/'frame_room060_024.s9xrmf.profile.toml'
    for state_path in sorted(a.fixtures.glob('*.raw.state.json')):
        raw=Path(str(state_path).removesuffix('.state.json'))
        attr=Path(str(raw)+'.attr')
        if not raw.exists() or not attr.exists():
            linked.append(dict(input=raw.name,status='missing_complete_inputs'))
            continue
        state=json.loads(state_path.read_text())
        if any('tile_xy' not in s or 'high_side' not in s for s in state.get('stairs',[])):
            linked.append(dict(input=raw.name,status='unsupported_missing_directed_stair_evidence'))
            continue
        command=[str(a.solver),str(raw),str(attr),str(profile),*map(str,state['link_xy']),str(state['plane'])]
        for stair in state['stairs']:
            command += [*map(str,stair['tile_xy']),str(int(stair['high_side']=='north')),str(int(state['stair_kind']==1))]
        command += ['--rom',str(a.rom)]
        artifact=a.output/(raw.stem+'-corpus-runtime.json')
        with artifact.open('w') as f:
            result=subprocess.run(command,stdout=f,stderr=subprocess.PIPE,text=True)
        if result.returncode:
            linked.append(dict(input=raw.name,status='unsupported',error=result.stderr))
            continue
        solved=json.loads(artifact.read_text())
        reasons=Counter(f"BG{c['layer']+1}/plane{c['plane']}/"+('missing_local' if not c['has_local'] else 'unknown_base' if c['base']==255 else 'resolved_base_not_geometry_approval') for c in solved['directional_geometry'])
        linked.append(dict(input=raw.name,status='replayed',room=solved['room_id'],placed_plane_denominator=16384,reasons=dict(reasons)))
summary=dict(packet_denominator=len(records),status_counts=dict(Counter(r['status'] for r in records)),records=records,
    linked_snapshot_denominator=len(linked),linked_snapshots=linked,
    scope='All local gameplay packets with complete live inputs, own captured profiles and template evidence. No unseen room/state coverage claimed. Unclassified/transparent art is not counted as solved geometry.')
(a.output/'corpus-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='records'},indent=2))
