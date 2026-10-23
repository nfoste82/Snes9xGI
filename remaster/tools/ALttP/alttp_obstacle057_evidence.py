"""Own057 target ROM writer/relief inventory; no current profile substitution."""
import argparse,json,tomllib,struct,hashlib
from pathlib import Path
from alttp_room_geometry import RoomSnapshot,RoomProvenance
p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--review',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);a=p.parse_args()
live=RoomSnapshot.read(a.review/'live-solver-input.raw');ref=RoomSnapshot.read(a.reference);prov=RoomProvenance.read(Path(str(a.reference)+'.prov'),ref)
assets={v['tile_hash']:v for v in tomllib.loads(Path(str(a.capture)+'.profile.toml').read_text())['assets']}
target='v1:4bpp:eff6f17a9f4420d9';records=[]
for layer in (0,1):
 for i,c in enumerate(live.cells[layer]):
  if c.tile_hash!=target:continue
  owner=prov.owners[layer][i];obj=prov.objects[owner-1] if 0<owner<=len(prov.objects) else None
  members=[]
  for j,v in enumerate(live.cells[layer]):
   if prov.owners[layer][j]!=owner:continue
   asset=assets.get(v.tile_hash,{})
   members.append(dict(x=v.x,y=v.y,word=v.word,hash=v.tile_hash,hflip=v.hflip,vflip=v.vflip,
    height=asset.get('height'),role=asset.get('tile_role'),same_reference_word=v.word==ref.cells[layer][j].word))
  records.append(dict(layer=layer,target=[c.x,c.y],writer=owner,dispatch=obj,members=members))
(a.review/'target-writer-evidence.json').write_text(json.dumps(records,indent=2)+'\n')
for r in records[::2]:
 print('target',r['layer'],r['target'],'writer',r['writer'],r['dispatch'])
 for v in r['members']:print(v['x'],v['y'],hex(v['word']),v['hash'],'height',None if v['height'] is None else (min(v['height']),max(v['height'])),'role',v['role'],'match',v['same_reference_word'])
if (a.review/'corpus-after.json').exists():
 before=json.loads((a.review/'corpus-before.json').read_text());after=json.loads((a.review/'corpus-after.json').read_text())
 sums=lambda r:dict(assemblies=sum(v['obstacle_assemblies'] for v in r['records']),parts=sum(v['obstacle_assembly_cells'] for v in r['records']),solved=sum(v['obstacle_assembly_solved'] for v in r['records']),part_base_conflicts=sum(v['obstacle_part_base_conflicts'] for v in r['records']),unknown_walls=sum(v['unknown_walls'] for v in r['records']),conflicts=sum(v['conflicts'] for v in r['records']),missing=[sum(v['missing'][i] for v in r['records']) for i in range(2)])
 changed=[dict(room=y['room'],before=x,after=y) for x,y in zip(before['records'],after['records']) if any(x[k]!=y[k] for k in ('obstacle_assemblies','obstacle_assembly_cells','obstacle_assembly_solved','conflicts','unknown_walls','missing'))]
 summary=dict(contexts=after['contexts'],rooms=after['rooms'],before=sums(before),after=sums(after),changed_contexts=len(changed),changed_rooms=len({v['room'] for v in changed}),changes=changed,unmodeled_room_ids=320-after['rooms'],dynamic_states_unmodeled=True)
 (a.review/'corpus-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('corpus',{k:v for k,v in summary.items() if k!='changes'})
