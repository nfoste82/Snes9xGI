#!/usr/bin/env python3
"""Complete-room v2 on explicit live inputs, with matched playback graph evidence.

Fails rather than borrowing stair edges if complete attributes/stairs differ.
The live seed is not a height anchor. No viewport denominator is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from alttp_collision_reachability import doorway_review

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture', type=Path)
    p.add_argument('--solver', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--fixture-graph', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if not a.output.is_dir():
        p.error('output must exist')
    subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('alttp_replay_live_height_evidence.py')),
                    str(a.capture), '--solver', str(a.solver), '--rom', str(a.rom), '--output', str(a.output)], check=True)
    live = json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    fixture = json.loads(a.fixture_graph.read_text())
    raw = a.output/'live-solver-input.raw'
    attr = Path(str(raw)+'.attr')
    if hashlib.sha256(attr.read_bytes()).hexdigest() != fixture['attribute_sha256']:
        p.error('playback graph attributes differ from complete live inputs; refusing borrowed edges')
    stairs = [{'index': n, 'table': 0, 'tile_xy': s[:2], 'high_side': 'north' if s[2] else 'south'}
              for n, s in enumerate(live['stairs'])]
    if sorted((s['tile_xy'], s['high_side']) for s in stairs) != sorted(
            (s['tile_xy'], s['high_side']) for s in fixture['state']['stairs']):
        p.error('stair records mismatch')
    state = dict(format='alttp-room-traversal-state-v1', room_id=live['room'], entrance=None,
                 link_xy=[v & 511 for v in live['link_xy']], plane=live['plane'], collision_mode=live['collision_mode'],
                 stair_kind=fixture['state']['stair_kind'], default_wall_attribute=1, stairs=stairs)
    trace = [dict(plane=state['plane'], x=state['link_xy'][0], y=state['link_xy'][1], submodule=0, frame=0)]
    graph = doorway_review(live['room'], attr.read_bytes(), state, trace)
    # Reconstruct each already observed transition, not assumed landing geometry.
    # Attribute equality above is mandatory; graph is re-explored from live seed.
    extras = []
    for e in fixture['graph']['stair_edges']:
        q, x, y = e['from']; r, xx, yy = e['to']
        extras.append([dict(plane=q, x=x, y=y, submodule=0, frame=0),
                       dict(plane=q, x=x, y=y, submodule=8, frame=1),
                       dict(plane=r, x=xx, y=yy, submodule=0, frame=e['frames'])])
    graph = doorway_review(live['room'], attr.read_bytes(), state, trace, extras)
    graph['matched_playback_evidence'] = dict(path=str(a.fixture_graph), sha256=hashlib.sha256(a.fixture_graph.read_bytes()).hexdigest(),
                                            complete_attribute_equality=True, limitation='No fresh Snes9x movement playback.')
    graph_path = a.output/'live-doorway-graph.json'; graph_path.write_text(json.dumps(graph, indent=2)+'\n')
    state_path = a.output/'live-traversal-state.json'; state_path.write_text(json.dumps(state, indent=2)+'\n')
    profile = a.output/'replay-input-profile.toml'
    cmd = [str(a.solver), str(raw), str(attr), str(profile), *map(str, state['link_xy']), str(state['plane'])]
    for s in live['stairs']:
        cmd += list(map(str, s))
    cmd += ['--rom', str(a.rom)]
    solution_path = a.output/'live-local-seed-runtime.json'
    with solution_path.open('w') as out:
        subprocess.run(cmd, stdout=out, check=True)
    solution = json.loads(solution_path.read_text())
    planes = [[255]*4096 for _ in range(2)]; visible = [255]*4096
    for plane, x, y, base in solution['cells_plane_x_y_base']:
        i = y*64+x; planes[plane][i] = base; visible[i] = base if visible[i]==255 else max(base,visible[i])
    for s in solution['structural_placements']:
        visible[s['y']*64+s['x']] = s['floor_base']
    compiled = a.output/'live-runtime.bin'
    compiled.write_bytes(b'ALTPHM3\0'+struct.pack('<H',live['room'])+struct.pack('<12288i',*(visible+planes[0]+planes[1])))
    result = subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('alttp_validate_room_heights.py')),
        '--graph', str(graph_path), '--snapshot', str(raw), '--attributes', str(attr), '--state', str(state_path),
        '--profile', str(profile), '--runtime-solution', str(solution_path), '--height-map', str(compiled),
        '--rom', str(a.rom), '--output', str(a.output/'live-validation-v2.json'),
        '--diagnostic-svg', str(a.output/'live-validation-v2.svg')])
    print('complete live v2 exit', result.returncode)
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
