#!/usr/bin/env python3
"""ROM-proven dungeon pack5c torch slots; conservative geometry-only proposals.

DecompressAnimatedDungeonTiles interleaves sixteen-tile blocks of pack5c with
the themed pack; misc.c cycles three0x400-byte DMA phases to VRAM word3b00.
Thus pack tile slot+16*phase has identical canonical placement coordinates.
No appearance grouping, flip rewriting, emission or normal propagation.
"""
import argparse
import json
from pathlib import Path
import sqlite3
import tomllib

FIELDS=('height','height_sampling','tile_role','tile_semantics_authored',
        'tile_semantic_normals','tile_semantic_context_dependent','tile_semantic_note')
# Reviewed current-user positional seeds. The family membership comes ONLY
# from pack provenance above, not this identity selection. Slots8/10 are the
# user's new local0..7 updates; obsolete absolute-band variants are reported.
SEEDS={6:'v1:4bpp:a8d1d0722ff1bda3',7:'v1:4bpp:713a84c159e10e36',
       8:'v1:4bpp:98b436e4f1eca7a9',9:'v1:4bpp:c5487cf307fcd5b2',
       10:'v1:4bpp:657478a2462983a8',11:'v1:4bpp:0286d2de151fd423'}

def reviewed_changes(profile,atlas):
    assets={a['tile_hash']:a for a in profile['assets']};changes={}
    for slot,ids in families(atlas).items():
        seed=assets[SEEDS[slot]]
        fields={k:seed[k] for k in ('height','height_sampling')}
        fields.update(tile_role='wall',tile_semantics_authored=True,
                      tile_semantic_normals=['south' if slot in (6,8,10) else 'east'],
                      tile_semantic_context_dependent=False,tile_semantic_note='')
        for tile in ids:
            fields_here={k:v for k,v in fields.items() if assets.get(tile,{}).get(k)!=v}
            if fields_here:changes[tile]=fields_here
    return changes

def families(atlas):
    with sqlite3.connect(atlas) as db:
        assert db.execute("select value from run_info where key='rom_sha256'").fetchone()[0]=='66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'
        sources=dict(db.execute("select tile_index,content_id from graphics_source where source_kind='background' and pack_index=92 and conversion='low'"))
    return {slot:[sources[slot+16*p] for p in range(3)] for slot in range(6,12)}

