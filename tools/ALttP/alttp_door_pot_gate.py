"""Complete matching live gate, reusing only identical reference transition inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
from alttp_room_geometry import RoomSnapshot
from alttp_collision_reachability import doorway_graph, stair_edges_from_trace, body_cells, read_attributes

def main():
    p=argparse.ArgumentParser()
    for n in ('capture','output','reference','route','rom'):
        p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--production-map',type=Path)
    a=p.parse_args();out=a.output
    state=json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    raw=out/'live-solver-input.raw';attr=Path(str(raw)+'.attr')
    live,ref=RoomSnapshot.read(raw),RoomSnapshot.read(a.reference)
    assert all(x.word==y.word for l in range(2) for x,y in zip(live.cells[l],ref.cells[l]))
    assert attr.read_bytes()==Path(str(a.reference)+'.attr').read_bytes()
    edges=stair_edges_from_trace([json.loads(v) for v in a.route.read_text().splitlines()],state['room'])
    assert len(edges)==2
    planes=read_attributes(attr.read_bytes(),state['room'])
    graph,positions=doorway_graph(planes,(state['plane'],*(v&511 for v in state['link_xy'])),edges)
    pin=dict(room_id=state['room'],link_xy=state['link_xy'],plane=state['plane'],collision_mode=state['collision_mode'],
             source_capture=str(a.capture),rom_sha256=state['rom_sha256'],historical_entry='not attested',
             transition_evidence='reference playback on identical full maps/attributes, not fresh Snes9x parity')
    g=dict(format='alttp-doorway-graph-review-v1',room_id=state['room'],state=pin,graph=graph,elevation={},
           attribute_sha256=hashlib.sha256(attr.read_bytes()).hexdigest(),
           planes=[dict(plane=str(i),reachable_foot_positions=len(v),reachable_body_cells=body_cells(v,planes[i])) for i,v in enumerate(positions)])
    (out/'live-graph.json').write_text(json.dumps(g,indent=2)+'\n');(out/'live-state.json').write_text(json.dumps(pin,indent=2)+'\n')
    solution=json.loads((out/'live-runtime.json').read_text())
    bases=[[255]*4096 for _ in range(2)];visible=[255]*4096
    for plane,x,y,b in solution['cells_plane_x_y_base']:
        i=y*64+x;bases[plane][i]=b;visible[i]=b if visible[i]==255 else max(visible[i],b)
    for v in solution['structural_placements']:visible[v['y']*64+v['x']]=v['floor_base']
    compiled=out/'review-map.bin';compiled.write_bytes(b'ALTPHM3\0'+struct.pack('<H',state['room'])+struct.pack('<12288i',*(visible+bases[0]+bases[1])))
    if a.production_map:compiled=a.production_map
    cmd=[sys.executable,'-B','tools/ALttP/alttp_validate_room_heights.py','--graph',str(out/'live-graph.json'),
         '--snapshot',str(raw),'--attributes',str(attr),'--state',str(out/'live-state.json'),
         '--profile',str(out/'replay-input-profile.toml'),'--runtime-solution',str(out/'live-runtime.json'),
         '--height-map',str(compiled),'--rom',str(a.rom),'--output',str(out/'validation-v2.json'),
         '--diagnostic-svg',str(out/'validation-v2.svg')]
    r=subprocess.run(cmd,capture_output=True,text=True)
    (out/'gate.json').write_text(json.dumps(dict(command=cmd,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
                      doorway_denominators=[len(v['reachable_body_cells']) for v in g['planes']],approval=False),indent=2)+'\n')
    print('validator',r.returncode,'planes',[len(v['reachable_body_cells']) for v in g['planes']],r.stderr)
    if (out/'validation-v2.json').exists():
        v=json.loads((out/'validation-v2.json').read_text())
        summary={k:v[k] for k in ('required_walkable_placements','valid_walkable_placements','per_plane_walkable_denominator',
                                  'plane_overlap_cells','required_fallback_placements','valid_fallback_placements',
                                  'solved_wall_cells','unknown_wall_cells','success')}
        for k in ('missing_placements','conflicting_components','compiled_map_missing','compiled_map_mismatches'):summary[k]=len(v.get(k,[]))
        summary['doorframes']={k:x for k,x in v.get('doorframes',{}).items() if k!='issues'}
        summary['doorframe_issues']=len(v.get('doorframes',{}).get('issues',[]))
        summary['persistent_support']={k:x for k,x in v.get('persistent_support',{}).items() if k!='placements'}
        summary['validator_exit']=r.returncode
        (out/'gate-summary.json').write_text(json.dumps(summary,indent=2)+'\n')

if __name__=='__main__':main()
