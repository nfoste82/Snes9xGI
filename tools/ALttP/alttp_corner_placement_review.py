"""Read-only complete placement inventory neighborhood review (not a height rule)."""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('inventory', type=Path)
p.add_argument('--hashes', nargs='+', default=['a9b6cfd6b17c9385', 'ccf87d6be13c02a4'])
p.add_argument('--templates', type=Path)
a = p.parse_args()
cells = [json.loads(line) for line in a.inventory.read_text().splitlines()]
index = {(c['bg'], c['x'], c['y']): c for c in cells}
for c in cells:
    if c['hash'] not in a.hashes:
        continue
    print('TARGET', {k: v for k, v in c.items() if k != 'height'}, 'range', min(c['height']), max(c['height']))
    if a.templates:
        import struct
        templates = a.templates.read_bytes()[8:]
        for off in range(0xb66, 0xd47, 0x20):
            for axis in range(2):
                for part in range(4):
                    for start in range(2):
                        seq = [struct.unpack_from('<H', templates, off+2*((start+k)*4+part if axis else part*4+start+k))[0] for k in range(3)]
                        for position in range(3):
                            ns = [index.get((c['bg'], c['x']+(k-position if axis else 0), c['y']+(k-position if not axis else 0))) for k in range(3)]
                            if all(n and n['word']==w for n,w in zip(ns,seq)):
                                print('EXACT STRIP', hex(off), axis, part, start, position, [n['surface'] for n in ns])
    for y in range(c['y']-1, c['y']+2):
        for x in range(c['x']-1, c['x']+2):
            n = index.get((c['bg'], x, y))
            if n:
                print(x, y, n['hash'], 'role', n['surface'], 'template', hex(n['template']), 'base', n['base'], 'planes', n['planes'], 'attr', n['attributes'], 'rows', [n['height'][i:i+8] for i in range(0,64,8)])
