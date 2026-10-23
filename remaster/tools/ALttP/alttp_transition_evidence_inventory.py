"""Inventory actual capture-sidecar transition evidence, never infer a trace."""
import argparse
import json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('captures',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
records=[]
for path in sorted(a.captures.glob('frame_room*.s9xrmf.dungeon.json')):
    r=json.loads(path.read_text());records.append(dict(capture=str(path).removesuffix('.dungeon.json'),room=r['room'],submodule=r['submodule'],phase=r.get('construction_phase'),previous=r.get('previous_room'),complete=all(Path(str(path).removesuffix('.dungeon.json')+s).exists() for s in ('.dungeon-input.bin','.profile.toml','.dungeon-templates.bin'))))
result=dict(sidecars=len(records),complete=sum(r['complete'] for r in records),supertile_transition=sum(r['submodule']==2 for r in records),records=records)
a.output.write_text(json.dumps(result,indent=2)+'\n');print({k:v for k,v in result.items() if k!='records'})
