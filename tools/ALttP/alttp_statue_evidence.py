"""Immutable own050 statue identities and matching reference dispatch inventory."""
import argparse,json,tomllib,collections
from pathlib import Path
from alttp_room_geometry import RoomSnapshot,RoomProvenance
p=argparse.ArgumentParser();p.add_argument('--review',type=Path,required=True);p.add_argument('--profile',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);a=p.parse_args()
live=RoomSnapshot.read(a.review/'live-solver-input.raw');ref=RoomSnapshot.read(a.reference);prov=RoomProvenance.read(Path(str(a.reference)+'.prov'),ref)
profile=tomllib.loads(a.profile.read_text());assets={v['tile_hash']:v for v in profile['assets']}
target='v1:4bpp:8044813840be1f26';records=[]
placements=json.loads((a.review/'statues-before-placements.json').read_text())
templates=Path(str(a.profile).removesuffix('.profile.toml')+'.dungeon-templates.bin').read_bytes()[8:]
import struct
for off in (0xe26,0xed2,0xf5e,0x84a,0x85a):
    h=4 if off in (0x84a,0x85a) else 3
    words=struct.unpack_from('<'+str(h*2)+'H',templates,off)
    for layer in (0,1):
     for y in range(65-h):
      for x in range(63):
            cells=[(y+yy)*64+x+xx for xx in range(2) for yy in range(h)]
            if any((live.cells[layer][i].word&0xdfff)!=(w&0xdfff) for i,w in zip(cells,words)):continue
            members=[]
            for i in cells:
                c=live.cells[layer][i];t=placements[i];asset=assets.get(c.tile_hash,{})
                owner=prov.owners[layer][i]
                members.append(dict(x=c.x,y=c.y,word=c.word,hash=c.tile_hash,
                    height_span=[min(asset['height']),max(asset['height'])] if 'height' in asset else None,
                    role=asset.get('tile_role'),attrs=t['attrs'],writer=owner,
                    same_reference_word=c.word==ref.cells[layer][i].word,
                    dispatch=prov.objects[owner-1] if 0<owner<=len(prov.objects) else None))
            records.append(dict(layer=layer,template=hex(off),origin=[x,y],members=members))
for i,c in enumerate(live.cells[1]):
    if c.tile_hash!=target:continue
    owner=prov.owners[1][i];obj=prov.objects[owner-1] if 0<owner<=len(prov.objects) else None
    members=[]
    for j,v in enumerate(live.cells[1]):
        if prov.owners[1][j]!=owner:continue
        asset=assets.get(v.tile_hash,{})
        members.append(dict(x=v.x,y=v.y,word=v.word,hash=v.tile_hash,height_span=[min(asset['height']),max(asset['height'])] if 'height' in asset else None,role=asset.get('tile_role'),same_reference_word=v.word==ref.cells[1][j].word))
    records.append(dict(target=[c.x,c.y],writer=owner,dispatch=obj,members=members))
(a.review/'statue-own-evidence.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(records,indent=2))
print('target own layers',[[[c.x,c.y] for c in layer if c.tile_hash==target] for layer in live.cells])
print('ROM template words',{hex(off):[hex(v) for v in struct.unpack_from('<6H',templates,off)] for off in (0xe26,0xed2,0xf5e)})
print('target-like placed tile numbers',collections.Counter(hex(c.word&1023) for layer in live.cells for c in layer if (c.word&1023) in {v&1023 for off in (0xe26,0xed2,0xf5e) for v in struct.unpack_from('<6H',templates,off)}))
