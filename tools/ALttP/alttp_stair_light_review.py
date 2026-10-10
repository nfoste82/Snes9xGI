"""Exact reported stair packets: immutable pins and independent complete gates."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
from alttp_collision_reachability import doorway_graph, body_cells, read_attributes

p = argparse.ArgumentParser()
for key in ('captures', 'output', 'solver', 'rom', 'fixtures'):
    p.add_argument('--' + key, type=Path, required=True)
a = p.parse_args()
assert a.output.is_dir()
records = []
for room, seq in ((0x60, 47), (0x61, 50), (0x61, 51)):
    cap = a.captures / f'frame_room{room:03x}_{seq:03d}.s9xrmf'
    out = a.output / f'live{seq:03d}'
    out.mkdir(exist_ok=True)
    subprocess.run([sys.executable, '-B', 'tools/ALttP/alttp_replay_live_height_evidence.py',
                    str(cap), '--solver', str(a.solver), '--output', str(out), '--rom', str(a.rom)], check=True)
    state = json.loads(Path(str(cap)+'.dungeon.json').read_text())
    raw = out / 'live-solver-input.raw'
    attr = Path(str(raw)+'.attr')
    prior = json.loads((a.fixtures / f'room-{room:03x}-doorway-graph.json').read_text())
    exact_collision = prior['attribute_sha256'] == hashlib.sha256(attr.read_bytes()).hexdigest()
    # Only transfer observed stair edges with byte-exact collision planes.
    edges = prior['graph']['stair_edges'] if exact_collision else []
    planes = read_attributes(attr.read_bytes(), room)
    graph, positions = doorway_graph(planes, (state['plane'], *(v & 511 for v in state['link_xy'])), edges)
    pin = dict(room_id=room, link_xy=state['link_xy'], plane=state['plane'], collision_mode=state['collision_mode'],
               source_capture=str(cap), rom_sha256=state['rom_sha256'],
               transition_evidence='prior directed reference playback, exact collision' if exact_collision else
               'collision differs: prior edges withheld; special-transition scope unresolved')
    g = dict(format='alttp-doorway-graph-review-v1', room_id=room, state=pin, graph=graph, elevation={},
             attribute_sha256=hashlib.sha256(attr.read_bytes()).hexdigest(),
             planes=[dict(plane=str(i), reachable_foot_positions=len(v), reachable_body_cells=body_cells(v, planes[i])) for i,v in enumerate(positions)])
    (out/'live-state.json').write_text(json.dumps(pin, indent=2)+'\n')
    (out/'live-graph.json').write_text(json.dumps(g, indent=2)+'\n')
    solution = json.loads((out/'live-runtime.json').read_text())
    bases = [[255]*4096 for _ in range(2)]; visible = [255]*4096
    for plane,x,y,b in solution['cells_plane_x_y_base']:
        i=y*64+x; bases[plane][i]=b; visible[i]=b if visible[i]==255 else max(visible[i],b)
    for v in solution['structural_placements']:
        visible[v['y']*64+v['x']]=v['floor_base']
    compiled = out/'review-map.bin'
    compiled.write_bytes(b'ALTPHM3\0'+struct.pack('<H',room)+struct.pack('<12288i',*(visible+bases[0]+bases[1])))
    cmd=[sys.executable,'-B','tools/ALttP/alttp_validate_room_heights.py','--graph',str(out/'live-graph.json'),
         '--snapshot',str(raw),'--attributes',str(attr),'--state',str(out/'live-state.json'),
         '--profile',str(out/'replay-input-profile.toml'),'--runtime-solution',str(out/'live-runtime.json'),
         '--height-map',str(compiled),'--rom',str(a.rom),'--output',str(out/'validation-v2.json'),
         '--diagnostic-svg',str(out/'validation-v2.svg')]
    r=subprocess.run(cmd, capture_output=True, text=True)
    pins={s or 'packet':hashlib.sha256(Path(str(cap)+s).read_bytes()).hexdigest()
          for s in ('','.profile.toml','.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin','.terrain-input.bin')}
    record=dict(room=room, sequence=seq, pins=pins, state={k:v for k,v in state.items() if k not in ('plane_bases','visible_bases')},
                command=cmd, exit=r.returncode, stdout=r.stdout, stderr=r.stderr, exact_collision=exact_collision,
                doorway_denominators=[len(v['reachable_body_cells']) for v in g['planes']], approval=False)
    (out/'gate.json').write_text(json.dumps(record,indent=2)+'\n'); records.append(record)
    print('room',hex(room),'validator',r.returncode,'exact collision',exact_collision,r.stderr)
(a.output/'gates.json').write_text(json.dumps(records,indent=2)+'\n')
