"""Independent schema22 footprint and terminal movement-support proof.

Solved floor bases are inputs to the existing room validator, not obstacle
claims. Ownership, actor footprints, contacts and member composition are rebuilt
here from complete tilemaps and completed collision arrays.
"""
from collections import deque
import re
from alttp_collision_reachability import LEADING, STEPS
from alttp_height_samples import valid_heights


def validate_groups(profile):
    groups = profile.get('obstacle_groups', [])
    if groups and profile.get('schema_version', 22) < 22:
        raise ValueError('obstacle_groups requires schema_version22')
    names, owners = set(), {}
    if len(groups) > 4096:
        raise ValueError('too many obstacle groups')
    for gi, g in enumerate(groups):
        if set(g) not in ({'name', 'members'}, {'name', 'members', 'sprite_assembly'}):
            raise ValueError('invalid obstacle group keys')
        if 'sprite_assembly' in g and (profile.get('schema_version', 22) < 30 or type(g['sprite_assembly']) is not bool):
            raise ValueError('sprite_assembly requires schema30 and a boolean')
        name = g.get('name', '')
        if not name or any(not (c.isascii() and (c.isalnum() or c in '_-')) for c in name) or name in names:
            raise ValueError('invalid or duplicate obstacle group name')
        names.add(name)
        members = g.get('members', [])
        if not 1 <= len(members) <= 4096:
            raise ValueError('obstacle group requires members')
        positions = set()
        for m in members:
            if set(m) != {'tile_hash', 'x', 'y', 'z', 'hflip', 'vflip', 'support_contact'}:
                raise ValueError('obstacle member requires all explicit keys')
            if not re.fullmatch(r'v1:(2|4|8)bpp:[0-9a-f]{16}', m['tile_hash']):
                raise ValueError('invalid obstacle tile identity')
            for k, limit in (('x', 63), ('y', 63), ('z', 4095)):
                if type(m[k]) is not int or not -limit <= m[k] <= limit:
                    raise ValueError('invalid obstacle coordinate')
            if any(type(m[k]) is not bool for k in ('hflip', 'vflip', 'support_contact')):
                raise ValueError('invalid obstacle boolean')
            pos = m['x'], m['y']
            if pos in positions:
                raise ValueError('duplicate obstacle member position')
            positions.add(pos)
            h = m['tile_hash']
            # Reusable identity index, never an ownership decision. Audit below
            # rejects every competing complete placed footprint.
            owners.setdefault(h, set()).add(gi)
        if not any(m['support_contact'] for m in members):
            raise ValueError('obstacle group requires explicit support_contact')
        if any(max(m[k] for m in members) - min(m[k] for m in members) >= 64 for k in ('x', 'y')):
            raise ValueError('obstacle footprint exceeds room')
    return groups, owners


