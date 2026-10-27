#!/usr/bin/env python3
"""Detect intact door writer's contiguous outer full-face extension bands.

Same writer, full width, exact single-band directional local ramps, and exact
door landing are mandatory. No nearby-floor or room-coordinate exceptions.
"""
import argparse,json,tomllib
from collections import Counter
from pathlib import Path
from alttp_room_geometry import RoomSnapshot,RoomProvenance
from alttp_doorway_detection import displayed

def expected_band(local,band,datum,axis,reverse):
 if len(local)!=64 or min(local)!=0 or max(local)!=7:return None
 if any(local[y*8+x] != ((7-(x if axis else y)) if reverse else (x if axis else y)) for y in range(8) for x in range(8)):return None
 return [datum+band*8+v for v in local]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('snapshot','terminal','profile','doorways','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();s=RoomSnapshot.read(a.snapshot);pr=RoomProvenance.read(Path(str(a.snapshot)+'.prov'),s);doors=json.loads(a.doorways.read_text());assets={v['tile_hash']:v for v in tomllib.loads(a.profile.read_text())['assets']}
 cells={}
 for line in a.terminal.read_text().splitlines():
  t=json.loads(line);key=(t['bg']-1,t['x'],t['y']);c=s.cell(*key);assert int(t['hash'],16)==int(c.tile_hash.split(':')[-1],16);cells[key]=dict(t,actual=displayed(t['actual'],c.word))
 records=[];gaps=[]
 for asm in doors['assemblies']:
  d=asm['direction'];ox,oy=asm['origin'];w,h=asm['width'],asm['height'];writer=asm['writer'];axis=int(d>=2);reverse=d in (0,2)
  seeds=[v for v in doors['placements'] if v['source']['writer']==writer and v.get('support_datum') is not None and (v['layer'],v['x'],v['y']) in map(tuple,asm['members'])]
  datums={v['support_datum'] for v in seeds}
  if len(datums)!=1:gaps.append(dict(assembly=asm,reason='missing_unique_landing'));continue
  datum=datums.pop();dx,dy=((0,-1),(0,1),(-1,0),(1,0))[d]
  outer=[tuple(v) for v in asm['members'] if (v[2]==(oy if d==0 else oy+h-1) if d<2 else v[1]==(ox if d==2 else ox+w-1))]
  for depth in range(1,64):
   keys=[(l,x+dx*depth,y+dy*depth) for l,x,y in outer]
   if any(not(0<=x<64 and 0<=y<64) or pr.owner(l,x,y)!=writer for l,x,y in keys):break
   candidates=[]
   for key in keys:
    c=s.cell(*key);asset=assets.get(c.tile_hash,{});t=cells[key];local=displayed(asset.get('height',[]),c.word)
    expected=expected_band(local,2+depth,datum,axis,reverse)
    if asset.get('tile_role')!='wall' or asset.get('geometry_by_flip') or asset.get('geometry_by_facing') or expected is None or not t['has_effective_height']:break
    candidates.append(dict(rule='door_outer_extension_unit_bands',layer=key[0],x=key[1],y=key[2],writer=writer,writer_record=pr.objects[writer-1],assembly_origin=asm['origin'],direction=d,depth=depth,support_datum=datum,expected=expected,actual=t['actual'],selected=t,authored_local=local))
   if len(candidates)!=len(keys):gaps.append(dict(assembly=asm,depth=depth,reason='non_full_linear_wall_extension'));break
   for v in candidates:
    v['mismatch_pixels']=[i for i,(x,y) in enumerate(zip(v['actual'],v['expected'])) if x!=y];v['status']='fail' if v['mismatch_pixels'] else 'pass';records.append(v)
 result=dict(approval=False,room=s.room,placements=records,status_counts=dict(Counter(v['status'] for v in records)),gaps=gaps,expectation='conditional_intact_door_external_landing_and_same_writer_full_width_unit_face_extension',limitations=['Equal-word reference ownership does not establish live dynamic writer history.','Single-band full wall arrays only; cap/cutout/overwritten/context geometry unsupported.'])
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['status_counts']))
if __name__=='__main__':main()
