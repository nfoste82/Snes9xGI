#!/usr/bin/env python3
"""Pinned complete-room v2 gate; no invented stair traversal or authored arrays."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'remaster' / 'tools' / 'ALttP'))
from alttp_collision_reachability import doorway_graph, body_cells, read_attributes

p = argparse.ArgumentParser(description=__doc__)
for name in ('capture', 'output', 'rom'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
s = json.loads(Path(str(a.capture) + '.dungeon.json').read_text())
raw = a.output / 'live-solver-input.raw'
attr = Path(str(raw) + '.attr')
planes = read_attributes(attr.read_bytes(), s['room'])
# All complete room cells are input; reachable support scope is actual pinned
# seed only. No reference playback was supplied to attest directed stair edges.
graph, positions = doorway_graph(planes, (s['plane'], *(v & 511 for v in s['link_xy'])), [])
pin = dict(room_id=s['room'], link_xy=s['link_xy'], plane=s['plane'],
           collision_mode=s['collision_mode'], rom_sha256=s['rom_sha256'],
           source_capture=str(a.capture), transition_evidence='none; no invented stair edges')
g = dict(format='alttp-doorway-graph-review-v1', room_id=s['room'], state=pin,
         graph=graph, elevation={}, attribute_sha256=hashlib.sha256(attr.read_bytes()).hexdigest(),
         planes=[dict(plane=str(i), reachable_foot_positions=len(v),
                      reachable_body_cells=body_cells(v, planes[i])) for i, v in enumerate(positions)])
(a.output / 'live-state.json').write_text(json.dumps(pin, indent=2) + '\n')
(a.output / 'live-graph.json').write_text(json.dumps(g, indent=2) + '\n')
r = json.loads((a.output / 'live-runtime.json').read_text())
bases = [[255] * 4096 for _ in range(2)]
visible = [255] * 4096
for plane, x, y, base in r['cells_plane_x_y_base']:
    i = y * 64 + x
    bases[plane][i] = base
    visible[i] = base if visible[i] == 255 else max(visible[i], base)
for v in r['structural_placements']:
    visible[v['y'] * 64 + v['x']] = v['floor_base']
compiled = a.output / 'review-map.bin'
compiled.write_bytes(b'ALTPHM3\0' + struct.pack('<H', s['room']) +
                     struct.pack('<12288i', *(visible + bases[0] + bases[1])))
cmd = [sys.executable, '-B', str(ROOT / 'remaster/tools/ALttP/alttp_validate_room_heights.py'),
       '--graph', str(a.output / 'live-graph.json'), '--snapshot', str(raw),
       '--attributes', str(attr), '--state', str(a.output / 'live-state.json'),
       '--profile', str(a.output / 'replay-input-profile.toml'),
       '--runtime-solution', str(a.output / 'live-runtime.json'), '--height-map', str(compiled),
       '--rom', str(a.rom), '--output', str(a.output / 'validation-v2.json'),
       '--diagnostic-svg', str(a.output / 'validation-v2.svg')]
run = subprocess.run(cmd, capture_output=True, text=True)
pins = {suffix or 'packet': hashlib.sha256(Path(str(a.capture) + suffix).read_bytes()).hexdigest()
    for suffix in ('', '.profile.toml', '.dungeon.json', '.dungeon-input.bin',
                   '.dungeon-templates.bin', '.terrain-input.bin', '.sprite-input.bin')
    if Path(str(a.capture) + suffix).exists()}
report = dict(command=cmd, exit=run.returncode, stdout=run.stdout, stderr=run.stderr,
              pins=pins, approval=False, reachable_denominators=[len(v['reachable_body_cells']) for v in g['planes']],
              limits=['no attested directed stair playback',
                      'BG room validation does not approve sprite shape/ownership/support; audit these separately'])
(a.output / 'gate.json').write_text(json.dumps(report, indent=2) + '\n')
print('validator_exit', run.returncode, 'reachable_denominators', report['reachable_denominators'],
      'report', a.output / 'gate.json')
