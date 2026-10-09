"""Research-audit every available complete offline state, without room approval."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sqlite3
from alttp_room_geometry import RoomSnapshot, RoomProvenance
from alttp_diagonal_sets import inspect


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--corpus',type=Path,required=True)
    p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--solver',type=Path,required=True)
    p.add_argument('--rom',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--atlas',type=Path,required=True)
    a=p.parse_args()
    if not a.output.is_dir():p.error('output parent must exist')
    profile=a.profile.read_bytes();pinned=a.output/'corpus-profile.toml';pinned.write_bytes(profile)
    with sqlite3.connect(a.atlas) as db:
        objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    byroom={}
    for obj in objects:byroom.setdefault(obj['room_id'],{})[obj['layer'],obj['ordinal']]=obj
    templates=a.rom.read_bytes()[0x1b52:0x1b52+12876]
    records=[]
    for raw in sorted(a.corpus.glob('room-*-entrance-*.raw')):
        state_path=Path(str(raw)+'.state.json');attr=Path(str(raw)+'.attr')
        r=dict(snapshot=str(raw),approval=False)
        if not state_path.exists() or not attr.exists():
            r['status']='unsupported_missing_complete_state';records.append(r);continue
        state=json.loads(state_path.read_text())
        cmd=[str(a.solver),str(raw),str(attr),str(pinned),*map(str,state['link_xy']),str(state['plane']),
             '--rom',str(a.rom)]
        result=subprocess.run(cmd,capture_output=True,text=True)
        if result.returncode:
            r.update(status='unsupported_solver',error=result.stderr)
        else:
            d=json.loads(result.stdout)
            previews=[x for x in d['directional_geometry'] if x.get('captured_local_source')=='placement_rom_diagonal_preview']
            snapshot=RoomSnapshot.read(raw)
            provenance=RoomProvenance.read(Path(str(raw)+'.prov'),snapshot)
            assemblies,claims=inspect(snapshot,provenance,byroom.get(snapshot.room,{}),templates)
            attestations=[]
            for x in previews:
                key=(x['layer'],x['x'],x['y']);owned=claims.get(key,set())
                owner=provenance.owner(*key)
                attestations.append(dict(layer=key[0],plane=x['plane'],xy=list(key[1:]),writer=owner,
                    status='attested_final_dispatch' if len(owned)==1 else 'unproved_final_dispatch',
                    claims=[list(v) for v in owned],
                    writer_record=provenance.objects[owner-1] if 0<owner<=len(provenance.objects) else None))
            r.update(status='audited',room=d['room_id'],preview_plane_placements=len(previews),
                     extended_plane_placements=sum(x.get('preview_maximum',0)-x.get('preview_minimum',0)>24 for x in previews),
                     solved_walls=d['solved_wall_cells'],unknown_walls=d['unknown_wall_cells'],
                     structural_conflicts=d['structural_conflicts'],required_planes=d['required_planes'],
                     missing_planes=d['missing_planes'],snapshot_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),
                     attributes_sha256=hashlib.sha256(attr.read_bytes()).hexdigest())
            r['final_writer_attestations']=attestations
            r['attested_preview_planes']=sum(v['status']=='attested_final_dispatch' for v in attestations)
            r['unproved_preview_planes']=len(attestations)-r['attested_preview_planes']
            if snapshot.room==0x61:
                r['diagonal_dispatches']=assemblies
                r['ceiling_contacts']=[]
                placed_records={(x['layer'],x['plane'],x['x'],x['y']):x for x in d['directional_geometry']}
                for layer in range(2):
                    for cell in snapshot.cells[layer]:
                        if cell.word!=0x3c15:continue
                        # Actual ceiling template identity and final writer are
                        # recorded, not proximity used as a height anchor.
                        owner=provenance.owner(layer,cell.x,cell.y)
                        if not any(0<=cell.x+dx<64 and 0<=cell.y+dy<64 and
                            (layer,cell.x+dx,cell.y+dy) in claims for dx,dy in ((1,0),(-1,0),(0,1),(0,-1))):continue
                        source=provenance.objects[owner-1] if 0<owner<=len(provenance.objects) else None
                        obj=byroom.get(snapshot.room,{}).get((source['phase'],source['ordinal'])) if source else None
                        r['ceiling_contacts'].append(dict(layer=layer,xy=[cell.x,cell.y],hash=cell.tile_hash,writer=owner,
                            writer_record=provenance.objects[owner-1] if 0<owner<=len(provenance.objects) else None,
                            writer_object=obj,
                            planes=[placed_records[layer,p,cell.x,cell.y] for p in range(2)]))
        records.append(r)
    report=dict(profile_sha256=hashlib.sha256(profile).hexdigest(),records=records,
                attempted_room_denominator=320,available_states=len(records),
                audited_states=sum(r['status']=='audited' for r in records),
                audited_rooms=len({r['room'] for r in records if r['status']=='audited'}),
                statuses=dict(Counter(r['status'] for r in records)),
                approval=False,unsupported_scope=['room017 unsettled','nondefault state enumeration',
                'complete doorway graphs outside055/060/061','matching live gameplay outside supplied packets'])
    report['attested_preview_planes']=sum(r.get('attested_preview_planes',0) for r in records)
    report['unproved_preview_planes']=sum(r.get('unproved_preview_planes',0) for r in records)
    (a.output/'corpus.json').write_text(json.dumps(report,indent=2)+'\n')
    print({k:v for k,v in report.items() if k!='records'})


if __name__=='__main__':main()
