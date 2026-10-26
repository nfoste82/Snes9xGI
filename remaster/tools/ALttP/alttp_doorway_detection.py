#!/usr/bin/env python3
"""Detection only: independent ROM/writer doorway contracts vs terminal heights.

Never writes a profile, height map, or solver result. Expectations are conditional
on exact surviving template words, one final door writer and same-layer explicit
landing contacts. Unknown ownership/contacts are defects, not guessed heights.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import subprocess
import tomllib

from alttp_doorway_metadata import migrate_profile
from alttp_room_geometry import RoomSnapshot, RoomProvenance

ROM_SHA = '66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'


def displayed(values, word):
    return [values[(7-y if word & 0x8000 else y)*8 +
                   (7-x if word & 0x4000 else x)] for y in range(8) for x in range(8)]


def candidates(snapshot, provenance, templates):
    """All twelve final words, not the solver's assembly/door_template fields.

    Equivalent template descriptions collapse; competing ownership does not.
    A partial overwrite fails closed. Priority-only differences are allowed,
    but the final writer must still be the same actual door dispatch.
    """
    by_writer = defaultdict(set)
    for layer in range(2):
        for y in range(64):
            for x in range(64):
                writer = provenance.owners[layer][y*64+x]
                if 1 <= writer <= len(provenance.objects) and provenance.objects[writer-1]['is_door']:
                    by_writer[writer].add((layer, x, y))
    found = {}
    ranges = ((0x2716,0x2926),(0x29f0,0x2be8),(0x2c6a,0x2e32),(0x2e6a,0x3032))
    for writer, cells in sorted(by_writer.items()):
        for d, (start,end) in enumerate(ranges):
            w,h = (4,3) if d < 2 else (3,4)
            for source in range(start,end+1,24):
                for flip in range(4):
                    direction = d ^ bool(flip & (2 if d < 2 else 1))
                    for split, bg in ((False,0),(False,1),(True,0)):
                        parts = []
                        for x in range(w):
                            for y in range(h):
                                outer = (y == (0 if d==0 else h-1)) if d<2 else (x == (0 if d==2 else w-1))
                                layer = int(outer) if split else bg
                                n = source+2*(x*h+y)
                                word = int.from_bytes(templates[n:n+2],'little') ^ (0x4000 if flip&1 else 0) ^ (0x8000 if flip&2 else 0)
                                parts.append((layer,w-1-x if flip&1 else x,h-1-y if flip&2 else y,word))
                        layer,px,py,word=parts[0]
                        for l,x,y in sorted(cells):
                            if l != layer: continue
                            ox,oy=x-px,y-py
                            if not (0<=ox<=64-w and 0<=oy<=64-h): continue
                            if any((l,ox+x,oy+y) not in cells or
                                   snapshot.cell(l,ox+x,oy+y).word & 0xdfff != v & 0xdfff
                                   for l,x,y,v in parts): continue
                            keys=tuple(sorted((l,ox+x,oy+y) for l,x,y,v in parts))
                            key=(writer,ox,oy,int(direction),keys)
                            found.setdefault(key,dict(writer=writer,origin=[ox,oy],direction=int(direction),
                                split=split,width=w,height=h,members=[list(k) for k in keys],sources=[]))['sources'].append(source)
    return list(found.values()), by_writer


def check_assembly(assembly, cells):
    """Pure contract check. cells have displayed local/actual arrays and metadata.

    The support layer is dictated by the intact split dispatch, never max-plane.
    Outside contacts are the template's exact two room-facing opening columns.
    Door is a structural role, even when a collision sample is nonblocking.
    """
    ox,oy=assembly['origin'];d=assembly['direction'];w=assembly['width'];h=assembly['height']
    support_layer=0 if assembly['split'] else assembly['members'][0][0]
    inward=((0,1),(0,-1),(1,0),(-1,0))[d]
    contacts=([(support_layer,ox+i,oy+(h if d==0 else -1)) for i in (1,2)] if d<2 else
              [(support_layer,ox+(w if d==2 else -1),oy+i) for i in (1,2)])
    records=[]
    def emit(key, rule, status, expected=None, **extra):
        c=cells.get(tuple(key),{})
        records.append(dict(layer=key[0],x=key[1],y=key[2],rule=rule,status=status,
            expected=expected,actual=c.get('actual'),source=dict(writer=assembly['writer'],
            template_sources=sorted(set(assembly['sources'])),support_layer=support_layer,
            contacts=[dict(placement=list(k),local=cells.get(k,{}).get('local'),
                actual=cells.get(k,{}).get('actual')) for k in contacts]),**extra))
    # No candidate may authorize itself through the solver's opening_base.
    datums=[]
    for key in contacts:
        c=cells.get(key)
        if not c or c['role']!='floor' or len(c.get('actual',[]))!=64 or len(c.get('local',[]))!=64:
            datums=[];break
        offsets={a-b for a,b in zip(c['actual'],c['local'])}
        if len(offsets)!=1:
            datums=[];break
        datums.extend(offsets)
    base=datums[0] if len(datums)==2 and len(set(datums))==1 else None
    for key in assembly['members']:
        l,x,y=key;c=cells.get(tuple(key),{});role=c.get('role','unknown')
        # Inner 2x2 is floor ONLY if canonical metadata says floor. Door cannot
        # be swept up by the contiguous-floor contract, regardless of collision.
        opening=(x in (ox+1,ox+2) and y!=(oy if d==0 else oy+h-1)) if d<2 else (
            y in (oy+1,oy+2) and x!=(ox if d==2 else ox+w-1))
        if opening and role=='door':
            rule='closed_door_assembly'
        elif opening and role=='floor':
            rule='open_interior_contiguous_floor'
        elif role in ('doorway','door'):
            rule='split_bg1_assembly' if assembly['split'] else 'doorway_assembly'
        else:
            emit(key,'semantic_ownership','unresolved',reason='unsupported_member_role');continue
        if base is None:
            emit(key,rule,'unresolved',reason='missing_or_conflicting_explicit_landing');continue
        local=c.get('local',[])
        if len(local)!=64 or len(c.get('actual',[]))!=64:
            emit(key,rule,'unresolved',reason='missing_local_or_terminal_samples');continue
        if rule=='open_interior_contiguous_floor':
            if any(local):
                emit(key,rule,'unresolved',reason='nonflat_opening_requires_region_contract');continue
            expected=[base]*64
        else:
            band=(oy+h-1-y if d==0 else y-oy if d==1 else ox+w-1-x if d==2 else x-ox)
            mask=c.get('mask',(1<<64)-1)
            frame=[v for k,v in enumerate(local) if mask>>k&1]
            if not frame:
                emit(key,rule,'unresolved',reason='empty_frame_mask');continue
            # The template band supplies the datum; preserve intrinsic relief.
            # Some cap arrays already encode band16. Adding band16 a second
            # time is NOT an independent expectation of a 32-unit cap datum.
            minimum=min(frame)
            # Authored assembly-height caps can include zero opening pixels
            # even inside a full metadata mask. Accept only a single 8-unit
            # ramp block plus zero cutouts, not arbitrary relief normalization.
            block=(max(frame)//8)*8
            if not all(v==0 or block<=v<=block+7 for v in frame):
                emit(key,rule,'unresolved',reason='unsupported_multiband_frame_relief');continue
            cutouts=block>0 and all(v==0 or block<=v<=block+7 for v in frame)
            if cutouts: minimum=block
            expected=[base+band*8+v-minimum if mask>>k&1 and not (cutouts and v==0) else base for k,v in enumerate(local)]
            if role=='door':
                # Closed panels retain authored relief, never floor-flattened.
                expected=[base+band*8+v for v in local]
            if assembly['split'] and l==1:
                counterpart=cells.get((0,x,y))
                if counterpart and counterpart['role'] in ('wall','wall_corner') and len(counterpart.get('actual',[]))==64:
                    # BG1 is the owning assembly. Its adjusted counterpart is
                    # stronger evidence than this BG2 solve's selected plane.
                    active=[k for k in range(64) if mask>>k&1 and not (cutouts and local[k]==0)]
                    for k in active: expected[k]=counterpart['actual'][k]
        mismatch=[k for k,(a,b) in enumerate(zip(c['actual'],expected)) if a!=b]
        counterpart=cells.get((0,x,y)) if assembly['split'] and l==1 else None
        emit(key,rule,'fail' if mismatch else 'pass',expected,mismatch_pixels=mismatch,
             support_datum=base,expectation_kind='independent_template_band_and_external_terminal_floor',
             canonical_local=local,metadata_role=role,local_source='pinned_profile_authored_height_displayed',
             bg1_counterpart=dict(placement=[0,x,y],actual=counterpart['actual'])
                 if counterpart and counterpart.get('role') in ('wall','wall_corner') else None)
    return records


def audit(snapshot, provenance, profile, templates, terminal):
    if len(terminal)!=8192 or len({(t['bg'],t['x'],t['y']) for t in terminal})!=8192:
        raise ValueError('terminal inventory must contain both complete BG maps (8192 unique placements)')
    assets={a['tile_hash']:a for a in profile['assets']};cells={}
    for t in terminal:
        l,x,y=t['bg']-1,t['x'],t['y'];c=snapshot.cell(l,x,y);a=assets.get(c.tile_hash,{})
        if int(t['hash'],16)!=int(c.tile_hash.split(':')[-1],16):
            raise ValueError('terminal tile identity does not match pinned snapshot')
        local=a.get('height',[])
        if a.get('geometry_by_flip') or a.get('geometry_by_facing'): local=[]
        cells[l,x,y]=dict(role=a.get('tile_role','unknown'),local=displayed(local,c.word) if len(local)==64 else [],
            actual=displayed(t['actual'],c.word),mask=sum(1<<k for k,v in enumerate(displayed(
                [(int(a.get('doorframe_mask','ffffffffffffffff'),16)>>k)&1 for k in range(64)],c.word)) if v))
    assemblies,writers=candidates(snapshot,provenance,templates)
    assemblies=[a for a in assemblies if any(cells[tuple(k)]['role'] in ('doorway','door') for k in a['members'])]
    ownership=defaultdict(set)
    for n,a in enumerate(assemblies):
        for key in a['members']: ownership[tuple(key)].add(n)
    records=[];accepted=[]
    for n,a in enumerate(assemblies):
        if any(len(ownership[tuple(k)])!=1 for k in a['members']):
            records.extend(dict(layer=k[0],x=k[1],y=k[2],rule='assembly_ownership',status='unresolved',
                reason='competing_intact_templates',expected=None,actual=cells[tuple(k)]['actual']) for k in a['members'])
        else:
            records.extend(check_assembly(a,cells));accepted.append(a)
    covered={tuple(k) for a in accepted for k in a['members']}
    unsupported=[]
    for writer,keys in sorted(writers.items()):
        for k in sorted(keys-covered):
            unsupported.append(dict(layer=k[0],x=k[1],y=k[2],writer=writer,
                reason='no_unique_complete_template_ownership',role=cells[k]['role']))
    return dict(format='alttp-doorway-detection-v1',room_id=snapshot.room,
        verdict='reviewable but incomplete',publishable=False,mode='detection_only',
        configured_floor_rise=profile.get('lighting_space',{}).get('upper_floor_height',50),
        denominators=dict(complete_bg_placements=8192,door_writers=len(writers),
            final_door_writer_cells=sum(map(len,writers.values())),intact_assemblies=len(accepted),
            checked_members=len(records),unsupported_writer_cells=len(unsupported)),
        status_counts=dict(Counter(r['status'] for r in records)),
        failure_groups=dict(Counter(r['rule'] for r in records if r['status']=='fail')),
        unresolved_groups=dict(Counter(r.get('reason') for r in records if r['status']=='unresolved')),
        assemblies=[dict(a,writer_record=provenance.objects[a['writer']-1]) for a in assemblies],
        placements=records,unsupported=unsupported,
        limitations=['External terminal floor datums are conditional evidence, not absolute anchors.',
            'Final writer proof cannot recover same-value overwrites or dynamic dispatch history.',
            'Unsupported partial templates, detailed closed-door panels and other states remain explicit.',
            'No gameplay comparison or runtime/profile mutation; not a room-wide validation replacement.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('snapshot','attributes','state','profile','rom','consumer','output'):
        p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args()
    try:
        rom=args.rom.read_bytes()
        if hashlib.sha256(rom).hexdigest()!=ROM_SHA: raise ValueError('unsupported ROM fingerprint')
        s=RoomSnapshot.read(args.snapshot);state=json.loads(args.state.read_text())
        if state['room_id']!=s.room or state['collision_mode']!=0: raise ValueError('unsupported/mismatched state')
        attr=args.attributes.read_bytes()
        if len(attr)!=8202 or attr[:8]!=b'ALTPAT1\0' or int.from_bytes(attr[8:10],'little')!=s.room:
            raise ValueError('invalid/mismatched complete collision arrays')
        provenance_path=Path(str(args.snapshot)+'.prov')
        provenance=RoomProvenance.read(provenance_path,s)
        profile=migrate_profile(tomllib.loads(args.profile.read_text()))
        command=[str(args.consumer),str(args.snapshot),str(args.attributes),str(args.profile),
                 *map(str,state['link_xy']),str(state['plane'])]
        for stair in state['stairs']:
            command += [*map(str,stair['tile_xy']),str(int(stair['high_side']=='north')),str(int(state['stair_kind']==1))]
        command+=['--rom',str(args.rom)]
        terminal=[json.loads(line) for line in subprocess.check_output(command,text=True).splitlines()]
        result=audit(s,provenance,profile,rom[0x1b52:0x1b52+12876],terminal)
        result['state']=state;result['consumer_command']=command
        result['inputs']={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in
            (args.snapshot,args.attributes,args.state,args.profile,args.rom,args.consumer,provenance_path)}
        args.output.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({k:result[k] for k in ('room_id','denominators','status_counts','failure_groups','unresolved_groups')}))
        return int(bool(result['status_counts'].get('fail') or result['status_counts'].get('unresolved') or result['unsupported']))
    except (ValueError,KeyError,OSError,subprocess.CalledProcessError) as e:
        p.exit(2,str(e)+'\n')


if __name__=='__main__':
    raise SystemExit(main())
