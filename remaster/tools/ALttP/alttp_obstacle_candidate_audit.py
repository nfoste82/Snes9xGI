#!/usr/bin/env python3
"""Read-only comparison of candidate local metadata and complete gate outputs."""
import hashlib
import json
import sys
import tomllib
from pathlib import Path

source, candidate, pinned, output = map(Path, sys.argv[1:])
profiles = [tomllib.loads(p.read_text()) for p in (source, candidate, pinned)]
assets = [{a['tile_hash']: a for a in p['assets']} for p in profiles]
for tile in ('v1:4bpp:c40518b112d0814e', 'v1:4bpp:40ea00e78742379c', 'v1:4bpp:2ff1e249201dfe5b'):
    for label, p in zip(('root', 'candidate', '011'), profiles):
        print('groups', tile, label, [g for g, v in p.get('asset_groups', {}).items() if tile in v['tile_hashes']])
print('candidate_sha256', hashlib.sha256(candidate.read_bytes()).hexdigest())
print('schema', profiles[0]['schema_version'], profiles[1]['schema_version'])
print('layer_changes_vs_root', {layer: sum(a.get(layer) != assets[0].get(tile, {}).get(layer)
                                          for tile, a in assets[1].items())
                                  for layer in ('height', 'normal_xyz', 'occlusion', 'emission_rgba')})
print('settings_preserved', all(profiles[1].get(k) == v for k, v in profiles[0].items()
                                if k not in ('assets', 'asset_groups', 'schema_version')))
for label, before in [('root', assets[0]), ('011', assets[2])]:
    changed = []
    for tile, a in assets[1].items():
        b = before.get(tile, {})
        if a.get('height') != b.get('height'):
            changed.append(tile)
    print('height_changes_vs', label, len(changed))
    for tile in changed:
        a, b = assets[1][tile], before.get(tile, {})
        if label == 'root' or tile in {
            'v1:4bpp:a19ae88181451d61', 'v1:4bpp:d00ecd4b62335cb1',
            'v1:4bpp:2ff1e249201dfe5b', 'v1:4bpp:37192c768ae456a4'}:
            print(tile, 'before', (min(b.get('height', [0])), max(b.get('height', [0]))),
                  'candidate', (min(a.get('height', [0])), max(a.get('height', [0]))),
                  'role', b.get('tile_role'), '->', a.get('tile_role'))
for room in ('055', '060', '061'):
    r = json.loads((output / f'room-{room}-validation-v2.json').read_text())
    s = json.loads((output / f'room-{room}-runtime.json').read_text())
    print('room', room, json.dumps({k: v for k, v in r.items() if not isinstance(v, (dict, list))}, sort_keys=True))
    d = r['directional_wall_continuity']
    print('directional', {k: d[k] for k in ('edge_denominator', 'conflicts', 'missing', 'status_counts')})
    print('runtime_conflicts', s['structural_conflicts'], 'issues', len(s['constraint_issues']),
          'component_issues', s['geometry_component_issues'])
    if room == '060':
        print('first_constraints', s['constraint_issues'][:16])
        print('first_issue_placements', s['issue_placements'][:16])
        print('report_list_keys', {k: len(v) for k, v in r.items() if isinstance(v, list)})
        print('compiled_mismatches', r['compiled_map_mismatches'])
        print('compiled_missing', r['compiled_map_missing'])
        print('unknown_walls', [(w['xy'], w.get('tile_hash'), w['local_min'], w['local_max']) for w in s['unresolved_wall_placements']])
        targets = ('fd86d3ed217c0d03', '73caad274b5bb270')
        for placement in r.get('placements', []):
            if any(h in str(placement.get('tile_hash')) for h in targets):
                print('obstacle', placement)
