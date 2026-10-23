#!/usr/bin/env python3
"""Temporary example authoring/replay audit; never mutates capture/profile inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tomllib
from alttp_authored_obstacles import audit
from alttp_room_geometry import RoomSnapshot
from alttp_collision_reachability import read_attributes


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--inputs', type=Path, required=True, help='existing own-input replay directory')
    p.add_argument('--layer', type=int, choices=(0, 1), required=True)
    p.add_argument('--x', type=int, required=True)
    p.add_argument('--y', type=int, required=True)
    p.add_argument('--width', type=int, required=True)
    p.add_argument('--height', type=int, required=True)
    p.add_argument('--missing-local-hash', action='append', default=[], help='temporary mixed-group fallback ablation only')
    a = p.parse_args()
    if not a.output.is_dir() or a.output.resolve() == a.inputs.resolve(): p.error('distinct existing output directory required')
    raw = a.inputs/'live-solver-input.raw'; attr = Path(str(raw)+'.attr')
    source_profile = a.inputs/'replay-input-profile.toml'
    baseline = json.loads((a.inputs/'live-runtime.json').read_text())
    inputs = [raw, attr, source_profile]
    before = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}
    snap = RoomSnapshot.read(raw)
    members = []
    for y in range(a.height):
        for x in range(a.width):
            c = snap.cell(a.layer, a.x+x, a.y+y)
            members.append(dict(tile_hash=c.tile_hash, x=x, y=y, z=1 if y == 0 else -1 if y == 1 else 0,
                                hflip=c.hflip, vflip=c.vflip, support_contact=y == a.height-1))
    text = source_profile.read_text()
    import re
    text = re.sub(r'^schema_version = \d+', 'schema_version = 22', text, count=1)
    text += '\n[[obstacle_groups]]\nname = "temporary_example"\n'
    for m in members:
        text += '\n[[obstacle_groups.members]]\n'
        for key, value in m.items():
            text += f'{key} = ' + (json.dumps(value) if type(value) is not bool else str(value).lower()) + '\n'
    if a.missing_local_hash:
        def strip(match):
            block=match.group();key=re.search(r'(?m)^tile_hash\s*=\s*"([^"]+)"',block)
            if key and key[1] in a.missing_local_hash:
                block=re.sub(r'(?ms)^height\s*=\s*\[.*?\]\s*\n','',block)
                block=re.sub(r'(?m)^height_sampling\s*=.*\n','',block)
            return block
        text=re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',strip,text)
    profile = a.output/'temporary-example.toml'; profile.write_text(text)
    parsed = tomllib.loads(text)
    command = [str(a.solver), str(raw), str(attr), str(profile), *map(str, baseline['link_xy']), str(baseline['plane'])]
    for s in baseline['stairs']: command += list(map(str, s))
    command += ['--rom', str(a.rom)]
    with (a.output/'runtime.json').open('w') as out: subprocess.run(command, stdout=out, check=True)
    solution = json.loads((a.output/'runtime.json').read_text())
    proof = audit(snap, read_attributes(attr.read_bytes(), snap.room), parsed, solution)
    (a.output/'independent-proof.json').write_text(json.dumps(proof, indent=2)+'\n')
    visible = [255]*4096; planes = [[255]*4096 for _ in range(2)]
    for q, x, y, base in solution['cells_plane_x_y_base']:
        i = y*64+x; planes[q][i] = base; visible[i] = max(visible[i], base) if visible[i] != 255 else base
    for r in solution['structural_placements']: visible[r['y']*64+r['x']] = r['floor_base']
    (a.output/'runtime.bin').write_bytes(b'ALTPHM3\0'+struct.pack('<H', snap.room)+struct.pack('<12288i', *(visible+planes[0]+planes[1])))
    summary = dict(approval=False, room=snap.room, selected_example=dict(layer=a.layer, x=a.x, y=a.y, width=a.width, height=a.height),
                   source_hashes=before, inputs_immutable=all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == h for f, h in before.items()),
                   new_group_proof={k: proof[k] for k in ('success', 'required_cells', 'solved_cells', 'missing_cells', 'conflicting_cells', 'per_layer')},
                   mismatches=len(proof['mismatches']), support_required=solution['required_planes'], support_missing=solution['missing_planes'],
                   structural_conflicts=solution['structural_conflicts'], unknown_walls=solution['unknown_wall_cells'],
                   temporary_profile_sha256=hashlib.sha256(profile.read_bytes()).hexdigest())
    (a.output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n'); print(summary)
    return int(not proof['success'] or not summary['inputs_immutable'])


if __name__ == '__main__': sys.exit(main())
