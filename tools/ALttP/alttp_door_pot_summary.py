import json
from pathlib import Path
import sys

p=Path(sys.argv[1]);e=json.loads((p/'evidence016.json').read_text());v=json.loads((p/'validation-v2.json').read_text())
print('pins',json.dumps(e['pins'],indent=2))
print('writers',json.dumps([r for r in e.get('final_writer_evidence',[]) if r['xy'] in ([30,4],[30,6],[40,43],[14,61])],indent=2))
print('gate',json.dumps({k:v[k] for k in ('room_id','required_walkable_placements','valid_walkable_placements','required_fallback_placements','valid_fallback_placements','plane_overlap_cells','solved_wall_cells','unknown_wall_cells','success')},indent=2))
print('conflicts',len(v.get('conflicts',[])),'compiled missing',len(v.get('compiled_map_missing',[])),
      'compiled mismatches',len(v.get('compiled_map_mismatches',[])))
print('frames',{k:x for k,x in v.get('doorframes',{}).items() if k!='issues'},'issues',len(v.get('doorframes',{}).get('issues',[])))
print('directional',v.get('directional_height_continuity',{}).get('status_counts'))
