"""Inventory exact placed diagonal cross sections from an immutable replay."""
import argparse
import json
import struct
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--probe', type=Path, required=True)
    a = p.parse_args()
    blob = Path(str(a.capture)+'.dungeon-input.bin').read_bytes()
    template = Path(str(a.capture)+'.dungeon-templates.bin').read_bytes()[8:]
    data = json.loads(a.probe.read_text())
    if 'records' in data:
        records=[r for r in data['records'] if r.get('status')=='audited']
        print('extended states',sum(r['extended_plane_placements']>0 for r in records),
              'extended plane placements',sum(r['extended_plane_placements'] for r in records),
              'preview plane placements',sum(r['preview_plane_placements'] for r in records),
              'rooms',[hex(r['room']) for r in records if r['extended_plane_placements']])
        for r in records:
            if r.get('room')!=0x61:continue
            print('ATTESTED',r.get('attested_preview_planes'),'UNPROVED',r.get('unproved_preview_planes'))
            for assembly in r.get('diagonal_dispatches',[]):
                print('DISPATCH',assembly['writer'],hex(assembly['handler']),assembly['origin'],assembly['repetitions'],
                      hex(assembly['template_offset']),assembly['omitted'])
            for c in r.get('ceiling_contacts',[])[:12]:
                print('CEILING',c)
        return
    if 'required_walkable_placements' in data:
        print({k:data[k] for k in ('required_walkable_placements','valid_walkable_placements',
              'solved_wall_cells','unknown_wall_cells','per_plane_walkable_denominator')})
        print('missing',len(data['missing_placements']),'seams',data.get('directional_seams'))
        return
    if 'layers' not in data:
        print(list(data))
        for key,value in data.items():
            if 'conflict' in key or 'issue' in key:
                print(key, value[:12] if isinstance(value,list) else value)
        return
    for off in (0x470, 0x47a, 0x4ca, 0x4e8, 0x4f2):
        print('template', hex(off), [hex(w) for w in struct.unpack_from('<5H', template, off)])
    for bg in (0, 1):
        print('BG', bg)
        layer = data['layers'][bg]
        for y in range(33, 51):
            print(y, ' '.join(f'{struct.unpack_from("<H",blob,8+(0 if bg else 8192)+2*(y*64+x))[0]:04x}' for x in range(4, 15)))
        for x in (11,12,13):
            for y in range(41, 48):
                i=y*64+x
                c=layer['cells'][i];s=layer['local_samples'][i]
                print(x,y,c['hash'],'word',hex(struct.unpack_from('<H',blob,8+(0 if bg else 8192)+i*2)[0]),
                      'template',hex(s['template']), 'base',c['base'],'role',c['role'], 'attr',s['attr'],
                      'span',c['local_span'],'rows',s['height'][:8],s['height'][56:])


if __name__ == '__main__':
    main()
