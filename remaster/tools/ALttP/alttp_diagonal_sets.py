#!/usr/bin/env python3
"""ROM-dispatch diagonal inventory and conservative canonical array transfer.

Dry-run is the default. Apply emits an apply_patch patch, never rewrites a
profile itself. No tile similarity, room whitelist, or placement base inference.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import struct
import subprocess
import tomllib

from alttp_atlas import SUPPORTED_US_SHA256
from alttp_room_geometry import RoomSnapshot, RoomProvenance

REFERENCES = ('a4aa6ebd24fefeb7', '503ab068dfa941d3',
              '3bc2b767d36c52c3', '31aba23efb31666b')
REFERENCES = tuple('v1:4bpp:' + key for key in REFERENCES)
ROLES = ('cap', 'upper', 'middle', 'foot', 'trim')
FIELDS = ('height', 'normal_xyz', 'occlusion')
VETO = {'floor', 'door', 'doorway', 'obstacle', 'stair_top', 'stair_bottom', 'wall_corner'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def band_for(row, word):
    return 4 - row if word & 0x8000 else row


def transformed(values, channels=1, hflip=False, vflip=False, normal=False):
    """Canonical H/V mirroring, including signed normal components about 128."""
    out = []
    for y in range(8):
        for x in range(8):
            i = ((7-y if vflip else y)*8 + (7-x if hflip else x))*channels
            pixel = list(values[i:i+channels])
            if normal:
                if hflip:
                    pixel[0] = max(0, min(255, 256-pixel[0]))
                if vflip:
                    pixel[1] = max(0, min(255, 256-pixel[1]))
            out.extend(pixel)
    return out


def transfer(reference, target_hflip=False):
    # Template V flips reverse band order, not intrinsic canonical artwork.
    # The accepted canonical semantics are SE; no H transform is needed for
    # those assets. Other canonical facings need independent semantic approval.
    return {field: transformed(reference[field], 3 if field == 'normal_xyz' else 1,
                               hflip=target_hflip, normal=field == 'normal_xyz')
            for field in FIELDS if field in reference}


def verify_low_cardinal_geometry(snapshot, templates, assets):
    """Independent zero-foot, 24-rise LOW cardinal seam convention.

    The second N column and first W row carry all three distinct height
    bands. The other N column reuses one identity at two bands and cannot
    independently establish a datum; do not claim that conflicted reuse.
    """
    evidence = []
    for axis, offset in ((0,0x328+8),(1,0x308)):
        for band,word in enumerate(struct.unpack_from('<3H',templates,offset)):
            key=snapshot._tile_hash((snapshot.layers[1][1]+(word&1023)*16)&32767)
            height=assets.get(key,{}).get('height')
            expected=[8*(2-band)+7-(x if axis else y) for y in range(8) for x in range(8)]
            if height != expected:
                raise ValueError('LOW cardinal seam/rise evidence missing or changed: '+key)
            evidence.append(dict(axis=axis,band=band,word=word,tile_hash=key,height=height))
    return evidence


def proposal_patch(text, updates):
    """Replace only authorized fields, preserving all unrelated bytes."""
    sections = []
    for match in re.finditer(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)', text):
        old = match.group()
        key = re.search(r'(?m)^tile_hash\s*=\s*"([^"]+)"', old)
        if not key or key[1] not in updates:
            continue
        new = old
        for field, values in updates[key[1]].items():
            line = field + ' = [' + ', '.join(map(str, values)) + ']\n'
            pattern = rf'(?m)^{field}\s*=.*\n'
            if re.search(pattern, new):
                new = re.sub(pattern, lambda _: line, new, count=1)
            else:
                new = new.rstrip() + '\n' + line + '\n'
        if old != new:
            sections.append('@@\n' + ''.join('-'+line+'\n' for line in old.splitlines()) +
                            ''.join('+'+line+'\n' for line in new.splitlines()))
    missing = set(updates) - {a['tile_hash'] for a in tomllib.loads(text).get('assets', [])}
    if missing:
        # Insert at the first rule boundary, without reserializing unrelated
        # materials/settings/assets. New entries carry intrinsic arrays only;
        # semantic classification remains in the existing canonical sidecar.
        rule = re.search(r'(?m)^\[\[rules\]\]', text)
        if not rule:
            raise ValueError('profile lacks an insertion boundary')
        additions = []
        for key in sorted(missing):
            additions.extend(['[[assets]]', f'tile_hash = "{key}"'])
            for field,values in updates[key].items():
                additions.append(field+' = ['+', '.join(map(str,values))+']')
            additions.extend(['height_sampling = "nearest"', ''])
        sections.append('@@\n'+''.join('+'+line+'\n' for line in additions)+' [[rules]]\n')
    return sections


def inspect(snapshot, provenance, objects, templates):
    """Reproduce each dispatch footprint; final writer+word jointly prove a band.

    An overwritten cell is not owned even when its art matches. Equal-word
    writes are unobservable in ALTPRV1 and stay unproved. Boundary wraps are
    rejected, rather than interpreted as room-local contact.
    """
    assemblies, claims = [], {}
    for writer, source in enumerate(provenance.objects, 1):
        obj = objects.get((source['phase'], source['ordinal']))
        if source['is_door'] or not obj or obj['subtype'] != 0:
            continue
        handler = obj['object_id']
        if not 9 <= handler <= 32:
            continue
        high = handler <= 20
        off = (0x420 if high else 0x498) + (handler-(9 if high else 21))*10
        words = struct.unpack_from('<5H', templates, off)
        step = -1 if (handler-9)%4 in (0, 3) else 1
        raw = source['raw']
        length = (raw & 3)*4 + ((raw >> 8) & 3) + 6
        x0, y0 = (raw & 255) >> 2, raw >> 10
        record = dict(writer=writer, handler=handler, family='high' if high else 'low',
                      template_offset=off, phase=source['phase'], origin=[x0,y0],
                      repetitions=length, words=list(words), placements=[],
                      omitted=Counter())
        for layer in range(2):
            for c in range(length):
                for row, word in enumerate(words):
                    x, y = x0+c, y0+c*step+row
                    if not (0 <= x < 64 and 0 <= y < 64):
                        record['omitted']['outside_room'] += 1
                        continue
                    cell = snapshot.cell(layer, x, y)
                    if provenance.owner(layer,x,y) != writer:
                        record['omitted']['not_final_writer_or_equal_write'] += 1
                        continue
                    if cell.word != word:
                        record['omitted']['writer_word_mismatch'] += 1
                        continue
                    band = band_for(row, word)
                    placement = dict(layer=layer,x=x,y=y,word=word,
                                     tile_hash=cell.tile_hash,band=ROLES[band],
                                     hflip=cell.hflip,vflip=cell.vflip)
                    record['placements'].append(placement)
                    claims.setdefault((layer,x,y),set()).add((cell.tile_hash, high, band))
        record['omitted'] = dict(record['omitted'])
        assemblies.append(record)
    # LoadType1ObjectSubtype2 38/39 draws a 4x3 stair opening, then only
    # ORs priority at its two BG2 flanks (dsto-1,dsto+4). It does not replace
    # the flank artwork. Recover an underlying diagonal band only with one
    # earlier dispatch footprint, exact word apart from that priority bit,
    # and all three other geometric bands in that column still owned by it.
    # Neither stair art inside the opening nor arbitrary matching hashes qualify.
    priority_candidates = defaultdict(list)
    for record in assemblies:
        if record['family'] != 'high':
            continue
        x0,y0 = record['origin']
        step = -1 if (record['handler']-9)%4 in (0,3) else 1
        for column in range(record['repetitions']):
            x,y = x0+column,y0+column*step
            for row,word in enumerate(record['words']):
                yy=y+row
                if not (0<=x<64 and 0<=yy<64) or band_for(row,word)>3:
                    continue
                owner = provenance.owner(1,x,yy)
                if not record['writer'] < owner <= len(provenance.objects):
                    continue
                source = provenance.objects[owner-1]
                overlay = objects.get((source['phase'],source['ordinal']))
                if source['is_door'] or not overlay or (overlay['subtype'],overlay['object_id']) not in ((2,0x38),(2,0x39)):
                    continue
                if (x,yy) not in ((overlay['x']-1,overlay['y']),(overlay['x']+4,overlay['y'])):
                    continue
                cell=snapshot.cell(1,x,yy)
                if cell.word != word|0x2000:
                    continue
                companions=[r for r,w in enumerate(record['words']) if r!=row and band_for(r,w)<4]
                if not all(0<=y+r<64 and provenance.owner(1,x,y+r)==record['writer'] and
                           snapshot.cell(1,x,y+r).word==record['words'][r] for r in companions):
                    continue
                band=band_for(row,word)
                placement=dict(layer=1,x=x,y=yy,word=cell.word,
                    tile_hash=cell.tile_hash,band=ROLES[band],hflip=cell.hflip,vflip=cell.vflip,
                    ownership_operation='verified_stair_flank_priority_or',final_writer=owner)
                priority_candidates[1,x,yy].append((record,placement,band))
    for xy,candidates in priority_candidates.items():
        if len(candidates)!=1:
            continue
        record,placement,band=candidates[0]
        record['placements'].append(placement)
        record['omitted']['not_final_writer_or_equal_write']-=1
        claims.setdefault(xy,set()).add((placement['tile_hash'],True,band))
    return assemblies, claims


def eligibility(key, bands, assets, semantics, references, low_ids):
    reasons = []
    if len(bands) != 1:
        reasons.append('conflicting_assembly_bands')
    if key in low_ids:
        reasons.append('also_low_diagonal_topology')
    annotation = semantics.get(key, {})
    asset = assets.get(key, {})
    if annotation.get('context_dependent') or asset.get('tile_semantic_context_dependent'):
        reasons.append('context_dependent')
    if annotation.get('role') in VETO or asset.get('tile_role') in VETO:
        reasons.append('explicit_non_equivalent_role')
    if annotation.get('role') != 'wall' or annotation.get('normals') != ['south_east']:
        reasons.append('missing_canonical_southeast_wall_evidence')
    if asset.get('tile_semantics_authored') and (asset.get('tile_role') != 'wall' or
            asset.get('tile_semantic_normals') != ['south_east']):
        reasons.append('profile_semantic_conflict')
    if key in references:
        reasons.append('immutable_reference')
    # Reference absence of occlusion is not permission to destroy a painted
    # target mask. A non-default existing mask requires separate pixel review.
    if 'occlusion' in asset and any(v != 255 for v in asset['occlusion']):
        reasons.append('target_occlusion_requires_review')
    return reasons


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--renderer', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--atlas', type=Path, required=True)
    p.add_argument('--profile', type=Path, required=True)
    p.add_argument('--semantics', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--apply', action='store_true', help='emit reviewed apply_patch patch (does not edit profile)')
    p.add_argument('--reuse-corpus', type=Path, help='reuse digest-verified complete discovery inputs')
    a = p.parse_args()
    renderer = a.renderer.resolve()
    rom = a.rom.read_bytes()
    if digest(rom) != SUPPORTED_US_SHA256:
        raise ValueError('unsupported ROM')
    if digest((renderer.parent/'zelda3.sfc').read_bytes()) != digest(rom):
        raise ValueError('renderer ROM mismatch')
    templates = rom[0x1b52:0x1b52+12876]
    text = a.profile.read_text()
    profile = tomllib.loads(text)
    assets = {v['tile_hash']:v for v in profile['assets']}
    semantic_document = json.loads(a.semantics.read_text())
    if semantic_document['rom_sha256'] != digest(rom):
        raise ValueError('semantic ROM mismatch')
    semantics = semantic_document['tiles']
    references = [assets[key] for key in REFERENCES]
    for band, ref in enumerate(references):
        expected = [max(0,min(24,31-8*band-x-y)) for y in range(8) for x in range(8)]
        if ref['height'] != expected or len(ref['normal_xyz']) != 192:
            raise ValueError('approved reference changed or incomplete')
    with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro', uri=True) as db:
        pin = db.execute("SELECT value FROM run_info WHERE key='rom_sha256'").fetchone()[0]
        if pin != digest(rom):
            raise ValueError('atlas ROM mismatch')
        objects = [json.loads(r[0]) for r in db.execute(
            "SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    byroom = defaultdict(dict)
    for obj in objects:
        byroom[obj['room_id']][(obj['layer'],obj['ordinal'])] = obj
    if not a.output.is_dir():
        raise ValueError('--output must be an existing review directory')
    # Include every own entrance, not merely ChooseEntrance's first one.
    entrance_result = subprocess.run([str(renderer),'--entrances'],cwd=renderer.parent,
                                     capture_output=True,text=True,check=True)
    entrances = defaultdict(list)
    for line in entrance_result.stdout.splitlines():
        fields = dict(item.split('=') for item in line.split())
        room = int(fields['room'],16)
        if room < 320:
            entrances[room].append(int(fields['entrance']))
    contexts = [(room,e) for room in range(320) for e in (entrances[room] or [-1])]
    prior = None
    if a.reuse_corpus:
        prior = json.loads((a.reuse_corpus/'discovery.json').read_text())
        if prior['rom_sha256'] != digest(rom) or prior['renderer_sha256'] != digest(renderer.read_bytes()):
            raise ValueError('reused corpus ROM/renderer mismatch')
        if prior['atlas_sha256'] != digest(a.atlas.read_bytes()):
            raise ValueError('reused corpus dispatch atlas mismatch')
        prior = {(r['room'],r['requested_entrance']):r for r in prior['rooms']}
    rooms, assemblies, high_bands, low_ids = [], [], defaultdict(set), set()
    low_bands, loaded_low_bands = defaultdict(set), defaultdict(set)
    loaded_high_bands = defaultdict(set)
    usages = defaultdict(Counter)
    loaded_templates = []
    low_cardinal_evidence = None
    for index,(room, entrance) in enumerate(contexts):
        stem = a.output / f'room-{room:03x}-entrance-{entrance}'
        raw = Path(str(stem)+'.raw')
        if prior is not None:
            saved = prior[room,entrance]
            raw = a.reuse_corpus/raw.name
            if saved['status'] == 'scanned':
                for suffix,pin in saved['input_sha256'].items():
                    if digest(Path(str(raw)+suffix).read_bytes()) != pin:
                        raise ValueError('reused input digest mismatch: '+str(raw)+suffix)
            result = subprocess.CompletedProcess([],0 if saved['status']=='scanned' else 1,
                                                  saved['renderer'],saved.get('error',''))
        else:
            result = subprocess.run([str(renderer),'--linked',hex(room),str(raw),str(entrance),'0','0','0'],
                                    cwd=renderer.parent,capture_output=True,text=True,timeout=60)
        context = dict(room=room,requested_entrance=entrance,renderer=result.stdout.strip(),
                       status='unsupported' if result.returncode else 'scanned')
        rooms.append(context)
        if result.returncode:
            context['error'] = result.stderr
            continue
        snapshot = RoomSnapshot.read(raw)
        # Accept the rule only with independently authored cardinal evidence
        # in an own-entry graphics context, not a borrowed/guessed short wall.
        if low_cardinal_evidence is None and entrance >= 0:
            try:
                low_cardinal_evidence = dict(room=room,entrance=entrance,
                    samples=verify_low_cardinal_geometry(snapshot,templates,assets))
            except ValueError:
                pass
        provenance = RoomProvenance.read(Path(str(raw)+'.prov'),snapshot)
        state_path = Path(str(raw)+'.state.json')
        context['state'] = json.loads(state_path.read_text())
        context['input_sha256'] = {suffix:digest(Path(str(raw)+suffix).read_bytes())
                                   for suffix in ('','.prov','.attr','.state.json')}
        fields = dict(v.split('=') for v in result.stdout.split())
        context['borrowed_graphics_context'] = int(fields['source_room'],16) != room
        found, claims = inspect(snapshot,provenance,byroom[room],templates)
        for record in found:
            record.update(room=room,entrance=int(fields['entrance']),theme=int(fields['theme']))
            assemblies.append(record)
        for layer in snapshot.cells:
            for cell in layer:
                own = claims.get((cell.layer,cell.x,cell.y),set())
                if len(own) == 1:
                    key,high,band = next(iter(own))
                    if high and band < 4:
                        high_bands[key].add(band)
                        usages[key]['high_'+ROLES[band]] += 1
                    elif not high and band < 4:
                        low_ids.add(key)
                        low_bands[key].add(band)
                        usages[key]['low_'+ROLES[band]] += 1
                    else:
                        usages[key]['trim'] += 1
                else:
                    usages[cell.tile_hash]['unproved_dispatch_ownership'] += 1
        # Inventory all templates in every loaded graphics context, even when
        # no corresponding object was dispatched in this state.
        for handler in range(9,33):
            off = 0x420+(handler-9)*10
            bands = []
            for row,word in enumerate(struct.unpack_from('<5H',templates,off)):
                key = snapshot._tile_hash((snapshot.layers[1][1]+(word&1023)*16)&32767)
                band = band_for(row,word)
                bands.append(dict(tile_hash=key,band=ROLES[band],word=word))
                if handler > 20:
                    low_ids.add(key)
                    if band < 4:
                        loaded_low_bands[key].add(band)
                elif band < 4:
                    loaded_high_bands[key].add(band)
            loaded_templates.append(dict(room=room,entrance=int(fields['entrance']),
                                         handler=handler,bands=bands))
        if (index+1)%20 == 0:
            print(f'Scanned {index+1}/{len(contexts)} contexts',flush=True)
    decisions, updates = {}, {}
    for key,bands in sorted({**high_bands,**low_bands}.items()):
        family = 'low' if key in low_bands else 'high'
        reasons = eligibility(key,bands,assets,semantics,REFERENCES,
                              set(high_bands) if family=='low' else low_ids)
        if key in high_bands and key in low_bands:
            reasons.append('cross_family_identity_requires_review')
        if family=='low' and low_cardinal_evidence is None:
            reasons.append('missing_independent_low_cardinal_rise')
        if (loaded_low_bands if family=='low' else loaded_high_bands)[key] != bands:
            reasons.append('conflicting_loaded_template_bands')
        if usages[key]['unproved_dispatch_ownership']:
            reasons.append('other_placements_require_ownership_review')
        decision = dict(family=family,bands=[ROLES[b] for b in sorted(bands)],reasons=reasons,
                        usages=dict(usages[key]))
        decisions[key] = decision
        if not reasons:
            arrays = transfer(references[next(iter(bands))])
            if any(assets.get(key,{}).get(f) != value for f,value in arrays.items()):
                updates[key] = arrays
                decision['status'] = 'eligible_update'
            else:
                decision['status'] = 'already_equivalent'
        else:
            decision['status'] = 'rejected'
    candidate_ids = {key for key,value in semantics.items()
                      if value.get('role')=='wall' and value.get('normals')==['south_east']}
    # Independent semantic denominator: unknown dispatches remain diagonal gaps.
    from alttp_diagonal_gaps import diagonal_universe, intrinsic_status, summarize
    universe = diagonal_universe(semantics)
    semantic_gap_inventory = summarize([intrinsic_status(assets.get(key,{})) for key in universe])
    groups = {}
    for record in loaded_templates:
        ordered = sorted(record['bands'],key=lambda b:ROLES.index(b['band']))
        ids = tuple(b['tile_hash'] for b in ordered[:4])
        family = 'low' if record['handler']>20 else 'high'
        group = groups.setdefault((family,ids),dict(family=family,ids=dict(zip(ROLES[:4],ids)),contexts=[]))
        group['contexts'].append({k:record[k] for k in ('room','entrance','handler')})
    for group in groups.values():
        proved = low_bands if group['family']=='low' else high_bands
        group['status'] = 'verified_bands' if all(key in proved for key in group['ids'].values()) else 'loaded_only'
    report = dict(format='alttp-diagonal-set-discovery-v1',verdict='reviewable but incomplete',
                  rom_sha256=digest(rom),profile_sha256=digest(text.encode()),
                  renderer_sha256=digest(renderer.read_bytes()),
                  atlas_sha256=digest(a.atlas.read_bytes()),semantics_sha256=digest(a.semantics.read_bytes()),
                   references={key:{f:assets[key].get(f) for f in FIELDS} for key in REFERENCES},
                   low_cardinal_evidence=low_cardinal_evidence,
                   semantic_gap_inventory=semantic_gap_inventory,
                   semantic_gap_ids=sorted(key for key in universe if intrinsic_status(assets.get(key,{}))['gap']),
                  rooms=rooms,assemblies=assemblies,loaded_templates=loaded_templates,
                  decisions=decisions,proposed_updates=updates,groups=list(groups.values()),
                  inventory=dict(requested_room_ids=320,scanned_room_ids=len({r['room'] for r in rooms if r['status']=='scanned'}),
                                 contexts=len(rooms),unsupported_contexts=sum(r['status']!='scanned' for r in rooms),
                                 borrowed_contexts=sum(r.get('borrowed_graphics_context',False) for r in rooms),
                                 placed_cells=8192*sum(r['status']=='scanned' for r in rooms),
                                  assembly_count=len(assemblies),high_ids=len(high_bands),low_ids=len(low_ids),
                                  low_geometric_ids=len(low_bands),loaded_low_geometric_ids=len(loaded_low_bands),
                                 eligible_updates=len(updates),loaded_high_ids=len(loaded_high_bands),
                                 loaded_only_high_ids=sorted(set(loaded_high_bands)-set(high_bands)),
                                 group_count=len(groups),southeast_semantic_ids=len(candidate_ids),
                                  southeast_without_proved_high_band=sorted(candidate_ids-set(high_bands))),
                   remaining_southeast_without_proved_diagonal_band=sorted(candidate_ids-set(high_bands)-set(low_bands)),
                  limitations=['Default constructed states and own entrances only; borrowed contexts are not gameplay-attested.',
                               'Switch, chest, water, door and progression states not enumerated; overworld not covered.',
                               'ALTPRV1 records value-changing dispatch ownership, not every equal-word write; only verified staircase flank priority-OR recovery is supported.',
                               'No new movement/support solve, absolute placement bases or gameplay comparison.',
                               'References have no explicit occlusion: equivalent runtime default is 255 at every pixel; reject painted target masks.',
                                'LOW five-row dispatches have four geometric bands plus separate floor trim, like HIGH; intrinsic 24-rise is independently constrained by LOW cardinal 23..0 profiles. Placement bases are not transferred.',
                                'Identities with unproved other uses, conflicting bands/families or painted masks are rejected.'],
                   reused_corpus=str(a.reuse_corpus) if a.reuse_corpus else None)
    (a.output/'discovery.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    summary = {k:v for k,v in report.items() if k not in ('assemblies','loaded_templates','rooms')}
    summary['contexts'] = [{k:v for k,v in r.items() if k not in ('input_sha256','state')} for r in rooms]
    summary['assembly_families'] = dict(Counter(r['family'] for r in assemblies))
    summary['omitted_footprint_cells'] = dict(sum((Counter(r['omitted']) for r in assemblies),Counter()))
    summary['proved_placements'] = sum(len(r['placements']) for r in assemblies)
    (a.output/'summary.json').write_text(json.dumps(summary,indent=2,sort_keys=True)+'\n')
    if a.apply:
        sections = proposal_patch(text,updates)
        patch = '*** Begin Patch\n*** Update File: '+str(a.profile)+'\n'+''.join(sections)+'*** End Patch\n'
        (a.output/'authoring.patch').write_text(patch)
    print(json.dumps(report['inventory'],indent=2))


if __name__ == '__main__':
    main()
