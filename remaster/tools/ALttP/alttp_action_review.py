"""Pinned action regression artifacts and mandatory complete-room gates."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
from PIL import Image
from alttp_collision_reachability import doorway_graph, body_cells, read_attributes

p = argparse.ArgumentParser()
p.add_argument('--captures', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--rom', type=Path, required=True)
p.add_argument('--graph061', type=Path, required=True)
p.add_argument('--graph062', type=Path, required=True)
a = p.parse_args()
records = []
for room, sequence, graph_path in ((0x61, 48, a.graph061), (0x62, 19, a.graph062)):
    capture = a.captures / f'frame_room{room:03x}_{sequence:03d}.s9xrmf'
    out = a.output / f'live{sequence:03d}'
    state = json.loads(Path(str(capture)+'.dungeon.json').read_text())
    raw = out/'live-solver-input.raw'
    attr = Path(str(raw)+'.attr')
    prior = json.loads(graph_path.read_text())
    planes = read_attributes(attr.read_bytes(), room)
    edges = prior['graph']['stair_edges']
    # Existing directed playback remains admissible only if changes are exactly
    # indexed liftable -> ordinary floor; no wall/stair/door change is tolerated.
    if room == 0x62:
        old = Path(str(a.captures/'frame_room062_016.s9xrmf')+'.dungeon-input.bin').read_bytes()[16392:24584]
        current = attr.read_bytes()[10:]
        changes = [(i, x, y) for i, (x, y) in enumerate(zip(old, current)) if x != y]
        assert changes and all(0x70 <= x <= 0x7f and y == 0 for _, x, y in changes), changes
    else:
        assert prior['attribute_sha256'] == hashlib.sha256(attr.read_bytes()).hexdigest()
        changes = []
    graph, positions = doorway_graph(planes, (state['plane'], *(v & 511 for v in state['link_xy'])), edges)
    pin = dict(room_id=room, link_xy=state['link_xy'], plane=state['plane'], collision_mode=state['collision_mode'],
               source_capture=str(capture), rom_sha256=state['rom_sha256'],
               transition_evidence='prior directed reference playback; exact collision or indexed-removal-only delta; fresh gameplay parity unproved')
    g = dict(format='alttp-doorway-graph-review-v1', room_id=room, state=pin, graph=graph, elevation={},
             attribute_sha256=hashlib.sha256(attr.read_bytes()).hexdigest(),
             planes=[dict(plane=str(i), reachable_foot_positions=len(v), reachable_body_cells=body_cells(v, planes[i])) for i,v in enumerate(positions)])
    (out/'live-state.json').write_text(json.dumps(pin, indent=2)+'\n')
    (out/'live-graph.json').write_text(json.dumps(g, indent=2)+'\n')
    solution = json.loads((out/'live-runtime.json').read_text())
    bases = [[255]*4096 for _ in range(2)]
    visible = [255]*4096
    for plane,x,y,b in solution['cells_plane_x_y_base']:
        i=y*64+x; bases[plane][i]=b; visible[i]=b if visible[i]==255 else max(visible[i],b)
    for v in solution['structural_placements']:
        visible[v['y']*64+v['x']]=v['floor_base']
    compiled = out/'review-map.bin'
    compiled.write_bytes(b'ALTPHM3\0'+struct.pack('<H',room)+struct.pack('<12288i',*(visible+bases[0]+bases[1])))
    cmd=[sys.executable,'-B','remaster/tools/ALttP/alttp_validate_room_heights.py','--graph',str(out/'live-graph.json'),
         '--snapshot',str(raw),'--attributes',str(attr),'--state',str(out/'live-state.json'),
         '--profile',str(out/'replay-input-profile.toml'),'--runtime-solution',str(out/'live-runtime.json'),
         '--height-map',str(compiled),'--rom',str(a.rom),'--output',str(out/'validation-v2.json'),
         '--diagnostic-svg',str(out/'validation-v2.svg')]
    r=subprocess.run(cmd, capture_output=True, text=True)
    pins={s or 'packet':hashlib.sha256(Path(str(capture)+s).read_bytes()).hexdigest()
          for s in ('','.profile.toml','.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin','.terrain-input.bin')}
    record=dict(room=room, sequence=sequence, pins=pins, state={k:v for k,v in state.items() if k not in ('plane_bases','visible_bases')},
                command=cmd, exit=r.returncode, stdout=r.stdout, stderr=r.stderr, collision_changes=changes,
                doorway_denominators=[len(v['reachable_body_cells']) for v in g['planes']], approval=False)
    (out/'gate.json').write_text(json.dumps(record,indent=2)+'\n'); records.append(record)
    print('room',hex(room),'validator',r.returncode,r.stderr)
for image in a.output.glob('local048*.ppm'):
    Image.open(image).save(image.with_suffix('.png'))
(a.output/'gates.json').write_text(json.dumps(records,indent=2)+'\n')
