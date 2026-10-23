"""Independent writer/handler attestation of the exact pot failure class."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sqlite3
import struct
from alttp_room_geometry import RoomSnapshot,RoomProvenance

def main():
 p=argparse.ArgumentParser()
 for n in ('corpus','atlas','rom','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();templates=a.rom.read_bytes()[0x1b52:0x1b52+12876]
 with sqlite3.connect(a.atlas) as db:objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
 bykey={(v['room_id'],v['layer'],v['ordinal']):v for v in objects}
 records=[]
 for raw in sorted(a.corpus.glob('room-*-entrance-*.raw')):
  s=RoomSnapshot.read(raw);prov=RoomProvenance.read(Path(str(raw)+'.prov'),s);attrs=Path(str(raw)+'.attr').read_bytes()[10:];claims={}
  for off in (0xe82,0xe92):
   words=struct.unpack_from('<4H',templates,off)
   for flip in range(4):
    for y in range(63):
     for x in range(63):
      cells=[s.cell(1,x+xx,y+yy) for xx in range(2) for yy in range(2)]
      if not all(c.word==(words[(1-xx if flip&1 else xx)*2+(1-yy if flip&2 else yy)] ^ (0x4000 if flip&1 else 0) ^ (0x8000 if flip&2 else 0)) for c,(xx,yy) in zip(cells,((0,0),(0,1),(1,0),(1,1)))):continue
      for plane in range(2):
       index=attrs[plane*4096+y*64+x]
       if not (0x70<=index<=0x7f and all(attrs[plane*4096+(y+yy)*64+x+xx]==index for xx in range(2) for yy in range(2))):continue
       owners={prov.owner(1,c.x,c.y) for c in cells};dispatches=[]
       for owner in owners:
        if owner<=0:continue
        w=prov.objects[owner-1];obj=bykey.get((s.room,w['phase'],w['ordinal']))
        dispatches.append(dict(writer=owner,dispatch=w,object=obj))
       valid=len(dispatches)==1 and dispatches[0]['object'] and ((dispatches[0]['object']['subtype']==1 and dispatches[0]['object']['object_id']==0x2f) or
                 (dispatches[0]['object']['subtype']==0 and dispatches[0]['object']['object_id'] in (0x95,0xbc)))
       claims[x,y,plane]=dict(xy=[x,y],plane=plane,template=off,flip=flip,index=index,status='attested_SinglePot' if valid else 'unproved_or_overwritten',writers=dispatches)
  records.append(dict(snapshot=str(raw),room=s.room,claims=list(claims.values())))
 summary=dict(contexts=len(records),rooms=len({v['room'] for v in records}),counts=dict(Counter(c['status'] for v in records for c in v['claims'])),records=records,
              scope='default complete contexts; exact indexed footprint independent provenance, no dynamic pickup states')
 a.output.write_text(json.dumps(summary,indent=2)+'\n');print({k:v for k,v in summary.items() if k!='records'})
if __name__=='__main__':main()
