"""Exact changed-asset and protected-field summary against a saved profile."""
import argparse
import json
from pathlib import Path
import tomllib
from alttp_torch_animation_metadata import invariants
p=argparse.ArgumentParser();p.add_argument('before',type=Path);p.add_argument('after',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
b=tomllib.loads(a.before.read_text());c=tomllib.loads(a.after.read_text());invariants(b,c)
bb={x['tile_hash']:x for x in b['assets']};cc={x['tile_hash']:x for x in c['assets']}
changes=[dict(tile_hash=tile,fields=[k for k in cc[tile] if bb.get(tile,{}).get(k)!=cc[tile][k]],new=tile not in bb) for tile in cc if bb.get(tile)!=cc[tile]]
r=dict(changed_assets=len(changes),new_assets=sum(x['new'] for x in changes),height_arrays_changed=sum('height' in x['fields'] for x in changes),protected_fields_unchanged=True,changes=changes)
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
