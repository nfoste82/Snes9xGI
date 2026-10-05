#!/usr/bin/env python3
"""Read-only exact sidecar/replay comparison, including placed authored seams.

Provenance is accepted only against its own complete snapshot; that snapshot's
BG2 words must also match the live solver wrapper before ownership is reported.
This diagnostic is not movement approval or validation v2.
"""
import argparse
import json
from pathlib import Path
import tomllib
from alttp_room_geometry import RoomSnapshot, RoomProvenance


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture', type=Path)
    p.add_argument('--replay', type=Path, required=True)
    p.add_argument('--snapshot', type=Path, required=True)
    p.add_argument('--provenance', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--rom', type=Path)
    p.add_argument('--atlas', type=Path)
    a = p.parse_args()
    state = json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    profile = tomllib.loads(Path(str(a.capture)+'.profile.toml').read_text())
    live = RoomSnapshot.read(a.replay/'live-solver-input.raw')
    reference = RoomSnapshot.read(a.snapshot)
    provenance = RoomProvenance.read(a.provenance, reference)
    if a.atlas:
        import sqlite3
        with sqlite3.connect(f'file:{a.atlas}?mode=ro', uri=True) as db:
            objects = [json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind IN ('dungeon_objects','dungeon_default_objects')")]
        for owner in (33,39,42,46,50):
            obj=provenance.objects[owner-1]
            print('handler',owner,[v for v in objects if v.get('room_id')==live.room and v.get('layer')==obj['phase'] and v['ordinal']==obj['ordinal'] and (v['raw_a']|v['raw_b']<<8)==obj['raw']])
    if any(u.word != v.word for u, v in zip(live.cells[1], reference.cells[1])):
        raise ValueError('complete BG2 maps differ; cannot transfer writer provenance')
    solution = json.loads((a.replay/'live-runtime.json').read_text())
    print('runtime counts', {k:solution[k] for k in ('solved_wall_cells','unknown_wall_cells','structural_conflicts','geometry_component_issues')})
    for r in solution['structural_placements']:
        if r['x'] in (44,59) and r['y']<8: print('structural',r)
    for r in solution['unresolved_wall_placements']:
        x,y=r['xy'];c=live.cell(1,x,y)
        owner=provenance.owner(1,x,y)
        print('unresolved',x,y,c.tile_hash,'writer',owner,'span',r['local_min'],r['local_max'], 'object',provenance.objects[owner-1] if owner in (42,46,50) else '')
    attributes = Path(str(a.capture)+'.dungeon-input.bin').read_bytes()[16392:24584]
    bases = [[255]*4096 for _ in range(2)]
    for plane, x, y, base in solution['cells_plane_x_y_base']:
        bases[plane][y*64+x] = base
    assets = {v['tile_hash']: v for v in profile['assets']}
    if a.rom:
        import struct
        rom = a.rom.read_bytes()
        for offset in (0x2e8,0x2f8,0x2c8,0x2d8):
            print('template',hex(offset),[hex(v) for v in struct.unpack_from('<8H',rom,0x1b52+offset)])
        for y in range(3,8):
            print('north actual',y,[hex(live.cell(1,x,y).word) for x in range(44,46)])
        print('waterfall template',[hex(v) for v in struct.unpack_from('<15H',rom,0x1b52+0x688)])
        for y in range(15,20):print('waterfall actual',y,[hex(live.cell(1,x,y).word) for x in (41,42,43,53,54)])
    groups = profile['asset_groups']
    records = []
    for c in live.cells[1]:
        i = c.y*64+c.x
        asset = assets.get(c.tile_hash, {})
        canonical = asset.get('height')
        heights = None if canonical is None else [canonical[(7-y if c.vflip else y)*8+(7-x if c.hflip else x)] for y in range(8) for x in range(8)]
        writer = provenance.owner(1,c.x,c.y)
        records.append(dict(x=c.x,y=c.y,tile_hash=c.tile_hash,writer=writer,
            attributes=[attributes[plane*4096+i] for plane in range(2)],
            groups=[k for k,v in groups.items() if c.tile_hash in v['tile_hashes']],
            captured_planes=[v[i] for v in state['plane_bases']],replayed_planes=[v[i] for v in bases],
            height_rows=None if heights is None else [heights[y*8:y*8+8] for y in range(8)],
            object=provenance.objects[writer-1] if 0<writer<=len(provenance.objects) else None))
    for r in records:
        if 41<=r['x']<=44 and 13<=r['y']<=18:
            print('rail-chain',r['x'],r['y'],r['tile_hash'],r['writer'],r['attributes'],r['captured_planes'],'->',r['replayed_planes'],r['groups'])
    mismatches = [r for r in records if r['captured_planes'] != r['replayed_planes']]
    a.output.write_text(json.dumps(dict(room=state['room'],capture_state={k:v for k,v in state.items() if not isinstance(v,list) or k in ('stairs','link_xy','scroll','required_planes','missing_planes')},
        plane_mismatch_cells=len(mismatches),mismatches=mismatches,placements=records,
        replay_counts={k:solution[k] for k in ('required_planes','missing_planes','solved_wall_cells','unknown_wall_cells','structural_conflicts','geometry_component_issues')}),indent=2)+'\n')
    print('captured/replay plane mismatch cells:',len(mismatches))
    for r in records:
        if (r['x'] in (41,44,59) and r['y']<10) or (r['x']==41 and 42<=r['y']<=46):
            print(r['x'],r['y'],r['tile_hash'],r['groups'],'writer',r['writer'],'attr',r['attributes'],r['captured_planes'],'->',r['replayed_planes'],'rows',None if r['height_rows'] is None else [row[0] for row in r['height_rows']])


if __name__ == '__main__':
    main()
