"""Complete available default-context independent dispatch/semantic proof audit."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import tomllib
from alttp_room_geometry import RoomSnapshot, RoomProvenance
from alttp_diagonal_sets import inspect
from alttp_diagonal_preview import diagonal_previews

p=argparse.ArgumentParser()
for name in ('corpus','profile','rom','atlas','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
rom=a.rom.read_bytes();profile_bytes=a.profile.read_bytes()
assert hashlib.sha256(rom).hexdigest()=='66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'
profile=tomllib.loads(profile_bytes.decode());template=rom[0x1b52:0x1b52+12876]
with sqlite3.connect(f'file:{a.atlas}?mode=ro',uri=True) as db:
    assert db.execute("SELECT value FROM run_info WHERE key='rom_sha256'").fetchone()[0]==hashlib.sha256(rom).hexdigest()
    objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
records=[]
for raw in sorted(a.corpus.glob('room-*-entrance-*.raw')):
    s=RoomSnapshot.read(raw);pr=RoomProvenance.read(Path(str(raw)+'.prov'),s)
    obj={(o['layer'],o['ordinal']):o for o in objects if o['room_id']==s.room}
    assemblies,claims=inspect(s,pr,obj,template);owned={}
    for assembly in assemblies:
        for v in assembly['placements']:
            key=v['layer'],v['x'],v['y'];claim=claims.get(key,set())
            if len(claim)==1:owned.setdefault(key,set()).add((*next(iter(claim)),assembly['writer'],assembly['handler'],assembly['template_offset']))
    partial=[];proved=0
    for layer in range(2):
        result=diagonal_previews(s,profile,template,layer,owned)
        proved+=len(result)
        partial.extend(dict(layer=layer,x=x,y=y,**v) for (x,y),v in sorted(result.items())
                       if v.get('evidence',{}).get('source')=='partial_semantic_diagonal_owned_tangent')
    records.append(dict(snapshot=str(raw),room=s.room,proved=proved,partial=partial))
report=dict(contexts=len(records),rooms=len({r['room'] for r in records}),profile_sha256=hashlib.sha256(profile_bytes).hexdigest(),
            contexts_with_partial=sum(bool(r['partial']) for r in records),partial_placements=sum(len(r['partial']) for r in records),
            unsupported='nondefault dynamic states, unobserved room IDs and absent/ambiguous final writers not approved',records=records)
a.output.write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='records'})
