"""Room071 regression evidence: pinned solver, observed reference edges, strict scope."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
from alttp_collision_reachability import doorway_graph,body_cells,read_attributes,stair_edges_from_trace

p=argparse.ArgumentParser(description=__doc__)
for n in ('captures','output','solver','rom','routes'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();assert a.output.is_dir()
traces=[]
for name in ('north','south'):
    raw=a.routes/(name+'.raw');attr=Path(str(raw)+'.attr').read_bytes()[10:]
    trace=[json.loads(v) for v in Path(str(raw)+'.route.jsonl').read_text().splitlines()]
    traces.append((attr,trace))
records=[]
for suffix in ('001','007','008'):
    capture=a.captures/f'frame_room071_{suffix}.s9xrmf';out=a.output/capture.stem;out.mkdir(exist_ok=True)
    subprocess.run([sys.executable,'-B','remaster/tools/ALttP/alttp_replay_live_height_evidence.py',str(capture),'--solver',str(a.solver),'--rom',str(a.rom),'--output',str(out)],check=True)
    s=json.loads(Path(str(capture)+'.dungeon.json').read_text());raw=out/'live-solver-input.raw';attr=Path(str(raw)+'.attr');planes=read_attributes(attr.read_bytes(),0x71)
    # Whole arrays differ in unrelated door/pot state. Verify every body cell
    # throughout each actual stair-handler traversal, including start/end.
    accepted=[];comparisons=[]
    for ref,trace in traces:
        for e in stair_edges_from_trace(trace,0x71):
            begin=next(i for i,v in enumerate(trace) if [v['plane'],v['x'],v['y']]==e['from'] and i+1<len(trace) and trace[i+1]['submodule'] in (8,16))
            end=begin+e['frames'];samples=set()
            for v in trace[begin:end+1]:
                for y in range((v['y']+8)//8,(v['y']+23)//8+1):
                    for x in range(v['x']//8,(v['x']+15)//8+1):samples.add((v['plane'],x,y))
            mismatches=[(p,x,y) for p,x,y in samples if ref[p*4096+y*64+x]!=planes[p][y*64+x]]
            comparisons.append(dict(edge=e,body_attribute_samples=len(samples),mismatches=mismatches))
            if not mismatches:accepted.append(e)
    graph,positions=doorway_graph(planes,(s['plane'],*(v&511 for v in s['link_xy'])),accepted)
    state=dict(room_id=0x71,link_xy=s['link_xy'],plane=s['plane'],collision_mode=0,rom_sha256=s['rom_sha256'],source_capture=str(capture))
    graphdata=dict(format='alttp-doorway-graph-review-v1',room_id=0x71,state=state,graph=graph,elevation={},attribute_sha256=hashlib.sha256(attr.read_bytes()).hexdigest(),planes=[dict(plane=str(i),reachable_foot_positions=len(v),reachable_body_cells=body_cells(v,planes[i])) for i,v in enumerate(positions)])
    (out/'state.json').write_text(json.dumps(state,indent=2));(out/'graph.json').write_text(json.dumps(graphdata,indent=2))
    solution=json.loads((out/'live-runtime.json').read_text());bases=[[255]*4096 for _ in range(2)];visible=[255]*4096
    for p,x,y,b in solution['cells_plane_x_y_base']:
        i=y*64+x;bases[p][i]=b;visible[i]=b if visible[i]==255 else max(visible[i],b)
    for r in solution['structural_placements']:visible[r['y']*64+r['x']]=r['floor_base']
    (out/'map.bin').write_bytes(b'ALTPHM3\0'+struct.pack('<H',0x71)+struct.pack('<12288i',*(visible+bases[0]+bases[1])))
    cmd=[sys.executable,'-B','remaster/tools/ALttP/alttp_validate_room_heights.py','--graph',str(out/'graph.json'),'--snapshot',str(raw),'--attributes',str(attr),'--state',str(out/'state.json'),'--profile',str(out/'replay-input-profile.toml'),'--runtime-solution',str(out/'live-runtime.json'),'--height-map',str(out/'map.bin'),'--rom',str(a.rom),'--output',str(out/'validation-v2.json'),'--diagnostic-svg',str(out/'validation-v2.svg')]
    result=subprocess.run(cmd,capture_output=True,text=True)
    record=dict(capture=str(capture),command=cmd,exit=result.returncode,error=result.stderr,trace_comparisons=comparisons,accepted_stair_edges=len(accepted),planes=[len(v['reachable_body_cells']) for v in graphdata['planes']],configured_fallbacks=solution['configured_stair_fallbacks'],authored_relations=solution['other_bg_authored_relations'],approval=False,remaining_graph_gap='Internal subtile door edges and other disconnected entries not enumerated; reference motion is not Snes9x parity.')
    (out/'gate.json').write_text(json.dumps(record,indent=2));records.append(record)
(a.output/'summary.json').write_text(json.dumps(records,indent=2));print(json.dumps(records,indent=2))
