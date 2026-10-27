#!/usr/bin/env python3
"""Read-only complete-room internal cardinal-band joins; no raster pass claim.

Extends the ROM/writer-backed three-band census to same-BG same-writer depth
joins. Conditional expectation is exact unit slope of selected intrinsic face.
"""
import argparse,json,sqlite3,hashlib
from collections import Counter
from pathlib import Path
from alttp_room_geometry import RoomSnapshot,RoomProvenance
from alttp_exposed_height_detection import cardinal_claims
from alttp_doorway_detection import displayed,ROM_SHA

def join(a,b,axis,reverse):
 if not a['has_effective_height'] or not b['has_effective_height']:return None
 la=[v-a['base'] for v in a['actual']];lb=[v-b['base'] for v in b['actual']]
 # Require complete selected single-band linear faces, not rounded/cutout art.
 slope=1 if reverse else -1
 def linear(v):
  return all(v[y*8+x]-v[0]==slope*(x if axis else y) for y in range(8) for x in range(8))
 if not linear(la) or not linear(lb):return None
 pairs=[(k*8+7,k*8) for k in range(8)] if axis else [(56+k,k) for k in range(8)]
 return [dict(a_k=i,b_k=j,a_height=a['actual'][i],b_height=b['actual'][j],expected_b=a['actual'][i]+slope,residual=b['actual'][j]-a['actual'][i]-slope) for i,j in pairs]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('snapshot','rom','atlas','terminal','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();rom=a.rom.read_bytes();assert hashlib.sha256(rom).hexdigest()==ROM_SHA
 s=RoomSnapshot.read(a.snapshot);pr=RoomProvenance.read(Path(str(a.snapshot)+'.prov'),s)
 with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro',uri=True) as db:objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
 objects={(o['layer'],o['ordinal']):o for o in objects if o['room_id']==s.room}
 claims=cardinal_claims(s,pr,objects,rom[0x1b52:0x1b52+12876]);cells={}
 for l in a.terminal.read_text().splitlines():
  t=json.loads(l);key=(t['bg'],t['x'],t['y']);c=s.cell(key[0]-1,*key[1:]);assert int(t['hash'],16)==int(c.tile_hash.split(':')[-1],16)
  cells[key]=dict(t,actual=displayed(t['actual'],c.word))
 assert len(cells)==8192
 records=[];unsupported=[]
 for key,claim in sorted(claims.items()):
  if not claim:continue
  bg,x,y=key;other=(bg,x+int(bool(claim['axis'])),y+int(not claim['axis']));cb=claims.get(other)
  if not cb or claim!=cb:continue
  v=join(cells[key],cells[other],claim['axis'],claim['reverse'])
  if v is None:unsupported.append(dict(a_cell=key,b_cell=other,claim=claim));continue
  records.append(dict(a_cell=key,b_cell=other,claim=claim,pixels=v,status='fail' if any(q['residual'] for q in v) else 'pass',expectation='conditional_selected_linear_face_unit_continuation',a_selected=cells[key],b_selected=cells[other]))
 result=dict(approval=False,room=s.room,complete_placements=8192,proved_cells=len(claims),status_counts=dict(Counter(r['status'] for r in records)),records=records,unsupported=unsupported,limitations=['Relative internal band seams only; no absolute datum, capture visibility, closed-door or overlay proof.','Equal-word reference provenance is research only when transferred.'])
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['status_counts']))
if __name__=='__main__':main()
