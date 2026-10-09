#!/usr/bin/env python3
"""Trace a placed footprint against exact captured ROM template words."""
import struct
import sys
from pathlib import Path

prefix = sys.argv[1]
blob = Path(prefix + '.dungeon-input.bin').read_bytes()
templates = Path(prefix + '.dungeon-templates.bin').read_bytes()[8:]
words = struct.unpack('<8192H', blob[8:16392])
x, y = map(int, sys.argv[2:4])
for off in (0xe26, 0xed2, 0xf5e):
    print('statue', hex(off), [hex(v) for v in struct.unpack_from('<6H', templates, off)])
for w, h in ((2, 3), (2, 4), (4, 4)):
    footprint = [words[(y+yy)*64+x+xx] & 0xdfff for xx in range(w) for yy in range(h)]
    print(w, h, [hex(v) for v in footprint])
    print('exact offsets', [hex(off) for off in range(0, len(templates)-w*h*2+1, 2)
          if [v & 0xdfff for v in struct.unpack_from('<'+'H'*(w*h), templates, off)] == footprint])
    print('identity offsets', [hex(off) for off in range(0, len(templates)-w*h*2+1, 2)
          if [v & 0xc3ff for v in struct.unpack_from('<'+'H'*(w*h), templates, off)] == [v & 0xc3ff for v in footprint]])
    print('first word offsets', [hex(off) for off in range(0,len(templates),2) if struct.unpack_from('<H',templates,off)[0]&0xc3ff==footprint[0]&0xc3ff])
