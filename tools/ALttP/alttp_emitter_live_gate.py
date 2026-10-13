#!/usr/bin/env python3
"""Complete pinned live height gate from observed seed; no borrowed stair edges."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
from alttp_collision_reachability import doorway_review


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture', type=Path)
    p.add_argument('--solver', required=True, type=Path)
    p.add_argument('--rom', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--research-profile', type=Path, help='explicit selected metadata on immutable capture inputs; not capture-pinned approval')
    a = p.parse_args()
    if not a.output.is_dir():
        p.error('output must exist')
    live = json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    if live['stairs']:
        p.error('requires observed transition evidence for rooms with stairs')
    tools = Path(__file__).parent
    replay=[sys.executable, '-B', str(tools/'alttp_replay_live_height_evidence.py'),
                    str(a.capture), '--solver', str(a.solver), '--rom', str(a.rom),
                    '--output', str(a.output)]
    if a.research_profile:replay+=['--research-profile',str(a.research_profile)]
    subprocess.run(replay, check=True)
    raw = a.output/'live-solver-input.raw'
    attr = Path(str(raw)+'.attr')
    state = dict(format='alttp-room-traversal-state-v1', room_id=live['room'], entrance=None,
                 link_xy=[v & 511 for v in live['link_xy']], plane=live['plane'],
                 collision_mode=live['collision_mode'], default_wall_attribute=1, stairs=[],
                 source_capture=str(a.capture), rom_sha256=live['rom_sha256'],
                 limitation='Observed seed, static whole-room cardinal scope; no fresh movement playback.')
    trace = [dict(plane=state['plane'], x=state['link_xy'][0], y=state['link_xy'][1], submodule=0, frame=0)]
    graph = doorway_review(live['room'], attr.read_bytes(), state, trace)
    graph_path = a.output/'live-graph.json'
    state_path = a.output/'live-state.json'
    graph_path.write_text(json.dumps(graph, indent=2)+'\n')
    state_path.write_text(json.dumps(state, indent=2)+'\n')
    runtime = a.output/'live-runtime.json'
    # Runtime masks world XY to room-local; serialize the same local seed as
    # the pinned traversal state, rather than its equivalent world coordinates.
    with runtime.open('w') as stream:
        subprocess.run([str(a.solver), str(raw), str(attr), str(a.output/'replay-input-profile.toml'),
                        *map(str, state['link_xy']), str(state['plane']), '--rom', str(a.rom)],
                       stdout=stream, check=True)
    solution = json.loads(runtime.read_text())
    planes = [[255]*4096 for _ in range(2)]
    visible = [255]*4096
    for plane, x, y, base in solution['cells_plane_x_y_base']:
        i = y*64+x
        planes[plane][i] = base
        visible[i] = base if visible[i] == 255 else max(visible[i], base)
    for s in solution['structural_placements']:
        visible[s['y']*64+s['x']] = s['floor_base']
    # The solver's placed inventory also includes immediate border fallbacks,
    # which are not authored walls/obstacles or walkable support cells.
    for s in solution.get('directional_geometry', []):
        if s['layer'] == 1 and s['fallback_directions'] and s['base'] != 255:
            visible[s['y']*64+s['x']] = s['base']
            planes[s['plane']][s['y']*64+s['x']] = s['base']
    compiled = a.output/'live-runtime.bin'
    compiled.write_bytes(b'ALTPHM3\0'+struct.pack('<H', live['room'])+
                         struct.pack('<12288i', *(visible+planes[0]+planes[1])))
    command = [sys.executable, '-B', str(tools/'alttp_validate_room_heights.py'),
               '--graph', str(graph_path), '--snapshot', str(raw), '--attributes', str(attr),
               '--state', str(state_path), '--profile', str(a.output/'replay-input-profile.toml'),
               '--runtime-solution', str(runtime), '--height-map', str(compiled), '--rom', str(a.rom),
               '--output', str(a.output/'validation-v2.json'),
               '--diagnostic-svg', str(a.output/'validation-v2.svg')]
    result = subprocess.run(command, capture_output=True, text=True)
    record = dict(command=command, exit=result.returncode, stdout=result.stdout, stderr=result.stderr,
                  graph_denominators=[len(v['reachable_body_cells']) for v in graph['planes']], approval=False)
    (a.output/'gate.json').write_text(json.dumps(record, indent=2)+'\n')
    report_path = a.output/'validation-v2.json'
    if report_path.exists():
        report = json.loads(report_path.read_text())
        record['summary'] = {k: report.get(k) for k in
                             ('required_walkable_placements', 'valid_walkable_placements',
                              'required_fallback_placements', 'valid_fallback_placements',
                              'solved_wall_cells', 'unknown_wall_cells', 'success')}
        for key in ('missing_placements', 'conflicting_components', 'compiled_map_missing', 'compiled_map_mismatches'):
            record['summary'][key] = len(report.get(key, []))
        (a.output/'gate-summary.json').write_text(json.dumps(record['summary'], indent=2)+'\n')
    print(json.dumps({k: v for k, v in record.items() if k not in ('command', 'stdout')}, indent=2))
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
