"""Independent complete-ROM stair datum/consumer audit. No artwork height fill."""

def audit(solution, snapshot, profile, templates):
    assets = {a['tile_hash']: a for a in profile.get('assets', [])}
    contextual = {a['tile_hash'] for a in profile.get('tile_contexts', [])}
    geometry = {(r['layer'], r['plane'], r['x'], r['y']): r
                for r in solution.get('directional_geometry', [])}
    bases = {(p, x, y): b for p, x, y, b in solution.get('cells_plane_x_y_base', [])}
    lower = {(p, x, y): b for p, x, y, b in solution.get('bg1_cells_plane_x_y_base', [])}
    issues, placements, claims = [], [], {}
    def local(cell):
        a = assets.get(cell.tile_hash, {})
        # Alternate sources are intentionally not certified by this capability.
        if cell.tile_hash in contextual or a.get('geometry_by_facing') or a.get('geometry_by_flip'):
            return None
        h = a.get('height')
        return h if isinstance(h, list) and len(h) == 64 else None
    def sample(cell, h, x, y):
        return h[(7-y if cell.vflip else y)*8+(7-x if cell.hflip else x)]
    for a in solution.get('stair_attachments', []):
        if a['conflict']:
            issues.append(dict(reason='unresolved_stair_attachment', **a)); continue
        try:
            layer, origin, flip = a['layer'], a['origin'], a['flip']
            x, y = origin % 64, origin // 64
            assert templates is not None and 0 <= x <= 60 and 1 <= y <= 59
            assert layer in (0, 1) and flip in range(4)
            north = not (flip & 2)
            low = (y+4 if north else y-1)*64+x+1
            high = (y-1 if north else y+4)*64+x+1
            assert a['low'] == low and a['high'] == high
            assert a['low_plane'] in (0, 1) and a['high_plane'] in (0, 1)
            assert a['low_plane'] == a['high_plane'] or (a['low_plane'], a['high_plane']) == (1, 0)
            b = bases if layer else lower
            for end, plane, expected in ((low,a['low_plane'],a['base']), (high,a['high_plane'],a['base']+a['rise'])):
                for dx in (0, 1):
                    cell = snapshot.cell(layer, end % 64 + dx, end // 64)
                    assert local(cell) == [0]*64 and b.get((plane,cell.x,cell.y)) == expected
            cells = []
            for xx in range(4):
                for yy in range(4):
                    cell = snapshot.cell(layer,x+xx,y+yy)
                    off = 0x10c8+2*((3-xx if flip&1 else xx)*4+(3-yy if flip&2 else yy))
                    expected = int.from_bytes(templates[off:off+2], 'little') ^ (0x4000 if flip&1 else 0) ^ (0x8000 if flip&2 else 0)
                    assert cell.word & 0xdfff == expected & 0xdfff and local(cell) is not None
                    cells.append(cell)
            for side in (0, 3):
                heights = []
                for step in range(32):
                    yy = step if north else 31-step
                    cell = snapshot.cell(layer,x+side,y+yy//8); h=local(cell)
                    row = [sample(cell,h,px,yy%8) for px in range(8)]
                    assert len(set(row)) == 1
                    heights.append(row[0])
                assert heights[-1] == 0 and heights[0]+1 == a['rise']
                assert all(0 <= u-v <= 2 for u,v in zip(heights,heights[1:]))
            for cell in cells:
                key=layer,cell.x,cell.y
                assert key not in claims or claims[key] == (origin,a['base'])
                claims[key]=(origin,a['base'])
                for plane in {a['low_plane'],a['high_plane']}:
                    r=geometry[layer,plane,cell.x,cell.y]; h=local(cell)
                    assert r.get('owned_stair') and r['base'] == a['base'] and not r['fallback_directions']
                    assert r.get('runtime_authored_pixels_preserved')
                    assert r.get('runtime_effective_height') == [v+a['base'] for v in h]
                    placements.append(dict(layer=layer,plane=plane,x=cell.x,y=cell.y,
                        floor_base=a['base'],local_source='authored_asset',local_min=min(h),local_max=max(h),
                        effective_min=min(h)+a['base'],effective_max=max(h)+a['base']))
        except (AssertionError, KeyError, TypeError, IndexError):
            issues.append(dict(reason='stair_proof_or_consumer_mismatch', **a))
    for r in geometry.values():
        if r.get('owned_stair') and (r['layer'],r['x'],r['y']) not in claims:
            issues.append(dict(reason='unclaimed_stair_member',layer=r['layer'],x=r['x'],y=r['y']))
    return dict(success=not issues,issues=issues,placements=placements,
                required_plane_members=len(placements),complete_assembly_claims=len(solution.get('stair_attachments', [])))
