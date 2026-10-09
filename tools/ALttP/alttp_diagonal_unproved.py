#!/usr/bin/env python3
"""Inspect every unproved use of a discovery identity in saved complete maps."""
import argparse
import json
from pathlib import Path
import sqlite3

from alttp_diagonal_sets import inspect
from alttp_room_geometry import RoomSnapshot, RoomProvenance


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--corpus', type=Path, required=True)
    p.add_argument('--atlas', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--identity', required=True)
    a = p.parse_args()
    with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro', uri=True) as db:
        objects = [json.loads(r[0]) for r in db.execute(
            "SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    templates = a.rom.read_bytes()[0x1b52:0x1b52+12876]
    result = []
    for raw in sorted(a.corpus.glob('*.raw')):
        s = RoomSnapshot.read(raw)
        prov = RoomProvenance.read(Path(str(raw)+'.prov'), s)
        room_objects = {(o['layer'],o['ordinal']):o for o in objects if o['room_id']==s.room}
        assemblies,claims = inspect(s,prov,room_objects,templates)
        for layer in s.cells:
            for cell in layer:
                if cell.tile_hash != a.identity or len(claims.get((cell.layer,cell.x,cell.y),set())) == 1:
                    continue
                writer = prov.owner(cell.layer,cell.x,cell.y)
                source = prov.objects[writer-1] if 0 < writer <= len(prov.objects) else None
                neighborhoods = []
                for y in range(max(0,cell.y-3), min(64,cell.y+4)):
                    for x in range(max(0,cell.x-3), min(64,cell.x+4)):
                        c = s.cell(cell.layer,x,y)
                        neighborhoods.append(dict(x=x,y=y,word=c.word,tile_hash=c.tile_hash,
                                                  writer=prov.owner(cell.layer,x,y)))
                result.append(dict(raw=str(raw),state=json.loads(Path(str(raw)+'.state.json').read_text()),
                                   room=s.room,layer=cell.layer,x=cell.x,y=cell.y,word=cell.word,
                                   hflip=cell.hflip,vflip=cell.vflip,writer=writer,source=source,
                                   object=room_objects.get((source['phase'],source['ordinal'])) if source else None,
                                   neighborhood=neighborhoods,assemblies=assemblies))
    (a.corpus/'unproved-identity.json').write_text(json.dumps(result,indent=2)+'\n')
    for r in result:
        print(json.dumps({k:v for k,v in r.items() if k not in ('neighborhood','assemblies')},indent=2))


if __name__ == '__main__':
    main()
