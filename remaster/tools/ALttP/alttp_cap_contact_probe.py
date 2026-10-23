"""Review exact regional cap contacts, authored samples and placed provenance.

This intentionally emits proposals/conflicts, not a publishable runtime map.
"""
import argparse
import hashlib
import json
import tomllib
from pathlib import Path
from alttp_room_geometry import RoomSnapshot, RoomProvenance


def regional_votes(current, neighbor, neighbor_inner):
    """Exact region seam votes; never choose majority or repair authored samples."""
    cap, face = [], []
    for k, (v, w, inner) in enumerate(zip(current, neighbor, neighbor_inner)):
        if v['height'] is None or w['height'] is None or inner is None:
            continue
        up = v['normal'] == [128, 128, 255]
        other_up = w['normal'] == [128, 128, 255]
        if up and other_up:
            cap.append({'pixel': k, 'base_delta': w['height'] - v['height']})
        elif not up and not other_up:
            step = abs(w['height'] - inner)
            face.append({'pixel': k, 'base_delta': w['height'] + step - v['height'], 'step': step})
    return cap, face


def status(votes):
    return ('compatible_relative_contact' if votes and
            len({v['base_delta'] for v in votes}) == 1 else 'unmatched_or_conflicting')


def review(snapshot_path, profile_path, solution_path):
    s = RoomSnapshot.read(snapshot_path)
    provenance_path = Path(str(snapshot_path) + '.prov')
    pr = RoomProvenance.read(provenance_path, s)
    profile = tomllib.loads(profile_path.read_text())
    assets = {v['tile_hash']: v for v in profile['assets']}
    solution = json.loads(solution_path.read_text())
    base = {(x, y): b for plane, x, y, b in solution['cells_plane_x_y_base'] if plane == 0}
    base.update({(v['x'], v['y']): v['floor_base'] for v in solution['structural_placements']})
    attr_path = Path(str(snapshot_path) + '.attr')
    raw_attr = attr_path.read_bytes()
    if raw_attr[:8] != b'ALTPAT1\0' or len(raw_attr) != 8202 or int.from_bytes(raw_attr[8:10], 'little') != s.room:
        raise ValueError('complete collision arrays do not match room')
    attr = raw_attr[10:]
    def sample(cell, x, y):
        asset = assets.get(cell.tile_hash, {})
        hs, ns = asset.get('height'), asset.get('normal_xyz')
        i = (7-y if cell.vflip else y)*8 + (7-x if cell.hflip else x)
        return {'height': hs[i] if hs else None, 'normal': ns[i*3:i*3+3] if ns else None}
    def record(x, y):
        cell = s.cell(1, x, y)
        return {'xy': [x, y], 'hash': cell.tile_hash, 'flip': [cell.hflip, cell.vflip],
                'writer': pr.owner(1, x, y), 'attribute': attr[y*64+x], 'base': base.get((x, y)),
                'north': [sample(cell, k, 0) for k in range(8)],
                'south': [sample(cell, k, 7) for k in range(8)],
                'west': [sample(cell, 0, k) for k in range(8)],
                'east': [sample(cell, 7, k) for k in range(8)]}
    templates = json.loads(Path('remaster/tools/ALttP/alttp-reference-wall-templates-v1.json').read_text())
    targets = [v['reference'] for v in templates['templates'] if v.get('id') == 'diagonal-authored-cap']
    contacts, caps = [], []
    for y in range(64):
        for x in range(64):
            if s.cell(1, x, y).tile_hash not in targets:
                continue
            current = record(x, y)
            caps.append(current)
            for dx, dy, edge, opposite in ((1, 0, 'east', 'west'), (-1, 0, 'west', 'east'),
                                            (0, 1, 'south', 'north'), (0, -1, 'north', 'south')):
                nx, ny = x+dx, y+dy
                if not (0 <= nx < 64 and 0 <= ny < 64):
                    continue
                neighbor = record(nx, ny)
                inner = [sample(s.cell(1, nx, ny),
                                1 if dx > 0 else 6 if dx < 0 else k,
                                1 if dy > 0 else 6 if dy < 0 else k)['height'] for k in range(8)]
                # Normal labels corroborate the explicit user cap template;
                # they do not establish support or cross-writer ownership.
                cap_votes, face_votes = regional_votes(current[edge], neighbor[opposite], inner)
                contacts.append({'from': current['xy'], 'to': neighbor['xy'], 'direction': edge,
                                 'same_writer': current['writer'] == neighbor['writer'],
                                 'writer': current['writer'], 'neighbor_writer': neighbor['writer'],
                                 'neighbor_hash': neighbor['hash'], 'neighbor_base': neighbor['base'],
                                 'attributes': [current['attribute'], neighbor['attribute']],
                                 'cap_votes': cap_votes, 'face_votes': face_votes,
                                 'cap_status': status(cap_votes), 'face_status': status(face_votes)})
    unresolved = []
    for placement in solution['unresolved_wall_placements']:
        x, y = placement['xy']
        r = record(x, y)
        hs = assets.get(r['hash'], {}).get('height', [])
        unresolved.append({'xy': [x,y], 'hash': r['hash'], 'writer': r['writer'],
                           'attribute': r['attribute'], 'local_span': [min(hs),max(hs)] if hs else None})
    groups = {}
    for r in unresolved:
        groups.setdefault(r['hash'], []).append(r['xy'])
    # Locate the collidable authored-flat boundary independently of a capture
    # crop. Its relative-zero value is not an accepted ground anchor.
    adjacent_flat = []
    for contact in contacts:
        nx,ny = contact['to']
        cell = s.cell(1,nx,ny)
        hs = assets.get(cell.tile_hash, {}).get('height', [])
        if hs and min(hs)==max(hs):
            adjacent_flat.append(contact)
    return {'format': 'alttp-cap-regional-contact-review-v1', 'room': s.room, 'publishable': False,
            'input_sha256': {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in (snapshot_path, provenance_path, attr_path, profile_path, solution_path)},
            'issues': solution['geometry_component_issues'], 'anchors': solution['relative_anchor_provenance'],
            'writers': pr.objects, 'contacts': contacts, 'cap_placements': caps,
            'unresolved_wall_denominator': len(unresolved), 'unresolved_walls': unresolved,
            'unresolved_wall_groups': groups, 'adjacent_flat_contacts': adjacent_flat}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--snapshot', type=Path, required=True)
    p.add_argument('--profile', type=Path, required=True)
    p.add_argument('--solution', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if a.output.resolve() in {a.snapshot.resolve(), a.profile.resolve(), a.solution.resolve(),
                             Path(str(a.snapshot)+'.attr').resolve(), Path(str(a.snapshot)+'.prov').resolve()}:
        p.error('output must be distinct from inputs')
    report = review(a.snapshot, a.profile, a.solution)
    a.output.write_text(json.dumps(report, indent=2)+'\n')
    for contact in report['contacts']:
        if contact['from'] == [10, 5]:
            print(json.dumps(contact))
    print('unresolved wall groups:', {k: len(v) for k,v in report['unresolved_wall_groups'].items()})
    print('cap placements:', len(report['cap_placements']))
    print('same-writer neighbors:', sorted({(c['neighbor_hash'], c['neighbor_writer']) for c in report['contacts'] if c['same_writer']}))
    print('cap writer dispatches:', [(i+1,v) for i,v in enumerate(report['writers'])
                                     if i+1 in {c['writer'] for c in report['contacts']}])


if __name__ == '__main__':
    main()
