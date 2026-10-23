"""Read-only exact captured OBJ/ground-pot artwork relation and current metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import tomllib

p=argparse.ArgumentParser()
p.add_argument('--capture',type=Path,required=True)
p.add_argument('--profile',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
blob=Path(str(a.capture)+'.dungeon-input.bin').read_bytes()
vram=blob[24584:]
templates=Path(str(a.capture)+'.dungeon-templates.bin').read_bytes()[8:]
current_bytes=a.profile.read_bytes()
current={v['tile_hash']:v for v in tomllib.loads(current_bytes.decode())['assets']}
captured={v['tile_hash']:v for v in tomllib.loads(Path(str(a.capture)+'.profile.toml').read_text())['assets']}
def decode(address):
    return [sum(((vram[(address+(b//2)*16+y*2+b%2)&65535]>>(7-x))&1)<<b for b in range(4)) for y in range(8) for x in range(8)]
def tile_id(pixels):
    h=14695981039346656037
    # v1 hash prefix is pinned by the portable tile identity implementation.
    for b in [1,1,4,8,8,*pixels]:h=((h^b)*1099511628211)&((1<<64)-1)
    return f'v1:4bpp:{h:016x}'
def transform(pixels,h,v):
    return [pixels[(7-y if v else y)*8+(7-x if h else x)] for y in range(8) for x in range(8)]
ground=[]
for off in (0xe82,0xe92):
    for part in range(4):
        w=int.from_bytes(templates[off+part*2:off+part*2+2],'little')
        ground.append(dict(template=hex(off),part=part,word=w,pixels=decode(16384+(w&1023)*32)))
# USA OBJ name base is verified against the captured known identities, not
# assumed interchangeable with BG character base. Search candidate VRAM bases.
ids=['21c1767459218910','a9b04900f78069e6','ac36befaa085a824','a98b371291ed848a']
records=[]
for name,key in zip((0x46,0x47,0x56,0x57),ids):
    full='v1:4bpp:'+key
    addresses=[at for at in range(0,65536,32) if tile_id(decode(at))==full]
    relations=[]
    for at in addresses:
        pixels=decode(at)
        for g in ground:
            for h in (False,True):
                for v in (False,True):
                    if pixels==transform(g['pixels'],h,v):
                        relations.append(dict(address=at,template=g['template'],part=g['part'],hflip=h,vflip=v))
    metadata=current.get(full,{})
    records.append(dict(name=hex(name),hash=full,addresses=addresses,exact_ground_relations=relations,
                        captured_height_count=len(captured.get(full,{}).get('height',[])),
                        current_height=metadata.get('height'),current_normals_count=len(metadata.get('normal_xyz',[]))))
report=dict(capture=str(a.capture),profile=str(a.profile),profile_sha256=hashlib.sha256(current_bytes).hexdigest(),
            profile_changed_during_audit=a.profile.read_bytes()!=current_bytes,authored_changes=False,records=records)
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
