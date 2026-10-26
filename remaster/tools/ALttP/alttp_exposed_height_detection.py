#!/usr/bin/env python3
"""Detection only: exposed cross-BG cardinal seams and missing scene receivers.

No runtime/profile writes. Dispatch identity + final writer + exact ROM word
prove each complete three-band section. Pixel exposure comes from a matching
captured raster, never nearest-floor selection or a guessed BG priority.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sqlite3

from alttp_doorway_detection import ROM_SHA, displayed
from alttp_room_geometry import RoomSnapshot, RoomProvenance


def scene_source_issue(pixel):
    """Array availability, role admission and actual field validity are separate."""
    if not pixel['use_scene'] or pixel['field_known'] or not pixel['participation']:
        return None
    if pixel.get('scene', {}).get('id') or pixel.get('main', {}).get('id'):
        return None
    sub = pixel.get('sub', {})
    if not sub.get('id') or sub.get('source') != 2 or sub.get('bg') != 1 or sub.get('ink', 0) <= 0:
        return None
    if not sub.get('has_height'):
        reason = 'missing_effective_height'
    elif not sub.get('has_normals'):
        reason = 'scene_restore_requires_normals'
    elif sub.get('role') not in (1, 2, 3, 5, 6, 10):
        reason = 'scene_restore_role_not_admitted'
    else:
        reason = 'scene_restore_other_precondition_or_transport'
    return dict(sx=pixel['sx'], sy=pixel['sy'], reason=reason, sub=sub)


def cardinal_claims(snapshot, provenance, objects, templates):
    """Surviving dispatch cross sections, not arbitrary artwork matches.

    Template flip bits are retained verbatim. Three face bands must survive on
    one layer with one writer; an overwritten trim is intentionally irrelevant.
    Conflicting section descriptions are rejected rather than chosen.
    """
    claims = {}
    specs = {1: (0x2e8, 'high', 0, 0), 2: (0x2f8, 'high', 0, 1),
             3: (0x328, 'low', 0, 0), 4: (0x338, 'low', 0, 1),
             0x61: (0x2c8, 'high', 1, 0), 0x62: (0x2d8, 'high', 1, 1),
             0x63: (0x308, 'low', 1, 0), 0x64: (0x318, 'low', 1, 1)}
    for writer, source in enumerate(provenance.objects, 1):
        obj = objects.get((source['phase'], source['ordinal']))
        if source['is_door'] or not obj or obj['subtype'] != 0 or obj['object_id'] not in specs:
            continue
        off, kind, axis, reverse = specs[obj['object_id']]
        size = obj['width_code'] * 4 + obj['height_code']
        repeats = (size or 26) if kind == 'high' else size + 1
        start = reverse
        for repeat in range(repeats):
            for tangent in range(2):
                section = []
                for depth in range(start, start + 3):
                    x = obj['x'] + (depth if axis else repeat * 2 + tangent)
                    y = obj['y'] + (repeat * 2 + tangent if axis else depth)
                    n = tangent * 4 + depth
                    word = int.from_bytes(templates[off + 2*n:off + 2*n + 2], 'little')
                    section.append((x, y, word))
                for layer in range(2):
                    if any(not (0 <= x < 64 and 0 <= y < 64) or
                           provenance.owner(layer, x, y) != writer or
                           (snapshot.cell(layer, x, y).word & 0xdfff) != (word & 0xdfff)
                           for x, y, word in section):
                        continue
                    for x, y, word in section:
                        key = (layer + 1, x, y)
                        claim = dict(writer=writer, kind=kind, axis=axis, reverse=reverse,
                                     template=off, section=[list(v[:2]) for v in section])
                        if key in claims and (claims[key] is None or claims[key] != claim):
                            claims[key] = None
                        else:
                            claims[key] = claim
    return claims


def compare_seam(a, b, ca, cb, direction):
    """Signed per-pixel step, extrapolated from the exposed authored profile.

    No fixed eight-unit/base correction. Both exposed sections must be exact
    complementary dispatch families with the same transformed downhill axis.
    """
    if not ca or not cb or ca['kind'] == cb['kind'] or ca['writer'] == cb['writer']:
        return None
    if (ca['axis'], ca['reverse']) != (cb['axis'], cb['reverse']):
        return None
    dx, dy = direction
    if (not ca['axis'] and dx) or (ca['axis'] and dy):
        return None
    if not a.get('has_height') or not b.get('has_height'):
        return dict(status='unresolved', reason='missing_effective_source')
    # Profiles are displayed, with authored flips applied before reaching here.
    ax, ay = a['k'] % 8, a['k'] // 8
    bx, by = b['k'] % 8, b['k'] // 8
    if (dx and (ax != 7 or bx != 0 or ay != by)) or (dy and (ay != 7 or by != 0 or ax != bx)):
        return None
    inner = (ay-dy)*8 + ax-dx
    if not 0 <= inner < 64:
        return None
    local = a['displayed_local']
    step = local[a['k']] - local[inner]
    if not step or (step > 0) != bool(ca['reverse']):
        return None
    actual = b['actual'] - a['actual']
    return dict(status='pass' if actual == step else 'fail', expected_step=step,
                actual_step=actual, residual=actual-step,
                a_height=a['actual'], b_height=b['actual'],
                a_writer=ca['writer'], b_writer=cb['writer'])


def audit(snapshot, provenance, objects, templates, terminal, pixels, scroll):
    if len(terminal) != 8192 or len({(r['bg'], r['x'], r['y']) for r in terminal}) != 8192:
        raise ValueError('requires complete unique two-BG terminal inventory')
    cells = {}
    for r in terminal:
        key = (r['bg'], r['x'], r['y']); c = snapshot.cell(r['bg']-1, r['x'], r['y'])
        if int(r['hash'], 16) != int(c.tile_hash.split(':')[-1], 16):
            raise ValueError('terminal identity mismatch')
        # Classifier relief is not necessarily the transported effective source.
        cells[key] = dict(r, displayed_local=displayed([h-r['base'] for h in r['actual']], c.word))
    claims = cardinal_claims(snapshot, provenance, objects, templates)
    exposed = {}; issues = []
    for p in pixels:
        issue = scene_source_issue(p)
        if issue: issues.append(issue)
        source = p.get('scene' if p['use_scene'] else 'main', {})
        if source.get('source') != 2 or source.get('bg') not in (1, 2) or source.get('ink', 0) <= 0:
            continue
        wx, wy = (scroll[0]+p['sx']) & 511, (scroll[1]+p['sy']) & 511
        key = (source['bg'], wx//8, wy//8); c = snapshot.cell(key[0]-1, *key[1:])
        canonical = (7-wy%8 if c.vflip else wy%8)*8 + (7-wx%8 if c.hflip else wx%8)
        if int(source['hash'], 16) != int(c.tile_hash.split(':')[-1], 16) or source['k'] != canonical:
            raise ValueError('stale raster/map/camera')
        exposed[p['sx'], p['sy']] = dict(source, key=key, k=wy%8*8+wx%8,
            actual=source['base']+source['local'], displayed_local=cells[key]['displayed_local'])
    seams = []
    for (sx, sy), a in exposed.items():
        for dx, dy in ((1, 0), (0, 1)):
            b = exposed.get((sx+dx, sy+dy))
            if not b or a['bg'] == b['bg']: continue
            r = compare_seam(a, b, claims.get(a['key']), claims.get(b['key']), (dx, dy))
            if r: seams.append(dict(r, sx=sx, sy=sy, a_cell=a['key'], b_cell=b['key']))
    candidates = []
    for key, ca in sorted(claims.items()):
        if not ca: continue
        bg, x, y = key
        for dx, dy in ((1, 0), (0, 1)):
            other = (3-bg, x+dx, y+dy); cb = claims.get(other)
            if cb and ca['kind'] != cb['kind'] and ca['writer'] != cb['writer'] and (ca['axis'],ca['reverse']) == (cb['axis'],cb['reverse']) and bool(dx) == bool(ca['axis']):
                candidates.append(dict(a_cell=key,b_cell=other,a_writer=ca['writer'],b_writer=cb['writer']))
    observed = {(tuple(r['a_cell']),tuple(r['b_cell'])) for r in seams}
    for c in candidates:
        c['observed']=(c['a_cell'],c['b_cell']) in observed
    return dict(approval=False, complete_placements=len(cells), proved_cardinal_cells=sum(v is not None for v in claims.values()),
                observed_pixels=len(exposed), scene_source_issues=issues, source_counts=dict(Counter(r['reason'] for r in issues)),
                seam_counts=dict(Counter(r['status'] for r in seams)), seams=seams,
                candidate_edges=candidates,unobserved_candidate_edges=sum(not c['observed'] for c in candidates))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('snapshot', 'rom', 'atlas', 'terminal', 'pixels', 'state', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args(); rom = a.rom.read_bytes()
    if hashlib.sha256(rom).hexdigest() != ROM_SHA: p.error('unsupported ROM')
    s = RoomSnapshot.read(a.snapshot); provpath=Path(str(a.snapshot)+'.prov')
    pr = RoomProvenance.read(provpath, s)
    with sqlite3.connect(f'file:{a.atlas.resolve()}?mode=ro', uri=True) as db:
        if db.execute("SELECT value FROM run_info WHERE key='rom_sha256'").fetchone()[0] != ROM_SHA:
            p.error('atlas ROM mismatch')
        objects = [json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    objects = {(o['layer'], o['ordinal']): o for o in objects if o['room_id'] == s.room}
    state = json.loads(a.state.read_text())
    if state['room'] != s.room: p.error('state room mismatch')
    result = audit(s, pr, objects, rom[0x1b52:0x1b52+12876],
                   [json.loads(l) for l in a.terminal.read_text().splitlines()],
                   [json.loads(l) for l in a.pixels.read_text().splitlines()], state['scroll'])
    result['inputs'] = {str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in
                        (a.snapshot, provpath, a.rom, a.atlas, a.terminal, a.pixels, a.state)}
    a.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('complete_placements','proved_cardinal_cells','observed_pixels','source_counts','seam_counts')}))


if __name__ == '__main__': main()
