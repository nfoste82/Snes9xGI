#!/usr/bin/env python3
"""Independent semantic-universe diagonal gaps, never filtered by discovery.

Read-only source audit. JSON/Markdown review artifacts enumerate every canonical
wall with south_east/southeast direction, including loaded-only/unobserved art.
Array presence is intrinsic availability, NOT effective placement approval.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sqlite3
import tomllib
import difflib

from alttp_room_geometry import RoomSnapshot, RoomProvenance
from alttp_diagonal_sets import eligibility, REFERENCES, transfer, ROLES, proposal_patch


def conversion_equivalent(target, source):
    """Only exact ROM source slot + Do3To4Low/High conversion, no art search."""
    slots=lambda e:{(s['source_kind'],s['pack_index'],s['tile_index'],s['snes_address']) for s in e['sources']}
    if len(slots(target))!=1 or slots(target)!=slots(source):return False
    low,high=target.get('decoded_index_rows'),source.get('decoded_index_rows')
    if low is None or high is None:return False
    if len(low)!=8 or len(high)!=8 or any(len(row)!=8 for row in low+high):return False
    tc={s['conversion'] for s in target['sources']};sc={s['conversion'] for s in source['sources']}
    if tc=={'high'} and sc=={'low'}:low,high=high,low
    elif tc!={'low'} or sc!={'high'}:return False
    return all(0<=a<=7 and b==(a|8 if a else 0) for ar,br in zip(low,high) for a,b in zip(ar,br))


def diagonal_universe(semantics):
    return {key for key,value in semantics.items() if value.get('role')=='wall'
            and any(str(n).lower().replace('_','').replace('-','')=='southeast'
                    for n in value.get('normals',[]))}


def intrinsic_status(asset):
    missing=[]
    for field,size in (('height',64),('normal_xyz',192)):
        value=asset.get(field)
        if not isinstance(value,list) or len(value)!=size or any(type(v)!=int or not 0<=v<=255 for v in value):
            missing.append(field)
    occlusion=asset.get('occlusion')
    mode=('default_full255' if occlusion is None else 'explicit_full255'
          if occlusion==[255]*64 else 'explicit_painted'
          if isinstance(occlusion,list) and len(occlusion)==64 else 'invalid')
    if mode=='invalid':missing.append('occlusion_invalid')
    return dict(gap=bool(missing),missing=missing,occlusion_behavior=mode,
                profile_asset_present=bool(asset),profile_role=asset.get('tile_role'),
                profile_semantics_authored=asset.get('tile_semantics_authored',False),
                profile_semantic_normals=asset.get('tile_semantic_normals',[]),
                height=asset.get('height'),normal_xyz=asset.get('normal_xyz'),occlusion=occlusion)


def summarize(entries):
    return dict(semantic_denominator=len(entries),intrinsic_complete=sum(not e['gap'] for e in entries),
                intrinsic_gaps=sum(e['gap'] for e in entries),
                missing_height=sum('height' in e['missing'] for e in entries),
                missing_normal=sum('normal_xyz' in e['missing'] for e in entries),
                occlusion_behaviors=dict(Counter(e['occlusion_behavior'] for e in entries)),
                explicit_profile_wall_role=sum(e['profile_role']=='wall' for e in entries),
                no_explicit_profile_wall_role=sum(e['profile_role']!='wall' for e in entries),
                no_explicit_profile_semantic_flag=sum(not e['profile_semantics_authored'] for e in entries),
                gi_arrays_require_explicit_role=False)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--semantics',type=Path,required=True)
    p.add_argument('--discovery',type=Path,required=True)
    p.add_argument('--corpus',type=Path,required=True)
    p.add_argument('--atlas',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--propose',action='store_true')
    p.add_argument('--verify-plan',type=Path,help='verify applied before-gap JSON updates and byte preservation')
    p.add_argument('--baseline',type=Path)
    a=p.parse_args()
    if not a.output.is_dir():p.error('output must exist')
    semantic=json.loads(a.semantics.read_text())
    discovery=json.loads(a.discovery.read_text())
    assert semantic['rom_sha256']==discovery['rom_sha256']
    assets={v['tile_hash']:v for v in tomllib.loads(a.profile.read_text())['assets']}
    if a.verify_plan:
        import re
        if not a.baseline:p.error('--verify-plan requires --baseline')
        before=json.loads(a.verify_plan.read_text())
        updates=before['proposed_updates']
        text=a.profile.read_text()
        for key,fields in updates.items():
            assert all(assets[key][field]==value for field,value in fields.items()),key
            assert 'occlusion' not in assets[key]
        def strip(match):
            key=re.search(r'(?m)^tile_hash\s*=\s*"([^"]+)"',match.group())
            return '' if key and key[1] in updates else match.group()
        assert re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',strip,text)==a.baseline.read_text(),'unrelated bytes changed'
        print('PASS conversion arrays and unrelated-byte preservation',len(updates))
    universe=diagonal_universe(semantic['tiles'])
    entries={key:dict(tile_hash=key,annotation=semantic['tiles'][key],**intrinsic_status(assets.get(key,{})),
                      placements=[],loaded_contexts=[],templates=[],sources=[],alternatives=[])
             for key in sorted(universe)}
    for record in discovery['loaded_templates']:
        for band in record['bands']:
            if band['tile_hash'] in entries:
                entries[band['tile_hash']]['templates'].append(dict(room=record['room'],entrance=record['entrance'],
                    handler=record['handler'],family='high' if record['handler']<=20 else 'low',**band))
    with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro',uri=True) as db:
        assert dict(db.execute('SELECT key,value FROM run_info'))['rom_sha256']==discovery['rom_sha256']
        objects=defaultdict(dict)
        for (data,) in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'"):
            obj=json.loads(data);objects[obj['room_id']][obj['layer'],obj['ordinal']]=obj
        for key,entry in entries.items():
            row=db.execute('SELECT indices FROM decoded_tile WHERE content_id=?',(key,)).fetchone()
            entry['decoded_index_rows']=[list(row[0][y*8:y*8+8]) for y in range(8)] if row else None
            sources=[dict(zip(('source_kind','pack_index','tile_index','conversion','snes_address'),row))
                     for row in db.execute('SELECT source_kind,pack_index,tile_index,conversion,snes_address FROM graphics_source WHERE content_id=?',(key,))]
            entry['sources']=sources
            siblings=set()
            for source in sources:
                siblings.update(row[0] for row in db.execute('SELECT content_id FROM graphics_source WHERE source_kind=? AND pack_index=? AND tile_index=?',
                    (source['source_kind'],source['pack_index'],source['tile_index'])))
            entry['alternatives']=[dict(tile_hash=sibling,**intrinsic_status(assets.get(sibling,{})),
                discovery=discovery['decisions'].get(sibling)) for sibling in sorted(siblings-{key})]
            entry['overworld_placements']=db.execute('SELECT COUNT(*) FROM overworld_base_tile_usage WHERE content_id=?',(key,)).fetchone()[0]
    rooms=[]
    for context in discovery['rooms']:
        if context['status']!='scanned':continue
        raw=a.corpus/f"room-{context['room']:03x}-entrance-{context['requested_entrance']}.raw"
        for suffix,digest in context['input_sha256'].items():
            assert hashlib.sha256(Path(str(raw)+suffix).read_bytes()).hexdigest()==digest
        snapshot=RoomSnapshot.read(raw);prov=RoomProvenance.read(Path(str(raw)+'.prov'),snapshot)
        rooms.append(context['room'])
        for layer in snapshot.cells:
            for cell in layer:
                if cell.tile_hash not in entries:continue
                writer=prov.owner(cell.layer,cell.x,cell.y)
                source=prov.objects[writer-1] if 0<writer<=len(prov.objects) else None
                obj=objects[context['room']].get((source['phase'],source['ordinal'])) if source else None
                entries[cell.tile_hash]['placements'].append(dict(room=context['room'],entrance=context['requested_entrance'],
                    layer=cell.layer,x=cell.x,y=cell.y,word=cell.word,hflip=cell.hflip,vflip=cell.vflip,
                    writer=writer,dispatch=source,object=obj))
        loaded=set()
        for layer in range(2):
            for slot in range(1024):
                key=snapshot._tile_hash((snapshot.layers[layer][1]+slot*16)&32767)
                if key in entries and (key,layer) not in loaded:
                    loaded.add((key,layer));entries[key]['loaded_contexts'].append(dict(room=context['room'],entrance=context['requested_entrance'],layer=layer))
    for key,entry in entries.items():
        entry['placement_rooms']=sorted({r['room'] for r in entry['placements']})
        entry['families']=sorted({r['family'] for r in entry['templates']})
        entry['known_bands']=sorted({r['band'] for r in entry['templates']})
        entry['evidence_gap']=('none_for_intrinsic_arrays' if not entry['gap'] else
            'loaded_template_no_final_placement' if entry['templates'] and not entry['placements'] else
            'other_dispatch_topology_requires_proof' if entry['placements'] else
            'unsampled_graphics_conversion_or_state_requires_proof')
        entry['status']='unresolved_gap' if entry['gap'] else 'intrinsic_available_placement_not_approved'
    summary=summarize(list(entries.values()))
    updates={}
    for key,entry in entries.items():
        proofs=[]
        for sibling in entry['alternatives']:
                source=entries.get(sibling['tile_hash'])
                decision=discovery['decisions'].get(sibling['tile_hash'],{})
                if source and not source['gap'] and conversion_equivalent(entry,source) and len(decision.get('bands',[]))==1 and not (
                        set(decision.get('reasons',[])) - {'immutable_reference'}):
                    veto=eligibility(key,{ROLES.index(decision['bands'][0])},assets,semantic['tiles'],REFERENCES,set())
                    if not veto and not entry['placements'] and not entry['templates']:
                        proofs.append(dict(source=sibling['tile_hash'],band=decision['bands'][0],family=decision['family'],
                            operation='same_ROM_pack_slot_Do3To4_palette_half_only'))
        entry['conversion_proofs']=proofs
        if len(proofs)==1 and entry['gap'] and not any(field in assets.get(key,{}) for field in ('height','normal_xyz')):
            updates[key]=transfer(assets[proofs[0]['source']])
        if proofs:
            entry['families']=sorted({proof['family'] for proof in proofs})
            entry['known_bands']=sorted({proof['band'] for proof in proofs})
            entry['evidence_gap']='conversion_intrinsic_proved_placement_unobserved'
    report=dict(format='alttp-semantic-diagonal-gaps-v1',verdict='reviewable but incomplete',approval=False,
        pins={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in (a.profile,a.semantics,a.atlas,a.discovery)},
        summary=summary,corpus=discovery['inventory'],entries=list(entries.values()),
        proposed_updates=updates,
        gap_ids=[key for key,e in entries.items() if e['gap']],
        limitations=['Every canonical wall/SE entry is counted independent of known dispatch; missing art is a gap, never excluded.',
            'Intrinsic complete means valid array availability only, not correct effective placement or verified role activation.',
            'Absent occlusion is reference-compatible full255 default, not a geometry hole or gap.',
            'No new movement, runtime or game-state enumeration.'])
    (a.output/'semantic-diagonal-gaps.json').write_text(json.dumps(report,indent=2)+'\n')
    lines=['# Exhaustive canonical southeast wall diagonal inventory','',json.dumps(summary,indent=2),'',
           '|ID (v1:4bpp omitted)|status|missing|occlusion|profile role|known family/band|placed rooms hex|evidence|',
           '|---|---|---|---|---|---|---|---|']
    for e in entries.values():
        lines.append('|'+ '|'.join((e['tile_hash'].removeprefix('v1:4bpp:'),e['status'],','.join(e['missing']) or 'none',e['occlusion_behavior'],
            str(e['profile_role']),','.join(e['families'])+'/'+','.join(e['known_bands']),
            ','.join(f'{room:03x}' for room in e['placement_rooms']) or 'none observed',e['evidence_gap']))+'|')
    (a.output/'semantic-diagonal-gaps.md').write_text('\n'.join(lines)+'\n')
    if a.propose:
        text=a.profile.read_text()
        sections=proposal_patch(text,updates)
        (a.output/'semantic-authoring.patch').write_text('*** Begin Patch\n*** Update File: remaster/profiles/ALttP/alttp-profile.toml\n'+
            ''.join(sections)+'*** End Patch\n')
        additions=''.join(line[1:]+'\n' for section in sections for line in section.splitlines() if line.startswith('+'))
        new=text.replace('[[rules]]',additions+'[[rules]]',1)
        (a.output/'semantic-authoring.diff').write_text(''.join(difflib.unified_diff(text.splitlines(True),new.splitlines(True),
            fromfile='a/remaster/profiles/ALttP/alttp-profile.toml',tofile='b/remaster/profiles/ALttP/alttp-profile.toml')))
        (a.output/'semantic-before-profile.toml').write_text(text)
        (a.output/'semantic-before-gaps.json').write_text(json.dumps(report,indent=2)+'\n')
        print('PROPOSED CONVERSION UPDATES',list(updates))
    print(json.dumps(summary,indent=2))
    for e in entries.values():
        if e['gap']:
            print(e['tile_hash'],e['evidence_gap'],'bands',e['known_bands'],'placements',len(e['placements']),
                  'loaded',len(e['loaded_contexts']),'sources',e['sources'],
                  'authored siblings',[(s['tile_hash'],s['discovery']) for s in e['alternatives'] if not s['gap']])
            print('GAP DISPATCHES',dict(Counter((r['object']['subtype'],r['object']['object_id']) if r['object'] else ('unknown',r['writer']) for r in e['placements'])))
            print('GAP ART',e['decoded_index_rows'])
            print('GAP SAMPLE',e['placements'][:2])


if __name__=='__main__':main()
