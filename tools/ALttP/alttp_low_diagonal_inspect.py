"""Print ROM-derived LOW template identities and pinned cardinal metadata."""
import json
from pathlib import Path
import struct
import tomllib
import hashlib

from alttp_room_geometry import RoomSnapshot


def main():
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--raw', type=Path, required=True)
    p.add_argument('--profile', type=Path, required=True)
    a = p.parse_args()
    prefix = str(a.capture)
    state = json.loads(Path(prefix+'.dungeon.json').read_text())
    blob = Path(prefix+'.dungeon-input.bin').read_bytes()
    vram = blob[24584:]
    templates = Path(prefix+'.dungeon-templates.bin').read_bytes()[8:]
    pinned = Path(prefix+'.profile.toml').read_bytes()
    fnv = 14695981039346656037
    for b in pinned:
        fnv = ((fnv ^ b)*1099511628211)&((1<<64)-1)
    assert f'{fnv:x}' == state['profile_serialized_fnv1a64']
    print('CAPTURE', a.capture, state, hashlib.sha256(pinned).hexdigest())
    snapshot = RoomSnapshot.read(a.raw)
    assets = {v['tile_hash']:v for v in tomllib.loads(a.profile.read_text())['assets']}
    old = {v['tile_hash']:v for v in tomllib.loads(pinned.decode())['assets']}
    def identity(word):
        data = [sum(((vram[(state['graphics_base']+(word&1023)*32+(z//2)*16+y*2+z%2)&65535]>>(7-x))&1)<<z
                    for z in range(4)) for y in range(8) for x in range(8)]
        h = 14695981039346656037
        for b in [1,1,4,8,8]+data:
            h = ((h^b)*1099511628211)&((1<<64)-1)
        return f'v1:4bpp:{h:016x}', data
    ids = set()
    for handler in range(21,33):
        words = struct.unpack_from('<5H', templates, 0x498+(handler-21)*10)
        keys = [identity(w)[0] for w in words]
        ids.update(keys)
        print('LOW',hex(handler),list(map(hex,words)),keys)
    for off in (0x328,0x338):
        print('CARDINAL',hex(off))
        for w in struct.unpack_from('<8H',templates,off):
            key,data=identity(w)
            print(hex(w),key,'height',assets.get(key,{}).get('height'), 'pinned',old.get(key,{}).get('height'))
    for key in sorted(ids):
        print('ASSET',key,'current',assets.get(key),'pinned',old.get(key))
        positions=[]
        for layer in snapshot.cells:
            positions.extend((c.layer,c.x,c.y,hex(c.word)) for c in layer if c.tile_hash==key)
        print('PLACEMENTS',positions)
        word=next(w for w in struct.unpack_from('<60H',templates,0x498) if identity(w)[0]==key)
        data=identity(word)[1]
        print('ART', [''.join(format(v,'x') for v in data[y*8:y*8+8]) for y in range(8)])


if __name__ == '__main__':
    main()
