"""Generic own complete-room v2 gate; transfer transitions only on exact attrs."""
import argparse,json,hashlib,struct,subprocess,sys
from pathlib import Path
from alttp_collision_reachability import doorway_graph,body_cells,read_attributes
p=argparse.ArgumentParser()
for n in ('capture','output','rom','fixture'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();s=json.loads(Path(str(a.capture)+'.dungeon.json').read_text());raw=a.output/'live-solver-input.raw';attr=Path(str(raw)+'.attr')
prior=json.loads(a.fixture.read_text());digest=hashlib.sha256(attr.read_bytes()).hexdigest();exact=prior['attribute_sha256']==digest
edges=prior['graph']['stair_edges'] if exact else []
planes=read_attributes(attr.read_bytes(),s['room']);graph,positions=doorway_graph(planes,(s['plane'],*(v&511 for v in s['link_xy'])),edges)
pin=dict(room_id=s['room'],link_xy=s['link_xy'],plane=s['plane'],collision_mode=s['collision_mode'],rom_sha256=s['rom_sha256'],source_capture=str(a.capture),transition_evidence='exact collision/reference directed playback' if exact else 'stair edges withheld: changed collision')
g=dict(format='alttp-doorway-graph-review-v1',room_id=s['room'],state=pin,graph=graph,elevation={},attribute_sha256=digest,planes=[dict(plane=str(i),reachable_foot_positions=len(v),reachable_body_cells=body_cells(v,planes[i])) for i,v in enumerate(positions)])
(a.output/'live-state.json').write_text(json.dumps(pin,indent=2)+'\n');(a.output/'live-graph.json').write_text(json.dumps(g,indent=2)+'\n')
r=json.loads((a.output/'live-runtime.json').read_text());bases=[[255]*4096 for _ in range(2)];visible=[255]*4096
for p,x,y,b in r['cells_plane_x_y_base']:
 i=y*64+x;bases[p][i]=b;visible[i]=b if visible[i]==255 else max(visible[i],b)
for v in r['structural_placements']:
 i=v['y']*64+v['x'];visible[i]=v['floor_base']
 # ALTPHM3 carries independent structural/support arrays, not just the visible
 # byte map. Dropping structural plane claims fabricates compiled-map mismatches
 # whenever a solved wall has no walkable support at its own coordinate.
 for p in v.get('planes',[]):
  bases[p][i]=v['floor_base']
compiled=a.output/'review-map.bin';compiled.write_bytes(b'ALTPHM3\0'+struct.pack('<H',s['room'])+struct.pack('<12288i',*(visible+bases[0]+bases[1])))
cmd=[sys.executable,'-B','remaster/tools/ALttP/alttp_validate_room_heights.py','--graph',str(a.output/'live-graph.json'),'--snapshot',str(raw),'--attributes',str(attr),'--state',str(a.output/'live-state.json'),'--profile',str(a.output/'replay-input-profile.toml'),'--runtime-solution',str(a.output/'live-runtime.json'),'--height-map',str(compiled),'--rom',str(a.rom),'--output',str(a.output/'validation-v2.json'),'--diagnostic-svg',str(a.output/'validation-v2.svg')]
run=subprocess.run(cmd,capture_output=True,text=True)
pins={v or 'packet':hashlib.sha256(Path(str(a.capture)+v).read_bytes()).hexdigest() for v in ('','.profile.toml','.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin','.terrain-input.bin')}
(a.output/'gate.json').write_text(json.dumps(dict(command=cmd,exit=run.returncode,stdout=run.stdout,stderr=run.stderr,pins=pins,state={k:v for k,v in s.items() if k not in ('plane_bases','visible_bases')},exact_collision=exact,reachable_denominators=[len(v['reachable_body_cells']) for v in g['planes']],approval=False),indent=2)+'\n')
print('room',hex(s['room']),'validator',run.returncode,'supports',[len(v['reachable_body_cells']) for v in g['planes']],'exact_collision',exact,run.stderr)
