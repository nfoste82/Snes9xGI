#!/usr/bin/env python3
"""Read-only terminal contracts beyond reachable support: diagonals and stairs.

Checks every independently proved pixel, INCLUDING walls called solved by the
runtime. Unproved dispatch members remain enumerated, not passed. External
terminal Floor datums are conditional observations, not absolute anchors. No
room/hash/rectangle exceptions and no authored or runtime geometry mutations.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import subprocess
import tomllib

from alttp_diagonal_preview import diagonal_previews
from alttp_diagonal_sets import inspect
from alttp_doorway_detection import ROM_SHA, displayed
from alttp_doorway_metadata import migrate_profile
from alttp_room_geometry import RoomSnapshot, RoomProvenance


def compare_pixels(actual, local, base, word):
    """Canonical shape first, then transform both masks and samples to placement."""
    expected=displayed([base+v for v in local],word)
    actual=displayed(actual,word)
    mismatch=[k for k,(a,b) in enumerate(zip(actual,expected)) if a!=b]
    return dict(status='fail' if mismatch else 'pass',expected=expected,actual=actual,
                mismatch_pixels=mismatch,mismatch_mask=f'{sum(1<<k for k in mismatch):016x}')


def external_datum(cells, keys):
    """Never select a nearby/max-plane floor or normalize relief to zero."""
    datums=[]
    for key in keys:
        c=cells.get(key,{})
        if c.get('role')!='floor' or len(c.get('local',[]))!=64 or len(c.get('actual',[]))!=64:
            return None
        ds={a-b for a,b in zip(c['actual'],c['local'])}
        if len(ds)!=1:return None
        datums.extend(ds)
    return datums[0] if datums and len(set(datums))==1 else None


def audit(snapshot, provenance, profile, templates, objects, terminal):
    assets={a['tile_hash']:a for a in profile['assets']}
    if len(terminal)!=8192 or len({(t['bg'],t['x'],t['y']) for t in terminal})!=8192:
        raise ValueError('requires 8192 unique terminal placements')
    cells={}
    for t in terminal:
        key=(t['bg']-1,t['x'],t['y']);c=snapshot.cell(*key);a=assets.get(c.tile_hash,{})
        if int(t['hash'],16)!=int(c.tile_hash.split(':')[-1],16) or len(t['actual'])!=64:
            raise ValueError('terminal identity/samples mismatch')
        local=a.get('height',[])
        if a.get('geometry_by_flip') or a.get('geometry_by_facing'):local=[]
        cells[key]=dict(role=a.get('tile_role','unknown'),local=displayed(local,c.word) if len(local)==64 else [],
                        actual=displayed(t['actual'],c.word),canonical_actual=t['actual'],asset=a)
    assemblies,owned=inspect(snapshot,provenance,objects,templates)
    records=[]
    for layer in range(2):
        proofs=diagonal_previews(snapshot,profile,templates,layer,owned=owned)
        # Connected oriented proof shells own their zero-foot contacts. Do not
        # borrow BG1 under BG2 or join incompatible transformed geometries.
        unseen=set(proofs);supports={}
        while unseen:
            start=min(unseen);unseen.remove(start);queue=[start];contacts=set()
            for x,y in queue:
                c=snapshot.cell(layer,x,y);p=proofs[x,y];h=displayed(p['height'],c.word)
                for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                    nx,ny=x+dx,y+dy
                    if not(0<=nx<64 and 0<=ny<64):continue
                    if (nx,ny) in proofs:
                        n=snapshot.cell(layer,nx,ny);np=proofs[nx,ny]
                        if (n.hflip,n.vflip,np['minimum'],np['maximum'])==(c.hflip,c.vflip,p['minimum'],p['maximum']) and (nx,ny) in unseen:
                            unseen.remove((nx,ny));queue.append((nx,ny))
                    else:
                        edge=[k*8+(7 if dx>0 else 0) for k in range(8)] if dx else list(range(56,64) if dy>0 else range(8))
                        if any(h[k]==0 for k in edge) and cells[layer,nx,ny]['role']=='floor':
                            contacts.add((layer,nx,ny))
            base=external_datum(cells,contacts)
            for xy in queue:supports[xy]=(base,sorted(contacts))
        for key,claims in sorted(owned.items()):
            l,x,y=key
            if l!=layer:continue
            c=snapshot.cell(l,x,y);p=proofs.get((x,y));base,contacts=supports.get((x,y),(None,[]))
            r=dict(rule='diagonal_partial_pixel_geometry',layer=l,x=x,y=y,word=c.word,
                   writer=provenance.owner(l,x,y),claims=[list(v) for v in sorted(claims)],
                   role=cells[key]['role'],support_contacts=[list(k) for k in contacts],support_datum=base,
                   actual=cells[key]['actual'],authored_local=cells[key]['local'],expected=None)
            if len(claims)!=1:r.update(status='unresolved',reason='ambiguous_dispatch_ownership')
            elif cells[key]['role']!='wall':r.update(status='excluded',reason='non_wall_dispatch_member_not_solved_geometry')
            elif p is None:r.update(status='unresolved',reason='no_independent_transformed_shape_proof')
            elif base is None:r.update(status='unresolved',reason='missing_or_conflicting_same_layer_zero_foot_contacts')
            else:
                r.update(compare_pixels(cells[key]['canonical_actual'],p['height'],base,c.word))
                r['shape_proof']=p
                r['displayed_shape']=displayed(p['height'],c.word)
                r['sloping_mask']=f'{sum(1<<k for k,v in enumerate(r["displayed_shape"]) if p["minimum"]<v<p["maximum"]):016x}'
                r['expectation_kind']='independent_owned_shape_plus_conditional_external_floor_datum'
            records.append(r)
    # Straight inter-room Nx4 handlers. The source-table offsets are ROM
    # handler contracts, not asset IDs or room-specific repair lists.
    offsets={0x1e:0x11a8,0x1f:0x11c8,0x20:0x11e8,0x21:0x1208}
    stairs=[]
    for writer,source in enumerate(provenance.objects,1):
        obj=objects.get((source['phase'],source['ordinal']))
        if source['is_door'] or not obj or obj['subtype']!=1 or obj['object_id'] not in offsets:continue
        ox,oy=obj['x'],obj['y'];handler=obj['object_id'];off=offsets[handler]
        for l in range(2):
            members=[(l,ox+x,oy+y) for x in range(4) for y in range(4)]
            if not any(0<=x<64 and 0<=y<64 and provenance.owner(l,x,y)==writer for _,x,y in members):continue
            intact=all(0<=x<64 and 0<=y<64 and provenance.owner(l,x,y)==writer and
                snapshot.cell(l,x,y).word==struct.unpack_from('<H',templates,off+2*((x-ox)*4+y-oy))[0] for _,x,y in members)
            contacts=[(l,ox+x,oy+4 if handler<0x20 else oy-1) for x in (1,2)]
            base=external_datum(cells,contacts) if intact else None
            stairs.append(dict(writer=writer,layer=l,handler=handler,origin=[ox,oy],intact=intact))
            for key in members:
                _,x,y=key
                if not(0<=x<64 and 0<=y<64):continue
                r=dict(rule='interroom_stair_floor_datum',layer=l,x=x,y=y,writer=writer,
                       handler=handler,template_offset=off,support_contacts=[list(k) for k in contacts],support_datum=base,
                       actual=cells[key]['actual'],authored_local=cells[key]['local'],expected=None)
                if not intact:r.update(status='unresolved',reason='partial_or_overwritten_stair_dispatch')
                elif cells[key]['role']!='floor':r.update(status='excluded',reason='non_floor_stair_member_requires_region_contract')
                elif base is None:r.update(status='unresolved',reason='missing_or_conflicting_exact_stair_landing')
                elif len(cells[key]['local'])!=64:r.update(status='unresolved',reason='missing_or_selected_authored_geometry')
                else:
                    r.update(compare_pixels(cells[key]['canonical_actual'],assets[snapshot.cell(*key).tile_hash]['height'],base,snapshot.cell(*key).word))
                    r['expectation_kind']='intact_rom_stair_intrinsic_relief_plus_external_landing'
                records.append(r)
    return dict(format='alttp-structure-detection-v1',room_id=snapshot.room,mode='detection_only',publishable=False,
                verdict='reviewable but incomplete',complete_bg_placements=8192,
                diagonal_dispatch_assemblies=assemblies,straight_stair_assemblies=stairs,placements=records,
                status_counts=dict(Counter(r['status'] for r in records)),
                by_rule={rule:dict(Counter(r['status'] for r in records if r['rule']==rule)) for rule in sorted({r['rule'] for r in records})},
                limitations=['Conditional terminal contacts are not absolute height anchors.',
                    'No cross-plane ownership inferred. Hidden BG geometry is not a visible defect.',
                    'Unproved shape/ownership and non-floor stair members remain explicit.',
                    'No gameplay parity, all-state coverage, or replacement for complete-room v2.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('snapshot','attributes','state','profile','rom','atlas','consumer','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();rom=a.rom.read_bytes()
    if hashlib.sha256(rom).hexdigest()!=ROM_SHA:p.error('unsupported ROM')
    s=RoomSnapshot.read(a.snapshot);state=json.loads(a.state.read_text())
    if state['room_id']!=s.room or state['collision_mode']!=0:p.error('unsupported/mismatched state')
    attr=a.attributes.read_bytes()
    if len(attr)!=8202 or attr[:8]!=b'ALTPAT1\0' or int.from_bytes(attr[8:10],'little')!=s.room:p.error('invalid attributes')
    prov=Path(str(a.snapshot)+'.prov');pr=RoomProvenance.read(prov,s)
    profile=migrate_profile(tomllib.loads(a.profile.read_text()))
    with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro',uri=True) as db:
        if db.execute("SELECT value FROM run_info WHERE key='rom_sha256'").fetchone()[0]!=ROM_SHA:p.error('atlas ROM mismatch')
        objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    objects={(o['layer'],o['ordinal']):o for o in objects if o['room_id']==s.room}
    cmd=[str(a.consumer),str(a.snapshot),str(a.attributes),str(a.profile),*map(str,state['link_xy']),str(state['plane'])]
    for stair in state['stairs']:cmd += [*map(str,stair['tile_xy']),str(int(stair['high_side']=='north')),str(int(state['stair_kind']==1))]
    cmd+=['--rom',str(a.rom)]
    terminal=[json.loads(line) for line in subprocess.check_output(cmd,text=True).splitlines()]
    result=audit(s,pr,profile,rom[0x1b52:0x1b52+12876],objects,terminal)
    result['state']=state;result['consumer_command']=cmd
    result['inputs']={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in
                      (a.snapshot,a.attributes,a.state,a.profile,a.rom,a.atlas,a.consumer,prov,Path(__file__))}
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(room=s.room,by_rule=result['by_rule'])))
    return int(any(result['status_counts'].get(k,0) for k in ('fail','unresolved')))


if __name__=='__main__':raise SystemExit(main())