def dma_phases(atlas,capture):
    """Three real pack phases with themed pack selected by current VRAM identity."""
    from alttp_wall_torch_evidence import identity
    blob=Path(str(capture)+'.dungeon-input.bin').read_bytes();assert len(blob)==90120
    state=json.loads(Path(str(capture)+'.dungeon.json').read_text())
    assert state['rom_sha256']=='66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'
    with sqlite3.connect(atlas) as db:
        candidates=[]
        for pack in (0x5d,0x5e,0x5f):
            ids=dict(db.execute("select tile_index,content_id from graphics_source where source_kind='background' and pack_index=? and conversion='low'",(pack,)))
            for phase in range(3):
                if all(identity(blob[24584:],0x7600+n*32,0)==ids[phase*16+n] for n in range(16)):
                    candidates.append((pack,phase))
        assert len(candidates)==1,('ambiguous themed animation context',candidates)
        pack,current=candidates[0];out=bytearray()
        def planar(indices):
            data=bytearray(32)
            for y in range(8):
                for x in range(8):
                    for b in range(4):data[b//2*16+y*2+b%2]|=((indices[y*8+x]>>b)&1)<<(7-x)
            return data
        for phase in range(3):
            for source in (pack,0x5c):
                for slot in range(16):
                    row=db.execute("select indices from graphics_source join decoded_tile using(content_id) where source_kind='background' and pack_index=? and conversion='low' and tile_index=?",(source,phase*16+slot)).fetchone()
                    assert row;out.extend(planar(row[0]))
    # Actual ROM load shuffle maps pack phase order as [0,1,2] themed and
    # [0,1,2]5c after interleave; prove current complete VRAM against phase.
    assert out[current*1024:(current+1)*1024]==blob[24584+0x7600:24584+0x7a00]
    return out,dict(themed_pack=pack,current_phase=current,pack5c=92,phases=3)

def propose(profile, atlas):
    assets={a['tile_hash']:a for a in profile['assets']}
    results=[]
    for slot,ids in families(atlas).items():
        members=[assets.get(i,{'tile_hash':i}) for i in ids]
        authored=[a for a in members if a.get('tile_role')=='wall' and a.get('height') is not None]
        record=dict(slot=slot,ids=ids,members=[{k:a[k] for k in FIELDS if k in a} for a in members])
        if not authored:
            record['status']='missing_authored_geometry_seed'
        else:
            source=authored[0]
            conflicts=[a['tile_hash'] for a in authored if a['height']!=source['height']]
            if conflicts:
                record.update(status='conflicting_authored_variants',conflicts=conflicts)
            else:
                changes=[]
                for a in members:
                    # Existing differing height without wall activation needs explicit
                    # review, not automatic replacement. Roles are never guessed.
                    if a.get('height') is not None and a['height']!=source['height']:
                        changes.append(dict(tile_hash=a['tile_hash'],status='review_existing_different_height'))
                    else:
                        fields={k:source[k] for k in FIELDS if k in source and a.get(k)!=source[k]}
                        if fields:changes.append(dict(tile_hash=a['tile_hash'],status='safe_geometry_only',fields=fields))
                record.update(status='proposed',source=source['tile_hash'],changes=changes)
        results.append(record)
    return results

def invariants(before,after):
    """Reject any unrelated authoring mutation, including every emission field."""
    b={a['tile_hash']:a for a in before['assets']};c={a['tile_hash']:a for a in after['assets']}
    assert b.keys()<=c.keys()
    for tile in c.keys()-b.keys():assert set(c[tile])<={'tile_hash',*FIELDS},tile
    for tile in b:
        assert {k:v for k,v in b[tile].items() if k not in FIELDS}=={k:v for k,v in c[tile].items() if k not in FIELDS},tile
    assert {k:v for k,v in before.items() if k!='assets'}=={k:v for k,v in after.items() if k!='assets'}

def corpus_inventory(atlas,captures,profile):
    from alttp_wall_torch_evidence import identity
    import struct
    assets={a['tile_hash']:a for a in profile['assets']};groups=families(atlas)
    wanted={tile:slot for slot,ids in groups.items() for tile in ids}
    records=[]
    for capture in sorted(captures.glob('frame_room*.s9xrmf')):
        prefix=str(capture);sidecar=Path(prefix+'.dungeon.json');raw=Path(prefix+'.dungeon-input.bin')
        if not sidecar.exists() or not raw.exists():records.append(dict(capture=prefix,status='unsupported_missing_complete_inputs'));continue
        state=json.loads(sidecar.read_text());blob=raw.read_bytes()
        if 'bg1_graphics_base' not in state:records.append(dict(capture=prefix,status='unsupported_graphics_context'));continue
        if len(blob)!=90120:records.append(dict(capture=prefix,status='invalid_inputs'));continue
        missing=conflicts=usages=0
        for bg in (1,0):
            base=state['graphics_base'] if bg else state['bg1_graphics_base']
            if base==65535:base=state['graphics_base']
            words=struct.unpack_from('<4096H',blob,8+(0 if bg else 8192))
            for word in words:
                tile=identity(blob[24584:],base,word)
                if tile not in wanted:continue
                usages+=1;a=assets.get(tile,{});seed=assets[SEEDS[wanted[tile]]]
                missing+=a.get('height') is None or a.get('tile_role')!='wall'
                conflicts+=a.get('height') is not None and a['height']!=seed['height']
        records.append(dict(capture=prefix,room=state['room'],status='complete_input_inventory',usages=usages,
                            missing_activation_or_height=missing,canonical_height_conflicts=conflicts))
    accepted=[r for r in records if r['status']=='complete_input_inventory']
    return dict(summary=dict(packets=len(records),complete_inputs=len(accepted),unsupported=len(records)-len(accepted),
                             usages=sum(r['usages'] for r in accepted),
                             missing=sum(r['missing_activation_or_height'] for r in accepted),
                             conflicts=sum(r['canonical_height_conflicts'] for r in accepted),
                             rooms=sorted({r['room'] for r in accepted}),approval=False),records=records)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile',type=Path,required=True);p.add_argument('--atlas',type=Path,required=True)
    p.add_argument('--before',type=Path);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--snapshot',type=Path,help='save exact pre-edit profile for invariance verification')
    p.add_argument('--reviewed-patch',action='store_true')
    p.add_argument('--capture',type=Path);p.add_argument('--dma-output',type=Path)
    p.add_argument('--corpus',type=Path)
    a=p.parse_args();profile=tomllib.loads(a.profile.read_text())
    if a.corpus:
        report=corpus_inventory(a.atlas,a.corpus,profile);a.output.write_text(json.dumps(report,indent=2)+'\n');print(report['summary']);return
    if a.capture:
        dma,proof=dma_phases(a.atlas,a.capture);a.dma_output.write_bytes(dma);print('DMA evidence',proof)
    if a.before:invariants(tomllib.loads(a.before.read_text()),profile)
    if a.snapshot:a.snapshot.write_bytes(a.profile.read_bytes())
    result=propose(profile,a.atlas);a.output.write_text(json.dumps(result,indent=2)+'\n')
    if a.reviewed_patch:
        import re
        changes=reviewed_changes(profile,a.atlas)
        text=a.profile.read_text();print('*** Begin Patch');print('*** Update File: '+str(a.profile))
        new_records=[]
        for tile,fields in changes.items():
            block=next((b for b in text.split('[[assets]]') if 'tile_hash = "'+tile+'"' in b),None)
            if block is None:
                new_records.append((tile,fields));continue
            rewritten=block
            additions=[]
            for key,value in fields.items():
                line=key+' = '+json.dumps(value,separators=(', ', ': '))
                if re.search(r'^'+key+r' = .*$',rewritten,re.M):
                    rewritten=re.sub(r'^'+key+r' = .*$',lambda m:line,rewritten,flags=re.M)
                else:additions.append(line)
            rewritten=rewritten.replace('tile_hash = "'+tile+'"','tile_hash = "'+tile+'"'+ ('\n'+'\n'.join(additions) if additions else ''),1)
            print('@@');print(' [[assets]]')
            for line in block.strip('\n').splitlines():print('-'+line)
            for line in rewritten.strip('\n').splitlines():print('+'+line)
        if new_records:
            anchor=profile['assets'][-1]['tile_hash']
            block=next(b for b in text.split('[[assets]]') if 'tile_hash = "'+anchor+'"' in b)
            block=block.split('[[rules]]')[0].strip('\n')
            print('@@');print(' [[assets]]')
            for line in block.splitlines():print(' '+line)
            for tile,fields in new_records:
                print('+');print('+[[assets]]');print('+tile_hash = "'+tile+'"')
                for key,value in fields.items():print('+'+key+' = '+json.dumps(value,separators=(', ', ': ')))
        print('*** End Patch')
    else:print(json.dumps(result,indent=2))

if __name__=='__main__':main()
