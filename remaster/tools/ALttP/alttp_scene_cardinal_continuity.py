#!/usr/bin/env python3
"""Conditional observed same-writer cardinal-band continuity, not room approval.

Uses intact ROM section claims and pinned authored relief, not solver proposals,
to determine a unit edge step. Reports actual selected scene/main pixel heights;
terminal placement bases are retained only for diagnosing source selection.
Reference-transferred ownership is conditional and never live writer history.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sqlite3
import tomllib

from alttp_doorway_detection import ROM_SHA, displayed
from alttp_exposed_height_detection import cardinal_claims
from alttp_room_geometry import RoomSnapshot, RoomProvenance


def compare(a, b, ca, cb, axis):
    """Two observed neighboring edge samples, independent intrinsic slope.

    None means unsupported, not a passing contract. Interior gradients on both
    sides must agree with the dispatch's oriented unit slope. No guessed fill.
    """
    if not ca or not cb or ca != cb or ca['axis'] != axis:
        return None
    if not a.get('has_height') or not b.get('has_height'):
        return None
    ka, kb = a['k'], b['k']
    if axis:
        if ka % 8 != 7 or kb % 8 != 0 or ka // 8 != kb // 8:
            return None
        ia, ib = ka - 1, kb + 1
    else:
        if ka // 8 != 7 or kb // 8 != 0 or ka % 8 != kb % 8:
            return None
        ia, ib = ka - 8, kb + 8
    la, lb = a.get('authored'), b.get('authored')
    if not isinstance(la, list) or not isinstance(lb, list) or len(la) != 64 or len(lb) != 64:
        return None
    step = 1 if ca['reverse'] else -1
    if la[ka] - la[ia] != step or lb[ib] - lb[kb] != step:
        return None
    ah, bh = a['base'] + a['local'], b['base'] + b['local']
    return dict(status='pass' if bh - ah == step else 'fail', a_height=ah,
                b_height=bh, expected_b=ah + step, expected_step=step,
                actual_step=bh-ah, residual=bh-ah-step)


def audit(snapshot, claims, terminal, assets, pixels, scroll):
    if len(terminal) != 8192 or len({(r['bg'], r['x'], r['y']) for r in terminal}) != 8192:
        raise ValueError('requires 8192 unique placements')
    cells = {}
    for r in terminal:
        key = (r['bg'], r['x'], r['y'])
        c = snapshot.cell(key[0]-1, key[1], key[2])
        if int(r['hash'], 16) != int(c.tile_hash.split(':')[-1], 16):
            raise ValueError('terminal/map art mismatch')
        values = assets.get(int(r['hash'], 16), {}).get('height')
        cells[key] = dict(terminal=r, authored=displayed(values, c.word) if isinstance(values, list) and len(values) == 64 else None)
    observed = {}; scene_count = Counter()
    for p in pixels:
        name = 'scene' if p['use_scene'] else 'main'
        source = p.get(name, {})
        if source.get('source') != 2 or source.get('bg') not in (1, 2) or source.get('ink', 0) <= 0:
            continue
        wx, wy = (scroll[0]+p['sx']) & 511, (scroll[1]+p['sy']) & 511
        key = (source['bg'], wx//8, wy//8); c = snapshot.cell(key[0]-1, *key[1:])
        canonical = (7-wy%8 if c.vflip else wy%8)*8 + (7-wx%8 if c.hflip else wx%8)
        if int(source['hash'],16) != int(c.tile_hash.split(':')[-1],16) or source['k'] != canonical:
            raise ValueError('stale selected raster/map/camera')
        scene_count[name] += 1
        observed[p['sx'],p['sy']] = dict(source, cell=key, k=(wy%8)*8+wx%8,
            authored=cells[key]['authored'], buffer=name,
            terminal_base=cells[key]['terminal']['base'],
            terminal_plane_bases=cells[key]['terminal']['plane_bases'])
    rows = []; unsupported = Counter(); candidates = 0
    for (sx,sy), a in observed.items():
        for dx,dy,axis in ((1,0,1),(0,1,0)):
            b = observed.get((sx+dx,sy+dy))
            if not b or a['cell'] == b['cell']:
                continue
            ca,cb = claims.get(a['cell']),claims.get(b['cell'])
            if not ca or ca != cb or ca['axis'] != axis:
                continue
            candidates += 1
            result = compare(a,b,ca,cb,axis)
            if result is None:
                unsupported['missing_source_or_nonunit_authored_edge'] += 1
                continue
            rows.append(dict(sx=sx, sy=sy, a_cell=a['cell'], b_cell=b['cell'],
                             claim=ca, a=a, b=b, **result))
    groups = Counter((r['claim']['writer'],r['claim']['axis'],r['claim']['reverse'],
                      r['a_cell'][0],r['a_cell'][1] if r['claim']['axis'] else r['a_cell'][2],
                      r['a']['hash'],r['b']['hash'],r['status'],r['residual']) for r in rows)
    return dict(approval=False, room=snapshot.room, complete_placements=len(cells),
        observed_selected_pixels=dict(scene_count), candidate_edge_pixels=candidates,
        status_counts=dict(Counter(r['status'] for r in rows)), unsupported=dict(unsupported),
        groups=[dict(writer=k[0],axis=k[1],reverse=k[2],bg=k[3],a_depth=k[4],
            a_hash=k[5],b_hash=k[6],status=k[7],residual=k[8],pixels=v) for k,v in sorted(groups.items())],
        records=rows, limitations=['Only observed selected scene or main edges inside intact same-writer three-band sections.',
          'Relative unit continuation only; no absolute floor datum or complete-room coverage.',
          'Transferred reference provenance is not live operation-level writer history.',
          'Terminal selected base is diagnostic; it does not substitute for actual scene source.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('snapshot','rom','atlas','terminal','profile','pixels','state','output'):
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args(); rom=a.rom.read_bytes()
    if hashlib.sha256(rom).hexdigest()!=ROM_SHA: p.error('unsupported ROM')
    s=RoomSnapshot.read(a.snapshot); provpath=Path(str(a.snapshot)+'.prov'); pr=RoomProvenance.read(provpath,s)
    with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro',uri=True) as db:
        if db.execute("SELECT value FROM run_info WHERE key='rom_sha256'").fetchone()[0] != ROM_SHA: p.error('atlas ROM mismatch')
        objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    objects={(o['layer'],o['ordinal']):o for o in objects if o['room_id']==s.room}
    claims=cardinal_claims(s,pr,objects,rom[0x1b52:0x1b52+12876]); state=json.loads(a.state.read_text())
    if state['room']!=s.room: p.error('room mismatch')
    profile=tomllib.loads(a.profile.read_text()); assets={int(v['tile_hash'].split(':')[-1],16):v for v in profile.get('assets',[])}
    result=audit(s,claims,[json.loads(l) for l in a.terminal.read_text().splitlines()],assets,
                 (json.loads(l) for l in a.pixels.read_text().splitlines()),state['scroll'])
    result['inputs']={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in (a.snapshot,provpath,a.rom,a.atlas,a.terminal,a.profile,a.pixels,a.state)}
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('complete_placements','observed_selected_pixels','candidate_edge_pixels','status_counts','unsupported')}))


if __name__=='__main__': main()