def movement_components(attributes, plane, link_xy, stairs):
    """Exact runtime ordinary-body scope, including separately seeded regions.

    Preserve directed leading-edge movement; no tile00 flood or floor merging.
    Cell ownership is first reproduced body component, as in native projection.
    """
    attr = attributes[plane]
    def walk(x, y, d):
        return all((v := attr[((y+sy)//8)*64+(x+sx)//8]) == 0 or
                   (0x80 <= v <= 0x8f and (v & 1) == (d >= 2)) for sx, sy in LEADING[d])
    seeds = []
    if plane == link_xy[0]:
        seeds.append((link_xy[1] & 511, link_xy[2] & 511))
    for s in stairs:
        x, y = s[:2]
        seeds.extend((x*8+8, yy) for yy in (y*8-24, y*8+16) if 0 <= x*8+8 <= 496 and 0 <= yy <= 487)
    seeds.extend((x*8, y*8) for y in range(61) for x in range(62)
                 if all(walk(x*8, y*8, d) for d in range(4)))
    visited = bytearray(497*488)
    components = [-1]*4096
    count = 0
    for x, y in seeds:
        if not (0 <= x <= 496 and 0 <= y <= 487) or visited[y*497+x]:
            continue
        if any(attr[yy*64+xx] != 0 for yy in range((y+8)//8, (y+23)//8+1)
               for xx in range(x//8, (x+15)//8+1)):
            continue
        queue = deque([(x, y)]); visited[y*497+x] = 1
        while queue:
            x, y = queue.popleft()
            for yy in range((y+8)//8, (y+23)//8+1):
                for xx in range(x//8, (x+15)//8+1):
                    cell = yy*64+xx
                    if attr[cell] == 0 and components[cell] < 0:
                        components[cell] = count
            center = attr[((y+16)//8)*64+(x+8)//8]
            for d, (dx, dy) in enumerate(STEPS):
                nx, ny = x+dx, y+dy
                if not (0 <= nx <= 496 and 0 <= ny <= 487) or visited[ny*497+nx]:
                    continue
                if 0x80 <= center <= 0x8f and (center & 1) != (d >= 2):
                    continue
                if walk(nx, ny, d):
                    visited[ny*497+nx] = 1; queue.append((nx, ny))
        count += 1
    return components


def support_direction(group, flip=0):
    contacts=[m for m in group['members'] if m['support_contact']]
    body=[m for m in group['members'] if not m['support_contact']]
    d=1
    if contacts and body:
        cx0,cx1=min(m['x'] for m in contacts),max(m['x'] for m in contacts)
        cy0,cy1=min(m['y'] for m in contacts),max(m['y'] for m in contacts)
        bx0,bx1=min(m['x'] for m in body),max(m['x'] for m in body)
        by0,by1=min(m['y'] for m in body),max(m['y'] for m in body)
        if cx0==cx1 and cy0<=by0 and cy1>=by1 and (cx0<bx0 or cx0>bx1):d=2 if cx0<bx0 else 3
        elif cy0==cy1 and cx0<=bx0 and cx1>=bx1 and (cy0<by0 or cy0>by1):d=0 if cy0<by0 else 1
    if d>=2 and flip&1:d=5-d
    if d<2 and flip&2:d=1-d
    return d


def audit(snapshot, attributes, profile, solution, state=None, components=None):
    groups, identities = validate_groups(profile)
    # Dynamic OBJ footprints never claim cells in complete BG room maps.
    sprite_groups = sum(bool(g.get('sprite_assembly')) for g in groups)
    # Keep original profile indexes, as native authoredObstacleGroup uses gi+1.
    background_groups = [(gi, g) for gi, g in enumerate(groups) if not g.get('sprite_assembly')]
    groups = [g for _, g in background_groups]
    identities = {m['tile_hash'] for g in groups for m in g['members']}
    assets = {a['tile_hash']: a.get('height') for a in profile.get('assets', [])}
    floor_ids = {h for name in ('dungeon_floor', 'wall_tops') for h in profile.get('asset_groups', {}).get(name, {}).get('tile_hashes', [])}
    roles = {a['tile_hash']: a.get('tile_role', 'unknown') for a in profile.get('assets', [])}
    for local in assets.values():
        if local is not None and not valid_heights(local, profile.get('schema_version', 1)):
            raise ValueError('invalid authored obstacle local array')
    state = state or solution
    stairs = solution.get('stairs', [])
    plane = state.get('plane', solution.get('plane', 0))
    active = {plane} | ({0, 1} if any(s[3] for s in stairs) else set())
    if components is None:
        xy = state.get('link_xy', solution.get('link_xy'))
        if xy is None:
            raise ValueError('obstacle audit requires pinned Link state')
        components = [movement_components(attributes, p, (plane, *xy), stairs) if p in active else [-1]*4096 for p in range(2)]
    report = dict(groups=len(groups), sprite_groups_excluded=sprite_groups, required_cells=0, solved_cells=0, missing_cells=0,
                  conflicting_cells=0, per_layer=[], placements=[], assemblies=[], issues=[], mismatches=[])
    inventory = {(r['layer'], r['plane'], r['y']*64+r['x']): r for r in solution.get('directional_geometry', [])}
    for layer in (0, 1):
        bases = {(p, y*64+x): b for p, x, y, b in solution.get('bg1_cells_plane_x_y_base' if layer == 0 else 'cells_plane_x_y_base',
                                                             solution.get('cells_plane_x_y_base', [])) if b != 255}
        cells = [snapshot.cell(layer, x, y) for y in range(64) for x in range(64)]
        required = {i for i, c in enumerate(cells) if c.tile_hash in identities}
        candidates, claims, seen = [], {}, set()
        # Missing local metadata does not erase identity denominator or matching
        # shape evidence. It is an explicit whole-footprint source blocker.
        for gi, g in background_groups:
            for flip in range(4):
                for y in range(64):
                    for x in range(64):
                        slots = []
                        for m in g['members']:
                            xx = x + (-m['x'] if flip & 1 else m['x'])
                            yy = y + (-m['y'] if flip & 2 else m['y'])
                            if not (0 <= xx < 64 and 0 <= yy < 64):
                                break
                            i = yy*64+xx; c = cells[i]
                            if c.tile_hash != m['tile_hash'] or not m['tile_hash'].startswith('v1:4bpp:') or c.hflip != (m['hflip'] ^ bool(flip & 1)) or c.vflip != (m['vflip'] ^ bool(flip & 2)):
                                break
                            slots.append((i, m))
                        if len(slots) != len(g['members']):
                            continue
                        signature = gi, tuple(sorted((i, m['z'], m['support_contact'], support_direction(g,flip) if m['support_contact'] else 0) for i, m in slots))
                        if signature in seen:
                            continue
                        seen.add(signature); ci = len(candidates)
                        candidates.append((gi, flip, slots))
                        for i, _ in slots:
                            claims.setdefault(i, set()).add(ci)
        solved, conflicted, covered = set(), set(), set()
        for ci, (gi, flip, slots) in enumerate(candidates):
            positions = [i for i, _ in slots]; covered.update(positions)
            record = dict(layer=layer, group=gi+1, assembly=min(positions)+1, flip=flip,
                          cells=positions, contacts=[], status='solved')
            votes = set()
            if any(claims[i] != {ci} for i in positions):
                record['status'] = 'ambiguous_overlap'; conflicted.update(positions)
            else:
                def identity(c):
                    return c.tile_hash, c.hflip, c.vflip
                def lower_floor(i):
                    c = snapshot.cell(0, i%64, i//64)
                    return (roles.get(c.tile_hash) == 'floor' or (roles.get(c.tile_hash, 'unknown') == 'unknown' and c.tile_hash in floor_ids)) and assets.get(c.tile_hash) == [0]*64
                underpass = layer == 1 and all(lower_floor(i) and identity(snapshot.cell(0, i%64, i//64)) != identity(snapshot.cell(1, i%64, i//64)) for i in positions)
                body_planes = [p for p in (0,) if underpass and
                               any(attributes[p][i] in (1, 2, 0x27) for i in positions) and
                               all(attributes[p][i] in (0, 1, 2, 0x27) and attributes[1-p][i] == 0 for i in positions)]
                body_plane = body_planes[0] if len(body_planes) == 1 else None
                record['body_plane'] = body_plane
                for i, m in slots:
                    if not m['support_contact']:
                        continue
                    d=support_direction(profile['obstacle_groups'][gi],flip)
                    dx,dy=((0,-1),(0,1),(-1,0),(1,0))[d]
                    outside_x,outside_y=i%64+dx,i//64+dy
                    while 0<=outside_x<64 and 0<=outside_y<64 and outside_y*64+outside_x in positions:
                        outside_x+=dx;outside_y+=dy
                    neighbors = [i] + ([outside_y*64+outside_x] if 0 <= outside_x < 64 and 0 <= outside_y < 64 else [])
                    local = assets.get(cells[i].tile_hash)
                    direct_contact = any(components[p][j] >= 0 and (p, j) in bases
                                         for p in active if body_plane is None or p == body_plane for j in neighbors)
                    if not direct_contact and local is not None:
                        for dx in (-1, 1):
                            x = i%64+dx
                            if not 0 <= x < 64:
                                continue
                            edge = 0 if dx < 0 else 7
                            c = cells[i]
                            if not all(local[(7-y if c.vflip else y)*8+(7-edge if c.hflip else edge)] == 0 for y in range(8)):
                                continue
                            j = i+dx
                            if ((roles.get(cells[j].tile_hash) == 'floor' or
                                 (roles.get(cells[j].tile_hash, 'unknown') == 'unknown' and cells[j].tile_hash in floor_ids)) and
                                    assets.get(cells[j].tile_hash) == [0]*64):
                                neighbors.append(j)
                    witnesses = []
                    for p in sorted(active):
                        if body_plane is not None and p != body_plane:
                            continue
                        for j in neighbors:
                            n = components[p][j]; b = bases.get((p, j))
                            if n < 0 and j != i and b is not None:
                                pending, visited, owners = [j], set(), set()
                                while pending:
                                    cell = pending.pop()
                                    if cell in visited:
                                        continue
                                    visited.add(cell)
                                    art = cells[cell]
                                    if (attributes[p][cell] != 0 or art.tile_hash in identities or
                                            not (roles.get(art.tile_hash) == 'floor' or
                                                 (roles.get(art.tile_hash, 'unknown') == 'unknown' and art.tile_hash in floor_ids)) or
                                            assets.get(art.tile_hash) != [0]*64 or bases.get((p, cell)) != b):
                                        continue
                                    owner = components[p][cell]
                                    if owner >= 0:
                                        owners.add(owner)
                                        continue
                                    for dx, dy in STEPS:
                                        x, y = cell%64+dx, cell//64+dy
                                        if 0 <= x < 64 and 0 <= y < 64:
                                            pending.append(y*64+x)
                                n = next(iter(owners)) if len(owners) == 1 else -1
                            if n >= 0 and b is not None:
                                witnesses.append(dict(plane=p, cell=j, component=n, base=b)); votes.add((p, n, b))
                    record['contacts'].append(dict(cell=i, witnesses=witnesses))
                if len(votes) > 1:
                    record['status'] = 'conflicting_support'; conflicted.update(positions)
                elif not votes or any(not c['witnesses'] for c in record['contacts']):
                    record['status'] = 'missing_contact_support'
                else:
                    p, n, b = next(iter(votes)); record.update(plane=p, component=n, supporting_floor_base=b)
                    if any(b+(m['z']*8 if assets.get(m['tile_hash']) is None else 0) == 255 or not -32767 <= b+(m['z']*8 if assets.get(m['tile_hash']) is None else 0) <= 32767 for _, m in slots):
                        record['status'] = 'unrepresentable_offset'; conflicted.update(positions)
                    else:
                        for i, m in slots:
                            authored = assets.get(m['tile_hash']) is not None
                            local = assets[m['tile_hash']] if authored else [yy if cells[i].vflip else 7-yy for yy in range(8) for _ in range(8)]
                            base = b if authored else b+m['z']*8
                            placement = dict(kind='authored_obstacle_group', layer=layer, x=i%64, y=i//64,
                                             planes=[p], floor_base=base, supporting_floor_base=b, member_z_tiles=m['z'],
                                             tile_hash=m['tile_hash'], local_source='authored_asset' if authored else 'placement_obstacle_vertical_fallback', local_height=local, local_min=min(local),
                                             local_max=max(local), effective_min=base+min(local), effective_max=base+max(local),
                                             assembly=record['assembly'], group=gi+1)
                            report['placements'].append(placement); solved.add(i)
            report['assemblies'].append(record)
        for i in sorted(required-solved):
            reasons = sorted({r['status'] for r in report['assemblies'] if r['layer'] == layer and i in r['cells']})
            report['issues'].append(dict(layer=layer, x=i%64, y=i//64, tile_hash=cells[i].tile_hash,
                                         reasons=reasons or ['incomplete_or_unmatched_footprint']))
        expected = {r['y']*64+r['x']: r for r in report['placements'] if r['layer'] == layer}
        for i in sorted(required):
            for p in (0, 1):
                actual = inventory.get((layer, p, i))
                e = expected.get(i); base = e['floor_base'] if e and p in e['planes'] else 255
                if inventory and actual is None:
                    report['mismatches'].append(dict(layer=layer, plane=p, cell=i, reason='missing_runtime_inventory'))
                if actual is not None and (actual.get('base') != base or actual.get('fallback_directions') != 0 or
                                           not actual.get('authored_obstacle') or actual.get('role') != 3):
                    report['mismatches'].append(dict(layer=layer, plane=p, cell=i, expected_base=base, actual=actual))
                if actual is not None and e is not None and (actual.get('authored_obstacle_group') is not None and
                      (actual['authored_obstacle_group'] != e['group'] or actual.get('authored_obstacle_assembly') != e['assembly'] or
                       actual.get('member_z_tiles') != e['member_z_tiles'])):
                    report['mismatches'].append(dict(layer=layer, plane=p, cell=i, reason='runtime_member_ownership'))
                if actual is not None and 'runtime_effective_height' in actual:
                    visible = expected.get(i); offset = visible['floor_base'] if visible else 0
                    local = visible['local_height'] if visible else assets.get(cells[i].tile_hash)
                    if local is not None and (actual['runtime_effective_height'] != [offset+h for h in local] or
                                              not actual.get('runtime_authored_pixels_preserved')):
                        report['mismatches'].append(dict(layer=layer, plane=p, cell=i, reason='runtime_pixel_composition'))
        counts = dict(layer=layer, required=len(required), solved=len(solved), conflicting=len(conflicted),
                      missing=len(required-solved-conflicted), plane_support_denominator=[sum(n >= 0 for n in c) for c in components])
        report['per_layer'].append(counts)
        for key, value in (('required_cells', counts['required']), ('solved_cells', counts['solved']),
                           ('missing_cells', counts['missing']), ('conflicting_cells', counts['conflicting'])):
            report[key] += value
    for layer in (0, 1):
        for i in range(4096):
            actual = inventory.get((layer, 0, i))
            if actual and actual.get('authored_obstacle') and snapshot.cell(layer, i%64, i//64).tile_hash not in identities:
                report['mismatches'].append(dict(layer=layer, cell=i, reason='unexpected_runtime_group_claim'))
    report['success'] = not (report['missing_cells'] or report['conflicting_cells'] or report['mismatches'])
    report['contact_denominator'] = sum(len(r['contacts']) for r in report['assemblies'])
    report['witnessed_contacts'] = sum(bool(c['witnesses']) for r in report['assemblies'] for c in r['contacts'])
    report['movement_scope'] = 'Complete ordinary collision-body components on observed/stair-activated planes; not viewport or pixel00 flood.'
    report['floor_base_scope'] = 'Accepted plane floor bases from the separately validated room solve; obstacle output claims are not inputs.'
    return report
