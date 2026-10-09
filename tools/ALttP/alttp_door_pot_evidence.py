"""Pinned door/pot failure-class inventory; never rewrites profile metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import tomllib
import struct
import sqlite3
from alttp_room_geometry import RoomSnapshot, RoomProvenance

TARGETS = ('482a1674f4994ba7', '3082443c6bd2ebc1', 'b225fad8c5498046',
           '71232b754e969748', 'af36e4c098c89b82')

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--replay', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--reference',type=Path)
    p.add_argument('--atlas',type=Path)
    a = p.parse_args()
    pins = {}
    for suffix in ('', '.profile.toml', '.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin'):
        f = Path(str(a.capture) + suffix)
        pins[f.name] = {'sha256': hashlib.sha256(f.read_bytes()).hexdigest(),
                        'size': f.stat().st_size, 'mtime_ns': f.stat().st_mtime_ns}
    profile = tomllib.loads(Path(str(a.capture) + '.profile.toml').read_text())
    assets = {v['tile_hash'].split(':')[-1]: v for v in profile['assets']}
    replay = json.loads(a.replay.read_text())
    result = {'pins': pins, 'metadata': {k: assets.get(k) for k in TARGETS}, 'placements': []}
    data = Path(str(a.capture)+'.dungeon-input.bin').read_bytes()
    templates = Path(str(a.capture)+'.dungeon-templates.bin').read_bytes()[8:]
    words = struct.unpack_from('<8192H', data, 8)
    if a.reference:
        ref=RoomSnapshot.read(a.reference);prov=RoomProvenance.read(Path(str(a.reference)+'.prov'),ref)
        assert all(words[(0 if l==1 else 4096)+y*64+x]==ref.cell(l,x,y).word for l in range(2) for y in range(64) for x in range(64))
        assert data[16392:24584]==Path(str(a.reference)+'.attr').read_bytes()[10:]
        records={}
        if a.atlas:
            with sqlite3.connect(a.atlas) as db:
                records={(v['layer'],v['ordinal']):v for row in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")
                         if (v:=json.loads(row[0]))['room_id']==ref.room}
        result['final_writer_evidence']=[]
        for layer in replay['layers']:
            for cell in layer['cells']:
                if cell['hash'] not in TARGETS:continue
                x,y=cell['xy'];owner=prov.owner(layer['bg'],x,y);w=prov.objects[owner-1] if owner>0 else None
                result['final_writer_evidence'].append(dict(bg=layer['bg'],xy=[x,y],writer=owner,dispatch=w,
                    object=records.get((w['phase'],w['ordinal'])) if w else None))
    result['door_template_matches'] = []
    for d,(start,end) in enumerate(((0x2716,0x2926),(0x29f0,0x2be8),(0x2c6a,0x2e32),(0x2e6a,0x3032))):
        w,h = (4,3) if d<2 else (3,4)
        for off in range(start,end+1,24):
            expected = struct.unpack_from('<12H', templates, off)
            for y in range(65-h):
                for x in range(65-w):
                    if all((words[(y+yy)*64+x+xx]&0xdfff)==(expected[xx*h+yy]&0xdfff) for xx in range(w) for yy in range(h)):
                        result['door_template_matches'].append(dict(direction=d,offset=hex(off),xy=[x,y],words=list(expected)))
    for layer in replay['layers']:
        for cell, samples in zip(layer['cells'], layer['local_samples']):
            if cell['hash'] in TARGETS:
                xy = cell['xy']
                neighbors = [v for v in layer['cells'] if abs(v['xy'][0]-xy[0])+abs(v['xy'][1]-xy[1]) == 1]
                result['placements'].append({'bg': layer['bg'], **cell, **samples,
                    'frame_attachments': [v for v in layer['doorframes'] if v['xy'] == xy],
                    'neighbors': neighbors})
    a.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'placements': [{k:v for k,v in r.items() if k not in ('height','neighbors')}
                                    for r in result['placements']],
                     'doors': result['door_template_matches'],
                     'metadata': {k: {f:v for f,v in assets[k].items() if f not in ('height','normal_xyz','occlusion','emission_rgba')}
                                  for k in TARGETS}}, indent=2))

if __name__ == '__main__':
    main()
