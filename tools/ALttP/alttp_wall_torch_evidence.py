#!/usr/bin/env python3
"""Inventory exact final wall-torch footprints and their missing local contacts.

Handler templates are reusable ownership evidence, NOT signed height anchors.
Uses capture-pinned complete inputs, and labels optional current-profile research.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import tomllib


def identity(vram, base, word):
    address = (base + (word & 1023) * 32) & 65535
    pixels = bytes(sum(((vram[(address + b//2*16 + y*2 + b%2) & 65535] >> (7-x)) & 1) << b
                       for b in range(4)) for y in range(8) for x in range(8))
    value = 14695981039346656037
    for byte in bytes((1, 1, 4, 8, 8)) + pixels:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64)-1)
    return f'v1:4bpp:{value:016x}'


def match_footprint(maps, words, x, y, width, height, flip, row_major=False):
    """Exact complete template/flip, allowing only a uniform palette shift."""
    if x < 0 or y < 0 or x+width > 64 or y+height > 64:
        return None
    parts = []
    palette_delta = None
    for xx in range(width):
        for yy in range(height):
            sx = width-1-xx if flip & 1 else xx
            sy = height-1-yy if flip & 2 else yy
            expected = words[sy*width+sx if row_major else sx*height+sy] ^ (0x4000 if flip & 1 else 0) ^ (0x8000 if flip & 2 else 0)
            word = maps[(y+yy)*64+x+xx]
            if (word & 0xc3ff) != (expected & 0xc3ff):
                return None
            delta = ((word >> 10) - (expected >> 10)) & 7
            if palette_delta is not None and palette_delta != delta:
                return None
            palette_delta = delta
            parts.append((x+xx, y+yy, word))
    return palette_delta, parts


def audit(capture, atlas, research_profile=None, runtime_path=None):
    prefix = str(capture)
    state = json.loads(Path(prefix+'.dungeon.json').read_text())
    blob = Path(prefix+'.dungeon-input.bin').read_bytes()
    tpl = Path(prefix+'.dungeon-templates.bin').read_bytes()
    assert blob[:8] == b'ALTPDI1\0' and len(blob) == 90120
    assert tpl[:8] == b'ALTPDT1\0' and len(tpl) == 12884
    selected = research_profile or Path(prefix+'.profile.toml')
    profile = tomllib.loads(selected.read_text())
    assets = {a['tile_hash']: a for a in profile['assets']}
    records = []
    footprints = {}
    for bg in (1, 0):
        maps = struct.unpack('<4096H', blob[8+(0 if bg else 8192):8+(8192 if bg else 16384)])
        base = state['graphics_base'] if bg else state['bg1_graphics_base']
        if base == 65535:
            base = state['graphics_base']
        for offset, width, height in ((0xa14, 4, 2), (0xa24, 4, 2), (0xa34, 2, 4), (0xa44, 2, 4)):
            words = struct.unpack_from('<8H', tpl, 8+offset)
            for flip in range(4):
                for y in range(65-height):
                    for x in range(65-width):
                        match = match_footprint(maps, words, x, y, width, height, flip, row_major=height==2)
                        if match is None:
                            continue
                        palette_delta, members = match
                        key = (bg, x, y, width, height)
                        if key in footprints:
                            footprints[key]['template_claims'].append(dict(template=offset, flip=flip, palette_delta=palette_delta))
                            continue
                        parts = []
                        for px, py, word in members:
                            tile = identity(blob[24584:], base, word)
                            a = assets.get(tile, {})
                            local = a.get('height')
                            if isinstance(local, int):
                                local = [local]*64
                            emission = a.get('emission_rgba', [0]*256)
                            parts.append(dict(x=px, y=py, word=word, tile_hash=tile,
                                              local_height=local, role=a.get('tile_role', 'unknown'),
                                              emitting_pixels=sum(bool(emission[i*4+3] and any(emission[i*4:i*4+3])) for i in range(64))))
                        record = dict(bg=bg, origin=[x,y], template=offset, flip=flip,
                                      template_claims=[dict(template=offset, flip=flip, palette_delta=palette_delta)],
                                      members=parts, missing_local=sum(p['local_height'] is None for p in parts),
                                      missing_emitting_local=sum(p['emitting_pixels']>0 and p['local_height'] is None for p in parts),
                                      attachment_base=None, reason='Template identity alone does not supply a signed attachment datum; this audit does not solve attachments')
                        footprints[key] = record
                        records.append(record)
    if runtime_path:
        runtime = json.loads(runtime_path.read_text())
        if research_profile:
            raise ValueError('perimeter contacts require capture-pinned profile, not research substitution')
        if runtime['room_id'] != state['room']:
            raise ValueError('runtime room does not match complete input')
        digest = 14695981039346656037
        for byte in blob[8:8200]:
            digest = ((digest ^ byte)*1099511628211) & ((1 << 64)-1)
        if runtime['bg2_words_fnv1a64'] != f'{digest:x}':
            raise ValueError('runtime final BG2 words do not match capture')
        digest = 14695981039346656037
        for byte in selected.read_bytes():
            digest = ((digest ^ byte)*1099511628211) & ((1 << 64)-1)
        if runtime['profile_fnv1a64'] != f'{digest:x}':
            raise ValueError('runtime profile does not match selected authored metadata')
        placed = {(r['x'],r['y']):r for r in runtime['structural_placements']}
        all_maps = struct.unpack('<8192H', blob[8:16392])
        for record in records:
            if record['bg'] != 1:
                continue
            members = {(r['x'],r['y']) for r in record['members']}
            contacts = []
            for member in record['members']:
                x,y = member['x'],member['y']
                for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                    nx,ny = x+dx,y+dy
                    if (nx,ny) in members or not (0<=nx<64 and 0<=ny<64):
                        continue
                    neighbor = placed.get((nx,ny))
                    if neighbor is None:
                        continue
                    word = all_maps[ny*64+nx]
                    tile = identity(blob[24584:], state['graphics_base'], word)
                    local = assets.get(tile,{}).get('height')
                    if isinstance(local,int):local=[local]*64
                    part = member['local_height']
                    def sample(h,w,xx,yy):
                        return h[(7-yy if w&0x8000 else yy)*8+(7-xx if w&0x4000 else xx)]
                    votes = None
                    if part is not None and local is not None:
                        votes = [neighbor['floor_base']+
                                 sample(local,word,0 if dx>0 else 7 if dx<0 else k,0 if dy>0 else 7 if dy<0 else k)-
                                 sample(part,member['word'],7 if dx>0 else 0 if dx<0 else k,7 if dy>0 else 0 if dy<0 else k)
                                 for k in range(8)]
                    contacts.append(dict(member=[x,y],neighbor=[nx,ny],neighbor_base=neighbor['floor_base'],
                                         exact_seam_base_votes=votes,
                                         status='missing_intrinsic_contact' if votes is None else
                                         'incompatible_exact_seam' if len(set(votes))!=1 else
                                         'candidate_seam_requires_surface_ownership'))
            record['perimeter_contacts'] = contacts
    with sqlite3.connect(atlas) as db:
        authored_objects = [json.loads(row[0]) for row in db.execute(
            "select data_json from world_record where kind='dungeon_objects' and json_extract(data_json,'$.room_id')=? and json_extract(data_json,'$.subtype')=0 and json_extract(data_json,'$.object_id') in (85,86,127,128)", (state['room'],))]
    return dict(capture=prefix, room=state['room'], research_replay=bool(research_profile),
                selected_profile_sha256=hashlib.sha256(selected.read_bytes()).hexdigest(),
                input_sha256=hashlib.sha256(blob).hexdigest(), atlas_objects=authored_objects,
                footprints=records, approval=False)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture', type=Path, help='one capture or directory for corpus audit')
    p.add_argument('--atlas', required=True, type=Path)
    p.add_argument('--research-profile', type=Path)
    p.add_argument('--runtime', type=Path, help='matching complete runtime solution for exact perimeter contact audit')
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    if a.capture.is_dir():
        records = []
        for capture in sorted(a.capture.glob('frame_room*.s9xrmf')):
            missing = [s for s in ('.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin', '.profile.toml')
                       if not Path(str(capture)+s).is_file()]
            if missing:
                records.append(dict(capture=str(capture), status='unsupported_missing_inputs', missing=missing))
                continue
            r = audit(capture, a.atlas, a.research_profile)
            r['status'] = 'complete_input_inventory' # ownership candidate audit, not packet parity approval
            records.append(r)
        supported = [r for r in records if r['status']=='complete_input_inventory']
        footprints = [f for r in supported for f in r['footprints']]
        summary = dict(packets=len(records), complete_inputs=len(supported),
                       unsupported=len(records)-len(supported),
                       footprint_occurrences=len(footprints), members=8*len(footprints),
                       missing_local=sum(r['missing_local'] for r in footprints),
                       missing_emitting_local=sum(r['missing_emitting_local'] for r in footprints),
                       attachment_base_unresolved=len(footprints),
                       rooms=sorted({r['room'] for r in supported}), approval=False)
        result = dict(summary=summary, records=records)
        print(json.dumps(summary))
    else:
        result = audit(a.capture, a.atlas, a.research_profile, a.runtime)
        print([(r['bg'], r['origin'], hex(r['template']), r['missing_local']) for r in result['footprints']])
    a.output.write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()
