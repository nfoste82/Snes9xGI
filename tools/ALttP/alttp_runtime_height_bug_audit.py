"""Paired own-profile packet and complete-room corpus audit. Never approval."""
import argparse
import collections
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
for name in ('captures', 'corpus', 'profile', 'rom', 'before', 'after', 'before-probe', 'after-probe', 'output'):
    p.add_argument('--'+name, type=Path, required=True)
a = p.parse_args()
assert a.output.is_dir()
assert hashlib.sha256(a.rom.read_bytes()).hexdigest() == '66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'
profile = a.output/'corpus-profile.toml'
profile.write_bytes(a.profile.read_bytes())

def packet(capture):
    missing = [s for s in ('.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin', '.profile.toml') if not Path(str(capture)+s).exists()]
    row = dict(capture=str(capture), missing=missing)
    if missing:
        return row | dict(status='unsupported_missing_evidence')
    row['pins'] = {s:hashlib.sha256(Path(str(capture)+s).read_bytes()).hexdigest() for s in ('', '.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin', '.profile.toml')}
    state = json.loads(Path(str(capture)+'.dungeon.json').read_text())
    row['state'] = {k:v for k,v in state.items() if k not in ('plane_bases','visible_bases')}
    for side, probe in (('before',a.before_probe),('after',a.after_probe)):
        prefix = a.output/(capture.stem+'-'+side)
        run = subprocess.run([str(probe),str(capture),str(prefix)],capture_output=True,text=True)
        row[side] = dict(error=run.stderr.strip()) if run.returncode else json.loads(prefix.with_suffix('.json').read_text())
    row['status'] = 'refused' if any('error' in row[s] for s in ('before','after')) else 'replayed'
    if row['status']=='replayed':
        row['changed'] = any({k:v for k,v in old.items() if k in new}!={k:v for k,v in new.items() if k in old}
                             for old,new in zip(row['before']['layers'],row['after']['layers']))
        row['effective_packet_changed']=hashlib.sha256((a.output/(capture.stem+'-before.s9xrmf')).read_bytes()).digest()!=hashlib.sha256((a.output/(capture.stem+'-after.s9xrmf')).read_bytes()).digest()
    return row

def linked(raw):
    state = json.loads(Path(str(raw)+'.state.json').read_text())
    command = [str(raw),str(raw)+'.attr',str(profile),*map(str,state['link_xy']),str(state['plane']),'--rom',str(a.rom)]
    row = dict(snapshot=str(raw),room=int(raw.name.split('-')[1],16))
    for side,solver in (('before',a.before),('after',a.after)):
        run = subprocess.run([str(solver),*command],capture_output=True,text=True)
        if run.returncode:
            row[side] = dict(error=run.stderr.strip())
        else:
            d = json.loads(run.stdout)
            row[side] = {k:d[k] for k in ('solved_wall_cells','unknown_wall_cells','structural_conflicts','required_planes','missing_planes','solved_authored_obstacle_cells','unmatched_authored_obstacle_cells')}
            row[side]['effective_digest'] = hashlib.sha256(json.dumps({k:d[k] for k in ('cells_plane_x_y_base','structural_placements','directional_geometry')},sort_keys=True).encode()).hexdigest()
            row[side]['support_digest'] = hashlib.sha256(json.dumps(d['cells_plane_x_y_base']).encode()).hexdigest()
            row[side]['placed_plane_denominator'] = len(d['directional_geometry'])
            row[side]['placed_reasons'] = dict(collections.Counter(
                'missing_local' if not c['has_local'] else 'unknown_base' if c['base']==255 else 'base_and_local_not_approval'
                for c in d['directional_geometry']))
    return row

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    packets = list(pool.map(packet,sorted(a.captures.glob('frame_room*.s9xrmf'))))
    rooms = list(pool.map(linked,sorted(a.corpus.glob('room-*-entrance-*.raw'))))
report = dict(approval=False,packets=packets,linked=rooms,profile_sha256=hashlib.sha256(profile.read_bytes()).hexdigest(),
    unsupported_scope='Unenumerated dynamic room states; offline room017 missing; borrowed graphics contexts are not gameplay parity. Subtile handler ablations are not observed traversal.',
    packet_status=dict(collections.Counter(r['status'] for r in packets)),linked_contexts=len(rooms),linked_rooms=len({r['room'] for r in rooms}),
    changed_packets=sum(r.get('changed',False) for r in packets),
    changed_linked=sum(r['before']!=r['after'] for r in rooms))
(a.output/'corpus.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('packets','linked')},indent=2))
