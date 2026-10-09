#!/usr/bin/env python3
"""Summarize explicit packet audits and complete solver sources, read-only."""
import json
import sys
from collections import Counter
from pathlib import Path

TARGETS = {'eff6f17a9f4420d9', '2fd3eada43bdfcd4', '19088620e4b960cc'}

def main():
    packet = json.loads(Path(sys.argv[1]).read_text())
    print('packet schema/scene', packet['schema'], packet['scene_pixels'])
    for h in sorted({i['hash'] for i in packet['instances']}):
        instances = [i for i in packet['instances'] if i['hash'] == h]
        print(h, 'instances', len(instances), 'sources', Counter((i['role'], i['group'], i['bg'], i['base'], i['override']) for i in instances),
              'changed canonical samples', sum(p[0] != p[1] for i in instances for p in i['pixels']),
              'references', [sum(i['references'][b] for i in instances) for b in range(3)])
    if len(sys.argv) > 2:
        runtime = json.loads(Path(sys.argv[2]).read_text())
        for layer in runtime['layers']:
            for i, c in enumerate(layer['cells']):
                if c['hash'] in TARGETS:
                    print('placed', layer['bg'], c, 'template', layer['local_samples'][i]['template'])

if __name__ == '__main__':
    main()
