"""Read-only seam/source investigation of unresolved semantic diagonals."""
import argparse,json,struct,tomllib
from pathlib import Path
from collections import Counter
from alttp_room_geometry import RoomSnapshot,RoomProvenance

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--gaps',type=Path,required=True);p.add_argument('--corpus',type=Path,required=True)
 p.add_argument('--profile',type=Path,required=True);p.add_argument('--rom',type=Path,required=True)
 a=p.parse_args();assets={v['tile_hash']:v for v in tomllib.loads(a.profile.read_text())['assets']}
 report=json.loads(a.gaps.read_text());templates=a.rom.read_bytes()[0x1b52:0x1b52+12876]
 for e in report['entries']:
  if not e['gap']:continue
  print('GAP',e['tile_hash'])
  print('TEMPLATE BANDS',dict(Counter((r['family'],r['band'],r['word']) for r in e['templates'])))
  occurrences=e['placements'];counts=Counter();examples=[]
  cache={}
  for c in occurrences:
   raw=a.corpus/f"room-{c['room']:03x}-entrance-{c['entrance']}.raw"
   if raw not in cache:
    s=RoomSnapshot.read(raw);cache[raw]=(s,RoomProvenance.read(Path(str(raw)+'.prov'),s))
   s,prov=cache[raw]
   for direction,dx,dy in [('north',0,-1),('south',0,1),('west',-1,0),('east',1,0)]:
    x,y=c['x']+dx,c['y']+dy
    if not 0<=x<64 or not 0<=y<64:continue
    n=s.cell(c['layer'],x,y);asset=assets.get(n.tile_hash,{})
    counts[direction,n.tile_hash,tuple(asset.get('height',[])),n.hflip,n.vflip]+=1
   if len(examples)<1:
    cells=[]
    for y in range(max(0,c['y']-3),min(64,c['y']+4)):
     for x in range(max(0,c['x']-3),min(64,c['x']+4)):
      n=s.cell(c['layer'],x,y);asset=assets.get(n.tile_hash,{})
      cells.append(dict(x=x,y=y,word=hex(n.word),id=n.tile_hash,writer=prov.owner(n.layer,x,y),
        height=asset.get('height'),normal=asset.get('normal_xyz'),role=asset.get('tile_role')))
    examples.append(cells)
  print('CONTACTS',[(d,key,(min(h),max(h)) if h else None,hf,vf,num) for (d,key,h,hf,vf),num in counts.items()])
  print('EXAMPLE',json.dumps(examples))
  if e['templates']:
   r=e['templates'][0];raw=a.corpus/next(v.name for v in a.corpus.glob(f"room-{r['room']:03x}-entrance-*.raw"))
   s=RoomSnapshot.read(raw)
   for off in (0x328,0x308,0x420+(r['handler']-9)*10):
    print('WORDS',hex(off),[(hex(w),s._tile_hash((s.layers[1][1]+(w&1023)*16)&32767)) for w in struct.unpack_from('<5H',templates,off)])

if __name__=='__main__':main()
