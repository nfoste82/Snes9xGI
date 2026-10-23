"""Independent complete final-word ownership for freestanding ROM obstacles."""

def obstacle_assemblies(snapshot, profile, templates, layer=1):
    if templates is None:
        return set()
    assets = {a['tile_hash']: a for a in profile.get('assets', [])}
    groups = profile.get('asset_groups', {})
    forbidden = set()
    for name in ('wall_faces', 'wall_tops', 'dungeon_floor', 'stair_treads', 'stair_rails'):
        forbidden.update(groups.get(name, {}).get('tile_hashes', []))
    obstacles = set(groups.get('obstacles', {}).get('tile_hashes', []))
    claims = {}
    for off, h in ((0xe26, 3), (0xed2, 3), (0xf5e, 3), (0x84a, 4), (0x85a, 4)):
        for flip in range(4):
            expected = [int.from_bytes(templates[off+((1-xx if flip&1 else xx)*h+(h-1-yy if flip&2 else yy))*2:
                        off+((1-xx if flip&1 else xx)*h+(h-1-yy if flip&2 else yy))*2+2], 'little') ^
                        (0x4000 if flip&1 else 0) ^ (0x8000 if flip&2 else 0)
                        for xx in range(2) for yy in range(h)]
            if len(set(expected)) < 2:
                continue
            for y in range(65-h):
                for x in range(63):
                    cells = [snapshot.cell(layer, x+xx, y+yy) for xx in range(2) for yy in range(h)]
                    def allowed(c):
                        a = assets.get(c.tile_hash, {})
                        role = a.get('tile_role', 'unknown')
                        return len(a.get('height', [])) == 64 and (role == 'obstacle' or
                            role == 'unknown' and (c.tile_hash in obstacles or c.tile_hash not in forbidden))
                    if all(c.word & 0xdfff == w & 0xdfff and allowed(c) for c, w in zip(cells, expected)):
                        owner = (x, y, h)
                        for c in cells:
                            claims.setdefault((c.x, c.y), set()).add(owner)
    result = set()
    for owners in claims.values():
        if len(owners) != 1:
            continue
        x, y, h = next(iter(owners))
        cells = {(x+xx, y+yy) for xx in range(2) for yy in range(h)}
        if all(claims[c] == {(x, y, h)} for c in cells):
            result.update(cells)
    return result
