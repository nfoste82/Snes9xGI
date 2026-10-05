#!/usr/bin/env python3
"""Summarize complete-room directional validation artifacts without dropping denominators."""
import json
import sys
from pathlib import Path

for path in map(Path, sys.argv[1:]):
    report = json.loads(path.read_text())
    seam = report['directional_wall_continuity']
    print(path)
    print(json.dumps({k: seam[k] for k in ('per_layer_plane_wall_denominator', 'edge_denominator',
                                         'conflicts', 'missing', 'status_counts')}, sort_keys=True))
    for e in seam['edges']:
        if e['layer'] == 0 and e['plane'] == 1 and e['y'] == 10 and e['x'] in (41,42,43) and e['direction'] == 'east':
            print(json.dumps(e, sort_keys=True))
