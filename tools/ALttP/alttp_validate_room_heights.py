#!/usr/bin/env python3
"""Audit effective, placement-local ALTTP room heights over a complete room."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path
import struct
import tomllib

from alttp_room_geometry import RoomSnapshot
from alttp_collision_reachability import read_attributes

SIZE = 64
UNKNOWN = 255
DIRS = ((0, -1, "north"), (0, 1, "south"), (-1, 0, "west"), (1, 0, "east"))


def load_height_map(path: Path, room: int) -> list[int] | dict:
    data = path.read_bytes()
    v1 = len(data) == 4106 and data[:8] == b"ALTPHM1\0"
    v2 = len(data) == 12298 and data[:8] == b"ALTPHM2\0"
    v3 = len(data) == 49162 and data[:8] == b"ALTPHM3\0"
    if (not (v1 or v2 or v3) or
            struct.unpack_from("<H", data, 8)[0] != room):
        raise ValueError("height map is not ALTPHM1/2/3 for the requested room")
    if v3:
        values = list(struct.unpack_from("<12288i", data, 10))
        if any(not -32767 <= v <= 32767 for v in values):
            raise ValueError("signed height map base out of range")
        return {"visible": values[:4096], "planes": [values[4096:8192], values[8192:]]}
    if v2:
        return {"visible": list(data[10:4106]), "planes": [list(data[4106:8202]), list(data[8202:12298])]}
    return list(data[10:])


def _fnv(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f"{value:x}"


def _profile(profile: dict) -> tuple[dict[str, list[int]], set[str]]:
    assets = {}
    for asset in profile.get("assets", []):
        tile_hash, height = asset.get("tile_hash"), asset.get("height")
        if tile_hash in assets:
            raise ValueError("profile contains duplicate tile hashes")
        if height is not None:
            if not isinstance(height, list) or len(height) != 64 or not all(
                    isinstance(v, int) and 0 <= v <= 255 for v in height):
                raise ValueError(f"asset {tile_hash} has an invalid height array")
            assets[tile_hash] = height
    walls = set(profile.get("asset_groups", {}).get("wall_faces", {}).get("tile_hashes", []))
    walls.update(a['tile_hash'] for a in profile.get('assets', []) if a.get('tile_role') in ('wall', 'wall_corner'))
    return assets, walls


def _ramp(dx: int, dy: int, hflip: bool, vflip: bool) -> list[int]:
    # Identical to assignBorderHeight in remaster/frame.h: the direction is in
    # displayed room space and the serialized instance stores canonical pixels.
    if hflip:
        dx = -dx
    if vflip:
        dy = -dy
    return [x if dx > 0 else 7 - x if dx < 0 else y if dy > 0 else 7 - y
            for y in range(8) for x in range(8)]


def grounded_floor_hashes(profile):
    """Runtime classifies explicit workbench floors even without generated groups.

    Role/group presence alone never supplies local zero geometry or solved base;
    attached_datum still requires both and same-plane graph membership.
    """
    groups=profile.get('asset_groups',{})
    result=set(groups.get('dungeon_floor',{}).get('tile_hashes',[]))
    result.update(groups.get('wall_tops',{}).get('tile_hashes',[]))
    result.update(a['tile_hash'] for a in profile.get('assets',[]) if a.get('tile_role')=='floor')
    # Explicit role overrides a stale generated structural group at runtime.
    result.difference_update(a['tile_hash'] for a in profile.get('assets',[])
                             if a.get('tile_role') in ('wall','wall_corner','obstacle','door'))
    return result


def build_usage(graph: dict, rise: int, snapshot: RoomSnapshot, attributes: tuple[bytes, bytes],
                profile: dict) -> tuple[list[dict], list[dict], list[dict], dict]:
    """Build the mandatory placement-level result for the runtime floor/fallback slice."""
    if graph.get("format") != "alttp-doorway-graph-review-v1":
        raise ValueError("expected alttp-doorway-graph-review-v1")
    planes, elevation = graph.get("planes"), graph.get("elevation")
    if not isinstance(planes, list) or len(planes) != 2 or not isinstance(elevation, dict):
        raise ValueError("graph must contain two planes and an elevation solution")
    assets, wall_hashes = _profile(profile)
    groups = profile.get('asset_groups', {})
    forbidden_template_hashes = set()
    for name in ('dungeon_floor','wall_tops','obstacles','stair_treads','stair_rails'):
        forbidden_template_hashes.update(groups.get(name,{}).get('tile_hashes',[]))
    forbidden_template_hashes -= wall_hashes
    for name in ('obstacles','stair_treads','stair_rails'):
        forbidden_template_hashes.update(groups.get(name,{}).get('tile_hashes',[]))
    forbidden_template_hashes.update(a['tile_hash'] for a in profile.get('assets',[]) if a.get('tile_role') == 'floor')
    template_walls = set()
    template_trims = set()
    template_door_parts = set()
    template_torches = set()
    from alttp_obstacle_assemblies import obstacle_assemblies
    template_obstacles = obstacle_assemblies(snapshot, profile, graph.get('_verified_rom_template_data'))
    template_corners = set()
    complementary_corner_contacts = {}
    template_data = graph.get('_verified_rom_template_data')
    if template_data is not None:
        for offset, w, h, sx, sy in ((0x2e8,2,3,0,0),(0x2f8,2,3,0,1),(0x2c8,3,2,0,0),(0x2d8,3,2,1,0)):
            for y in range(65-h):
                for x in range(65-w):
                    cells = [snapshot.cell(1,x+xx,y+yy) for yy in range(h) for xx in range(w)]
                    exact = all((c.word & 0xdfff) == (int.from_bytes(template_data[offset+((c.x-x)*4+c.y-y+sy if w==2 else (c.y-y)*4+c.x-x+sx)*2:offset+((c.x-x)*4+c.y-y+sy if w==2 else (c.y-y)*4+c.x-x+sx)*2+2], 'little') & 0xdfff) and
                                c.tile_hash in assets and c.tile_hash not in forbidden_template_hashes for c in cells)
                    seed = any(c.tile_hash in wall_hashes for c in cells)
                    if exact and not seed and w == 3:
                        foot_x = x + 3 if offset == 0x2c8 else x - 1
                        face_x = x + 2 if offset == 0x2c8 else x
                        if 0 <= foot_x < 64:
                            for yy in range(h):
                                foot,face = snapshot.cell(1,foot_x,y+yy),snapshot.cell(1,face_x,y+yy)
                                if foot.tile_hash not in wall_hashes:
                                    continue
                                def height(c,xx,k):
                                    local=assets.get(c.tile_hash,[])
                                    return local[(7-k if c.vflip else k)*8+(7-xx if c.hflip else xx)] if len(local)==64 else None
                                ax,bx=(0,7) if offset==0x2c8 else (7,0)
                                samples=[(height(foot,ax,k),height(foot,7-ax,k),height(face,bx,k),height(face,7-bx,k)) for k in range(8)]
                                if all(None not in v and v[0]>v[1] and v[2]<v[3] for v in samples) and len({v[0]-v[2] for v in samples})==1:
                                    seed=True
                    if exact and seed:
                        template_walls.update((c.x,c.y) for c in cells)
        # Independently attest split lower east door writes and preserved high
        # perimeter, including priority-only operation and alternating row phase.
        for offset in (0x2e82,0x2f72):
            for phase in range(2):
                for y in range(1,59):
                    for x in range(2,61):
                        if not snapshot.cells[0]:
                            continue
                        exact=all(snapshot.cell(1 if col==2 else 0,x-2+col,y+row).word==int.from_bytes(template_data[offset+(col*4+row)*2:offset+(col*4+row)*2+2],'little') for col in range(3) for row in range(4))
                        cells=[snapshot.cell(1,x+col,y+row) for row in range(-1,5) for col in range(1,4)]
                        if exact and all((c.word&0xdfff)==(int.from_bytes(template_data[0x2d8+(((c.y-y+phase+2)%2)*4+c.x-x)*2:0x2da+(((c.y-y+phase+2)%2)*4+c.x-x)*2],'little')&0xdfff) and c.tile_hash in assets and c.tile_hash not in forbidden_template_hashes for c in cells):
                            template_walls.update((c.x,c.y) for c in cells)
                            template_door_parts.update((x,y+row) for row in range(4))
        for offset, vertical, start in ((0x2e8,True,0),(0x2f8,True,1),(0x2c8,False,0),(0x2d8,False,1)):
            for column in range(2):
                words = [int.from_bytes(template_data[offset+(column*4+start+k)*2:offset+(column*4+start+k)*2+2],'little') for k in range(3)]
                for y in range(62 if vertical else 64):
                    for x in range(64 if vertical else 62):
                        cells = [snapshot.cell(1,x+(0 if vertical else k),y+(k if vertical else 0)) for k in range(3)]
                        if (all(c.word==word and c.tile_hash in assets and c.tile_hash not in forbidden_template_hashes for c,word in zip(cells,words)) and
                            all(c.tile_hash in wall_hashes or (c.x,c.y) in template_walls for c in (cells[0],cells[2]))):
                            template_walls.update((c.x,c.y) for c in cells)
        for offset in range(0xb66, 0xd47, 0x20):
            for column in range(4):
                for start in range(2):
                    words = [int.from_bytes(template_data[offset+(column*4+start+k)*2:offset+(column*4+start+k)*2+2], 'little') for k in range(3)]
                    for y in range(62):
                        for x in range(64):
                            cells = [snapshot.cell(1,x,y+k) for k in range(3)]
                            if (all(c.word == word and c.tile_hash in assets and c.tile_hash not in forbidden_template_hashes for c, word in zip(cells,words)) and
                                all(c.tile_hash in wall_hashes or (c.x,c.y) in template_walls for c in (cells[0],cells[2]))):
                                template_walls.update((c.x,c.y) for c in cells)
        # Complete handler-emitted freestanding columns, including end rows.
        # Independent final-word proof; runtime assertions are not evidence.
        for offset in (0x89a,0x8b2,0xa54,0xa6c,0xa84,0xa9c):
            for y in range(61):
                for x in range(62):
                    cells=[snapshot.cell(1,x+xx,y+yy) for xx in range(3) for yy in range(4)]
                    words=[int.from_bytes(template_data[offset+n*2:offset+n*2+2],'little') for n in range(12)]
                    if (all(c.word==w and len(assets.get(c.tile_hash,[]))==64 and c.tile_hash not in forbidden_template_hashes and
                            (c.tile_hash in wall_hashes or min(assets[c.tile_hash])!=max(assets[c.tile_hash])) for c,w in zip(cells,words)) and
                        any(c.tile_hash in wall_hashes for c in cells)):
                        template_walls.update((c.x,c.y) for c in cells)
        # Low-corner triangle and its two surviving tangent branches. The
        # counterpart witnesses ownership only, never a placement base.
        for kind,offset in enumerate((0xbe6,0xc06,0xc26,0xc46)):
            for y in range(61):
                for x in range(61):
                    cells=[];original=[True,True];branches=[]
                    for yy in range(3):
                        for xx in range(4-yy):
                            tx=3-xx if kind>=2 else xx;ty=3-yy if kind%2 else yy
                            word=int.from_bytes(template_data[offset+(tx*4+ty)*2:offset+(tx*4+ty)*2+2],'little')
                            c=snapshot.cell(1,x+tx,y+ty)
                            original[0]&=c.word==word
                            original[1]&=bool(snapshot.cells[0]) and snapshot.cell(0,x+tx,y+ty).word==word
                            if xx==0 or yy==0:
                                cells.append((c,word))
                                if xx:branches.append((0,c.tile_hash in wall_hashes))
                                if yy:branches.append((1,c.tile_hash in wall_hashes))
                    cx=x+(3 if kind>=2 else 0);cy=y+(3 if kind%2 else 0)
                    corner=snapshot.cell(1,cx,cy)
                    if (any(original) and all(c.word==w and len(assets.get(c.tile_hash,[]))==64 and c.tile_hash not in forbidden_template_hashes and (c.tile_hash in wall_hashes or min(assets[c.tile_hash])!=max(assets[c.tile_hash])) for c,w in cells) and
                        all(any(a==axis and seed for a,seed in branches) for axis in (0,1)) and
                        assets.get(corner.tile_hash)==[max(7-xx,7-yy) for yy in range(8) for xx in range(8)]):
                        template_walls.update((c.x,c.y) for c,w in cells)
                        template_corners.add((cx,cy))
                    # Exact other-BG triangle, one surviving inward branch,
                    # and a separately classified high face own both regions.
                    # Ownership is proved from final words, never a base vote.
                    if original[1] and corner.word==snapshot.cell(0,cx,cy).word and assets.get(corner.tile_hash)==[max(7-xx,7-yy) for yy in range(8) for xx in range(8)]:
                        for axis in (0,1):
                            dx=0 if axis else (-1 if kind>=2 else 1);dy=(-1 if kind%2 else 1) if axis else 0
                            branch=[]
                            for distance in (1,2):
                                bx,by=cx+dx*distance,cy+dy*distance
                                word=int.from_bytes(template_data[offset+((bx-x)*4+by-y)*2:offset+((bx-x)*4+by-y)*2+2],'little')
                                c=snapshot.cell(1,bx,by);branch.append(c)
                                if c.word!=word or c.tile_hash not in wall_hashes or len(assets.get(c.tile_hash,[]))!=64:break
                            else:
                                hx,hy=cx-dx,cy-dy
                                if not (0<=hx<64 and 0<=hy<64):continue
                                high=snapshot.cell(1,hx,hy);hh=assets.get(high.tile_hash,[])
                                if high.tile_hash not in wall_hashes or len(hh)!=64:continue
                                def displayed(c,h,xx,yy):return h[(7-yy if c.vflip else yy)*8+(7-xx if c.hflip else xx)]
                                delta=None;compatible=True
                                for k in range(8):
                                    ex=0 if dx>0 else 7 if dx<0 else k;ey=0 if dy>0 else 7 if dy<0 else k
                                    nx=7 if dx>0 else 0 if dx<0 else k;ny=7 if dy>0 else 0 if dy<0 else k
                                    v=displayed(high,hh,nx,ny)-displayed(corner,assets[corner.tile_hash],ex,ey)
                                    if delta is not None and delta!=v:compatible=False
                                    delta=v
                                    if displayed(high,hh,nx,ny)>=displayed(high,hh,7-nx if dx else k,7-ny if dy else k):compatible=False
                                if compatible:
                                    template_corners.add((cx,cy));template_walls.update((c.x,c.y) for c in branch)
                                    # Missing perpendicular inward contact uses
                                    # this opposite high-axis face below.
                                    complementary_corner_contacts[cx,cy,1-axis]=(hx,hy,dx,dy)
        obstacle_seeds=set(groups.get('obstacles',{}).get('tile_hashes',[]))
        obstacle_seeds.update(a['tile_hash'] for a in profile.get('assets',[]) if a.get('tile_role')=='obstacle')
        for kind,offset in enumerate((0x688,0x694)):
            words=[int.from_bytes(template_data[offset+n*2:offset+n*2+2],'little') for n in range(6)]
            for y in range(2,62):
                for x in range(51):
                    for width in range(14,min(29,64-x)+1):
                        cells=[snapshot.cell(1,x+k,y) for k in range(width)]
                        trims=[snapshot.cell(1,x+k,y+(-1 if kind else 1)) for k in range(width)]
                        expected=[words[k+1 if k<2 else k-width+6 if k>=width-2 else 3] for k in range(width)]
                        if not(all(c.word==w and len(assets.get(c.tile_hash,[]))==64 and c.tile_hash not in forbidden_template_hashes for c,w in zip(cells,expected)) and
                            all(c.word==words[0] and c.tile_hash in obstacle_seeds and c.tile_hash in assets for c in trims)):continue
                        for end in (0,1):
                            cx=x+(width-1 if end else 0);dy=1 if kind else -1;co=0xbe6+(0x40 if end else 0)+(0 if kind else 0x20)
                            branch=[snapshot.cell(1,cx,y+dy*k) for k in (1,2)]
                            expected=[int.from_bytes(template_data[co+((3 if end else 0)*4+(k if kind else 3-k))*2:co+((3 if end else 0)*4+(k if kind else 3-k))*2+2],'little') for k in (1,2)]
                            corner=snapshot.cell(1,cx,y)
                            if (all(c.word==w and c.tile_hash in assets and c.tile_hash not in forbidden_template_hashes and (c.tile_hash in wall_hashes or min(assets[c.tile_hash])!=max(assets[c.tile_hash])) for c,w in zip(branch,expected)) and
                                any(c.tile_hash in wall_hashes for c in branch) and assets.get(corner.tile_hash)==[max(7-xx,7-yy) for yy in range(8) for xx in range(8)]):
                                template_corners.add((cx,y));template_walls.update((c.x,c.y) for c in branch+[corner,cells[width-2 if end else 1]])
        for offset in (0xec2,0xeca):
            for y in range(63):
                for x in range(63):
                    cells=[snapshot.cell(1,x+xx,y+yy) for xx in range(2) for yy in range(2)]
                    words=[int.from_bytes(template_data[offset+n*2:offset+n*2+2],'little') for n in range(4)]
                    if (all(c.word==w and len(assets.get(c.tile_hash,[]))==64 and
                            (c.tile_hash in obstacle_seeds or c.tile_hash not in forbidden_template_hashes|wall_hashes) for c,w in zip(cells,words)) and
                        any(c.tile_hash in obstacle_seeds for c in cells)):
                        template_torches.update((c.x,c.y) for c in cells)
        for rail_kind, offset in enumerate((0x688,0x694)):
            rail = [int.from_bytes(template_data[offset+n*2:offset+n*2+2],'little') for n in range(6)]
            for y in range(63):
                for x in range(61):
                    for middle in range(10,min(25,60-x)+1):
                        width=middle+4
                        cells=[snapshot.cell(1,x+k,y+rail_kind) for k in range(width)]
                        trims=[snapshot.cell(1,x+k,y+1-rail_kind) for k in range(width)]
                        expected=[rail[k+1] if k<2 else rail[k-width+6] if k>=width-2 else rail[3] for k in range(width)]
                        if (all(c.word==word and c.tile_hash in wall_hashes and c.tile_hash in assets for c,word in zip(cells,expected)) and
                            all(c.word==rail[0] and c.tile_hash in assets and c.tile_hash not in forbidden_template_hashes and c.tile_hash not in wall_hashes for c in trims)):
                            template_trims.update((c.x,c.y) for c in trims)
    from alttp_diagonal_preview import diagonal_previews
    preview_geometry = diagonal_previews(snapshot, profile, template_data, owned=graph.get('_verified_diagonal_ownership'))
    template_walls.update(preview_geometry)
    obstacle_hashes = set(profile.get("asset_groups", {}).get("obstacles", {}).get("tile_hashes", []))
    obstacle_hashes.update(a['tile_hash'] for a in profile.get('assets', []) if a.get('tile_role') == 'obstacle')
    walkable: set[tuple[int, int, int]] = set()
    denominators = []
    for plane, record in enumerate(planes):
        cells = record.get("reachable_body_cells")
        if not isinstance(cells, list):
            raise ValueError(f"plane {plane} has no reachable_body_cells denominator")
        for cell in cells:
            if (not isinstance(cell, list) or len(cell) != 2 or
                    not all(isinstance(v, int) for v in cell) or
                    not all(0 <= v < SIZE for v in cell)):
                raise ValueError(f"plane {plane} has an invalid reachable body cell")
            walkable.add((plane, cell[0], cell[1]))
        denominators.append(len({(x, y) for p, x, y in walkable if p == plane}))
    solved = {}
    explicit_bases = elevation.get("cells_plane_x_y_base")
    for cell in explicit_bases if explicit_bases is not None else elevation.get("cells_plane_x_y_level", []):
        if (not isinstance(cell, list) or len(cell) != 4 or
                not all(isinstance(v, int) for v in cell)):
            raise ValueError("elevation contains an invalid solved cell")
        plane, x, y, level = cell
        if plane not in (0, 1) or not (0 <= x < SIZE and 0 <= y < SIZE) or (level < 0 and explicit_bases is None):
            raise ValueError("elevation solved cell is out of range")
        key = plane, x, y
        if key in solved and solved[key] != level:
            raise ValueError("elevation assigns conflicting levels to one support cell")
        solved[key] = level if explicit_bases is not None else level * rise

    placements, missing, conflicts = [], [], []
    for plane, x, y in sorted(walkable):
        cell = snapshot.cell(1, x, y)
        level = solved.get((plane, x, y))
        local = assets.get(cell.tile_hash)
        if level is None or local is None:
            missing.append({"plane": plane, "x": x, "y": y,
                            "reason": "missing_level" if level is None else "missing_local_height",
                            "tile_hash": cell.tile_hash})
            continue
        base = level
        if not -32767 <= base <= 32767 or base == UNKNOWN:
            missing.append({"plane": plane, "x": x, "y": y, "reason": "base_overflow"})
            continue
        placements.append({"kind": "walkable_support", "plane": plane, "x": x, "y": y,
                            "floor_base": base, "tile_hash": cell.tile_hash,
                           "local_source": "authored_asset", "local_min": min(local),
                           "local_max": max(local), "effective_min": base + min(local),
                           "effective_max": base + max(local)})

    # Exact documented one-cell fallback candidate rule. A candidate is emitted
    # only when every support contact agrees on plane-independent base/direction.
    candidates: dict[tuple[int, int], list[tuple[int, int, int, int, int]]] = {}
    for plane, x, y in sorted(walkable):
        level = solved.get((plane, x, y))
        if level is None:
            continue
        for dx, dy, _ in DIRS:
            bx, by = x + dx, y + dy
            if not (0 <= bx < SIZE and 0 <= by < SIZE):
                continue
            index = by * SIZE + bx
            placed = snapshot.cell(1, bx, by)
            # Runtime excludes ordinary occupied support on either plane from
            # identity-less border fallback. Otherwise lower colliders could
            # replace an upper bridge's authored relief (exact008).
            if (0, bx, by) in walkable or (1, bx, by) in walkable:
                continue
            if attributes[plane][index] in (1, 2) and placed.tile_hash not in wall_hashes and (bx,by) not in template_walls and (bx,by) not in template_trims:
                candidates.setdefault((bx, by), []).append((plane, level, dx, dy, y * SIZE + x))
    for (x, y), contacts in sorted(candidates.items(), key=lambda item: (item[0][1], item[0][0])):
        levels = {base for _, base, _, _, _ in contacts}
        directions = sorted({(dx, dy) for _, _, dx, dy, _ in contacts})
        planes_here = sorted({p for p, *_ in contacts})
        if len(levels) != 1 or len(planes_here) != 1:
            conflicts.append({"kind": "fallback", "x": x, "y": y, "planes": planes_here,
                              "contacts": [list(v) for v in contacts],
                              "reason": "different_floor_levels" if len(levels) != 1 else
                                        "cross_plane_support_identity_required"})
            continue
        base = next(iter(levels))
        cell = snapshot.cell(1, x, y)
        if cell.tile_hash in obstacle_hashes or (x,y) in template_obstacles:
            local = assets.get(cell.tile_hash)
            if local is None:
                missing.append({"plane": planes_here[0], "x": x, "y": y,
                                "reason": "missing_obstacle_relief", "tile_hash": cell.tile_hash})
                continue
            placements.append({"kind": "authored_obstacle", "planes": planes_here, "x": x, "y": y,
                               "floor_base": base, "tile_hash": cell.tile_hash,
                               "local_source": "authored_asset", "local_min": min(local), "local_max": max(local),
                               "effective_min": base + min(local), "effective_max": base + max(local)})
            continue
        directional_ramps = [_ramp(dx, dy, cell.hflip, cell.vflip) for dx, dy in directions]
        # Outside/multi-edge corner convention: each contacting edge remains at
        # floor height, so the conservative composed surface is the pixel-wise
        # minimum of all outward ramps.
        ramp = [min(values) for values in zip(*directional_ramps)]
        direction_names = [next(name for xx, yy, name in DIRS if (xx, yy) == direction)
                           for direction in directions]
        placements.append({"kind": "collidable_fallback", "planes": planes_here, "x": x, "y": y,
                           "floor_base": base, "tile_hash": cell.tile_hash,
                           "displayed_directions": direction_names,
                           "composition": "single_ramp" if len(directions) == 1 else
                                          "minimum_outward_ramps",
                           "hflip": cell.hflip, "vflip": cell.vflip,
                           "local_source": "placement_ramp_0_7", "local_height": ramp,
                           "effective_min": base, "effective_max": base + 7})
    # Independently reproduce exact-zero/+Z foot connectivity against the
    # pinned complete maps, collision planes and solved support, not the packet.
    asset_records = {a["tile_hash"]: a for a in profile.get("assets", [])}
    floor_hashes = set(profile.get("asset_groups", {}).get("dungeon_floor", {}).get("tile_hashes", []))
    floor_hashes |= set(profile.get("asset_groups", {}).get("wall_tops", {}).get("tile_hashes", []))
    floor_hashes = grounded_floor_hashes(profile)
    foot_cache, datum_cache = {}, {}
    def foot_pixel(px, py):
        key = px, py
        if key not in foot_cache:
            cell = snapshot.cell(1, px // 8, py // 8)
            a = asset_records.get(cell.tile_hash, {})
            local, normals = a.get("height", []), a.get("normal_xyz", [])
            k = (7-py%8 if cell.vflip else py%8)*8 + (7-px%8 if cell.hflip else px%8)
            foot_cache[key] = ((cell.tile_hash in wall_hashes or (cell.x,cell.y) in template_walls) and len(local) == 64 and max(local) > 0 and
                               len(normals) == 192 and local[k] == 0 and normals[k*3:k*3+3] == [128,128,255])
        return foot_cache[key]
    def attached_datum(plane, px, py):
        key = plane, px, py
        if key in datum_cache:
            return datum_cache[key]
        seen, queue, levels, unknown = {(px, py)}, [(px, py)], set(), False
        for xx, yy in queue:
            for dx, dy, _ in DIRS:
                nx, ny = xx+dx, yy+dy
                if not (0 <= nx < 512 and 0 <= ny < 512):
                    continue
                x, y = nx//8, ny//8
                if foot_pixel(nx, ny) and attributes[plane][y*64+x] != 0:
                    if (nx, ny) not in seen:
                        seen.add((nx, ny)); queue.append((nx, ny))
                elif attributes[plane][y*64+x] == 0:
                    cell = snapshot.cell(1, x, y)
                    if cell.tile_hash in floor_hashes and assets.get(cell.tile_hash) == [0]*64:
                        base = solved.get((plane,x,y)) if (plane,x,y) in walkable else None
                        if base is None: unknown = True
                        else: levels.add(base)
        datum = next(iter(levels)) if len(levels) == 1 and not unknown else None
        for xx, yy in seen: datum_cache[plane,xx,yy] = datum
        return datum
    for record in elevation.get("structural_placements", []):
        x, y, base = record.get("x"), record.get("y"), record.get("floor_base")
        if not all(isinstance(v, int) for v in (x, y, base)) or not (0 <= x < 64 and 0 <= y < 64 and -32767 <= base <= 32767 and base != UNKNOWN):
            raise ValueError("invalid structural placement")
        cell = snapshot.cell(1, x, y)
        if record.get('kind')=='authored_obstacle' and (x,y) in template_torches | template_obstacles:
            local=assets[cell.tile_hash];planes_here=record.get('planes',[])
            if not planes_here or any(p not in (0,1) for p in planes_here):
                raise ValueError('torch requires an independently solved plane')
            for p in planes_here:
                contacts={solved[p,x+dx,y+dy] for dx,dy,_ in DIRS if (p,x+dx,y+dy) in walkable and (p,x+dx,y+dy) in solved}
                if contacts!={base}:raise ValueError('torch lacks unanimous same-plane support')
            placements.append(dict(kind='authored_obstacle',x=x,y=y,planes=planes_here,floor_base=base,
                tile_hash=cell.tile_hash,local_source='authored_asset',local_min=min(local),local_max=max(local),
                effective_min=base+min(local),effective_max=base+max(local),assembly_source=
                'verified_complete_floor_torch' if (x,y) in template_torches else 'verified_complete_freestanding_obstacle'))
            continue
        if record.get('kind') == 'authored_obstacle' and record.get('wall_template_offset') in (0x688,0x694):
            if (x,y) not in template_trims:
                raise ValueError('runtime rail trim lacks verified complete handler2f/30 run')
            local=assets.get(cell.tile_hash)
            planes_here=record.get('planes',[])
            if not planes_here or any(p not in (0,1) for p in planes_here):
                raise ValueError('rail trim requires an independently solved plane')
            for p in planes_here:
                seen={(x,y)};queue=[(x,y)];contacts=set()
                for xx,yy in queue:
                    for dx,dy,_ in DIRS:
                        nx,ny=xx+dx,yy+dy
                        if (nx,ny) in template_trims and (nx,ny) not in seen:
                            seen.add((nx,ny));queue.append((nx,ny))
                        elif (p,nx,ny) in walkable and (p,nx,ny) in solved:
                            contacts.add(solved[p,nx,ny])
                if contacts != {base}:
                    raise ValueError('rail trim base lacks unanimous same-plane support contacts')
            placements.append({'kind':'authored_obstacle','x':x,'y':y,'planes':record.get('planes',[]),
                               'floor_base':base,'tile_hash':cell.tile_hash,'local_source':'authored_asset',
                               'local_min':min(local),'local_max':max(local),'effective_min':base+min(local),
                                'effective_max':base+max(local),'assembly_source':'verified_rom_handler2f_trim' if record.get('wall_template_offset')==0x688 else 'verified_rom_handler30_trim'})
            continue
        if record.get('kind')=='authored_obstacle' and record.get('door_wall_part'):
            if (x,y) not in template_door_parts:
                raise ValueError('door part lacks exact split template operation')
            neighbor=snapshot.cell(1,x+1,y)
            local=assets.get(cell.tile_hash,[]);other=assets.get(neighbor.tile_hash,[])
            face=next((r for r in elevation.get('structural_placements',[]) if r.get('x')==x+1 and r.get('y')==y and r.get('kind')=='authored_wall'),None)
            def sample(c,a,xx,k):return a[(7-k if c.vflip else k)*8+(7-xx if c.hflip else xx)]
            if len(local)!=64 or len(other)!=64 or face is None or any(base+sample(cell,local,7,k)!=face['floor_base']+sample(neighbor,other,0,k) for k in range(8)):
                raise ValueError('door part lacks preserved authored seam')
            placements.append(dict(kind='authored_obstacle',x=x,y=y,planes=record.get('planes',[]),floor_base=base,tile_hash=cell.tile_hash,local_source='authored_asset',local_min=min(local),local_max=max(local),effective_min=base+min(local),effective_max=base+max(local),assembly_source='verified_split_lower_east_door_wall_cover'))
            continue
        if record.get("kind") != "authored_wall":
            continue # Obstacles are independently verified from support contacts above.
        if cell.tile_hash not in wall_hashes and (x,y) not in template_walls:
            raise ValueError("runtime wall is not classified in pinned profile")
        if record.get('terminal_corner'):
            if (x,y) not in template_corners:raise ValueError('corner lacks both surviving ROM branches')
            # Require both transformed face seams, not an offset assertion.
            for dx,dy in ((-1 if cell.hflip else 1,0),(0,-1 if cell.vflip else 1)):
                n=snapshot.cell(1,x+dx,y+dy);a=assets[cell.tile_hash];b=assets.get(n.tile_hash,[])
                neighbor=next((r for r in elevation.get('structural_placements',[]) if r.get('x')==x+dx and r.get('y')==y+dy and r.get('kind')=='authored_wall'),None)
                def sample(c,h,xx,yy):return h[(7-yy if c.vflip else yy)*8+(7-xx if c.hflip else xx)]
                # A missing visible branch base requires the original signed
                # counterpart path. Runtime serializes it independently below.
                if neighbor is None:
                    evidence=record.get('corner_branch_relations',[])
                    neighbor=next((r for r in evidence if r.get('x')==x+dx and r.get('y')==y+dy),None)
                    if neighbor is not None:
                        support=neighbor.get('support',-1);plane=neighbor.get('plane')
                        if not isinstance(support,int) or not 0<=support<8192 or plane not in (0,1):
                            raise ValueError('corner counterpart lacks signed support identity')
                        counterpart=snapshot.cell(0,n.x,n.y)
                        if counterpart.word!=n.word or assets.get(counterpart.tile_hash)!=b:
                            raise ValueError('corner counterpart identity/local mismatch')
                        support_base=solved.get((support//4096,support%64,(support%4096)//64))
                        if support_base is None or support_base+neighbor.get('rise',0)!=neighbor['floor_base']:
                            raise ValueError('corner signed support relation disagrees with reproduced support')
                if neighbor is None or len(b)!=64:
                    axis=0 if dx else 1
                    contact=complementary_corner_contacts.get((x,y,axis))
                    if contact is None:raise ValueError('corner missing independently solved branch')
                    hx,hy,hdx,hdy=contact;high=snapshot.cell(1,hx,hy);hh=assets.get(high.tile_hash,[])
                    high_record=next((r for r in elevation.get('structural_placements',[]) if r.get('x')==hx and r.get('y')==hy and r.get('kind')=='authored_wall'),None)
                    if high_record is None or len(hh)!=64:raise ValueError('corner high contact lacks independently solved base')
                    if not high_record.get('wall_template_offset'):raise ValueError('corner high contact lacks template provenance')
                    accepted_planes=record.get('planes',[])
                    high_planes=high_record.get('planes',[])
                    if not set(accepted_planes)&set(high_planes):raise ValueError('corner high contact crosses structural plane')
                    for plane in set(accepted_planes)&set(high_planes):
                        attr=attributes[plane][hy*64+hx]
                        if attr not in (1,2,0x28,0x2a,4):raise ValueError('corner high contact is nonblocking/unsupported')
                    # Evaluate the EXACT transformed max-ramp at the neighboring
                    # pixel center outside0..7, not its zero clipped derivative.
                    for k in range(8):
                        cx=0 if hdx>0 else 7 if hdx<0 else k;cy=0 if hdy>0 else 7 if hdy<0 else k
                        nx=7 if hdx>0 else 0 if hdx<0 else k;ny=7 if hdy>0 else 0 if hdy<0 else k
                        xx=cx-hdx;yy=cy-hdy
                        canonical_x=7-xx if cell.hflip else xx;canonical_y=7-yy if cell.vflip else yy
                        extended=max(7-canonical_x,7-canonical_y)
                        if base+extended!=high_record['floor_base']+sample(high,hh,nx,ny):raise ValueError('corner high extrapolated seam disagreement')
                    continue
                if any(base+sample(cell,a,(7 if dx>0 else 0) if dx else k,(7 if dy>0 else 0) if dy else k)!=
                       neighbor['floor_base']+sample(n,b,(0 if dx>0 else 7) if dx else k,(0 if dy>0 else 7) if dy else k) for k in range(8)):
                    raise ValueError('corner face seam disagreement')
        local = assets.get(cell.tile_hash)
        if local is None:
            missing.append({"x": x, "y": y, "reason": "missing_wall_local_height"})
            continue
        source = "authored_asset"
        attachment = {}
        if record.get('local_source') == 'placement_rom_diagonal_preview':
            proof = preview_geometry.get((x,y))
            if proof is None or record.get('local_height') != proof['height']:
                raise ValueError('diagonal preview lacks exact ROM ownership/cardinal cross section')
            local = proof['height']
            source = 'placement_rom_diagonal_preview'
            attachment = {'local_height':local, 'hflip':cell.hflip, 'vflip':cell.vflip,
                          'evidence':proof.get('evidence', 'ordered_stepped_ROM_column_both_cardinal_unit_sections'),
                          'preview_only':True}
        elif "local_height" in record or "attached_floor_base" in record:
            floor = record.get("attached_floor_base")
            planes_here = record.get("planes", [])
            if (not isinstance(floor, int) or not 0 <= floor <= 32767 or floor == UNKNOWN or not planes_here or
                    any(p not in (0,1) for p in planes_here)):
                raise ValueError("invalid mixed floor datum/planes")
            contacts = []
            for p in planes_here:
                for yy in range(y*8,y*8+8):
                    for xx in range(x*8,x*8+8):
                        if foot_pixel(xx,yy) and attributes[p][y*64+x] != 0:
                            contacts.append(attached_datum(p,xx,yy))
            if not contacts or any(v != floor for v in contacts):
                raise ValueError("mixed floor datum lacks unambiguous same-plane grounded connectivity "
                                 f"at ({x},{y}) {cell.tile_hash}: asserted_floor={floor}, "
                                 f"planes={planes_here}, reproduced_datums={sorted(set(contacts), key=str)}, "
                                 f"grounded_samples={len(contacts)}")
            expected = [max(h, floor-base) for h in local]
            if record.get("local_height") != expected or record.get("local_source") != "placement_grounded_floor_clamp":
                raise ValueError("mixed floor effective local array does not reproduce clamp")
            local, source = expected, "placement_grounded_floor_clamp"
            attachment = {"attached_floor_base": floor, "local_height": local,
                          "hflip": cell.hflip, "vflip": cell.vflip,
                          "evidence": "exact_authored_zero_up_foot_connected_same_plane_support"}
        placements.append({"kind": "authored_wall", "x": x, "y": y, "floor_base": base,
                            "planes": record.get("planes", []),
                            "tile_hash": cell.tile_hash, "local_source": source, **attachment,
                           "local_min": min(local), "local_max": max(local),
                           "effective_min": base + min(local), "effective_max": base + max(local)})
    meta = {"per_plane_walkable_denominator": denominators,
            "plane_overlap_cells": len({(x, y) for _, x, y in walkable if
                                        (0, x, y) in walkable and (1, x, y) in walkable})}
    return placements, missing, conflicts, meta


def directional_seams(records, snapshot, attributes, profile):
    """Inventory every cardinal wall edge; only compatible structural contacts constrain heights.

    Across a descending raster face a unit step is expected, not equality.
    Tangents and independent occluding planes are explicitly separate scopes.
    No seam test supplies a missing base or invents a connection.
    """
    assets, walls = _profile(profile)
    placed = {}
    for r in records:
        if r['layer'] not in (0,1) or r['plane'] not in (0,1) or not (0<=r['x']<64 and 0<=r['y']<64):
            raise ValueError('directional placement out of range')
        key = r['layer'], r['plane'], r['x'], r['y']
        if key in placed:
            raise ValueError('duplicate directional geometry placement')
        c = snapshot.cell(r['layer'], r['x'], r['y'])
        local = assets.get(c.tile_hash)
        if bool(local is not None) != r['has_local']:
            raise ValueError('directional local source mismatch')
        source = 'authored_asset'
        if 'captured_local_height' in r:
            captured = r['captured_local_height']
            if len(captured) != 64 or not all(isinstance(h,int) and 0<=h<=255 for h in captured):
                raise ValueError('invalid captured effective local source')
            local = captured
            source = r.get('captured_local_source','captured_instance')
        elif r['fallback_directions'] and r['base'] != UNKNOWN:
            ramps = [_ramp(dx,dy,c.hflip,c.vflip) for d,(dx,dy,_) in enumerate(DIRS)
                     if r['fallback_directions'] & (1 << d)]
            local = list(map(min,zip(*ramps)))
            source = 'placement_fallback'
        elif local is not None and r['floor_datum'] != UNKNOWN and r['base'] != UNKNOWN:
            local = [max(h,r['floor_datum']-r['base']) for h in local]
            source = 'placement_grounded_floor_clamp'
        displayed = None if local is None else [local[(7-y if c.vflip else y)*8+(7-x if c.hflip else x)] for y in range(8) for x in range(8)]
        placed[key] = {**r,'tile_hash':c.tile_hash,'local_source':source,'local':displayed}
    def boundary_steps(r, dx, dy):
        local = r['local']
        at = lambda x, y: local[y*8+x]
        model = None
        for maximum in (False, True):
            xs = [(at(x+1,y)-at(x,y), at(x,y), x) for y in range(8) for x in range(7) if at(x+1,y)!=at(x,y)]
            ys = [(at(x,y+1)-at(x,y), at(x,y), y) for y in range(7) for x in range(8) if at(x,y+1)!=at(x,y)]
            if not xs or not ys:
                continue
            sx,hx,x = xs[0]; sy,hy,y = ys[0]
            ax,ay = hx-sx*x,hy-sy*y
            candidate = lambda xx,yy: (max if maximum else min)(ax+sx*xx,ay+sy*yy)
            if all(at(x,y)==candidate(x,y) for y in range(8) for x in range(8)):
                model = candidate
                break
        result = []
        for k in range(8):
            x,y = (7 if dx>0 else 0) if dx else k, (7 if dy>0 else 0) if dy else k
            if r.get('captured_local_source') == 'placement_rom_diagonal_preview':
                c=snapshot.cell(r['layer'],r['x'],r['y'])
                xx=7-(x+dx) if c.hflip else x+dx
                yy=7-(y+dy) if c.vflip else y+dy
                extended=max(r['preview_minimum'],min(r['preview_maximum'],r['preview_intercept']-xx-yy))
                result.append(abs(extended-at(x,y)))
            else:
                result.append(abs(model(x+dx,y+dy)-at(x,y)) if model else abs(at(x,y)-at(x-dx,y-dy)))
        return result
    edges = []
    for key,a in sorted(placed.items()):
        if a['role'] != 2:
            continue
        layer,p,x,y=key
        for dx,dy,name in DIRS:
            b=placed.get((layer,p,x+dx,y+dy))
            rec={'layer':layer,'plane':p,'x':x,'y':y,'tile_hash':a['tile_hash'],
                 'direction':name,'neighbor_xy':[x+dx,y+dy], 'base':a['base'],
                 'local_source':a['local_source']}
            if b is None:
                rec['status']='room_boundary';edges.append(rec);continue
            rec.update(neighbor_hash=b['tile_hash'],neighbor_base=b['base'],neighbor_local_source=b['local_source'])
            if a['local'] is None or b['local'] is None:
                rec['status']='missing_local_source';edges.append(rec);continue
            at=lambda r,xx,yy:r['local'][yy*8+xx]
            ae=[at(a,7 if dx>0 else 0 if dx else k,7 if dy>0 else 0 if dy else k) for k in range(8)]
            ai=[at(a,6 if dx>0 else 1 if dx else k,6 if dy>0 else 1 if dy else k) for k in range(8)]
            be=[at(b,0 if dx>0 else 7 if dx else k,0 if dy>0 else 7 if dy else k) for k in range(8)]
            bi=[at(b,1 if dx>0 else 6 if dx else k,1 if dy>0 else 6 if dy else k) for k in range(8)]
            # An obstacle's authored relief is above its solved support plane,
            # not the structural wall seam. Match runtime's support contact;
            # retain the actual relief samples separately in the audit.
            if b['role'] == 3:
                rec['neighbor_authored_relief_edge'] = be
                rec['neighbor_contact_source'] = 'obstacle_support_plane'
                be = bi = [0]*8
            slope=sum(u-v for u,v in zip(ae,ai))
            other_slope=sum(u-v for u,v in zip(bi,be))
            # Collision00 identifies independent SUPPORT, not absence of a
            # visible structural face. A packet-proven placed-surface edge is
            # audited in visible geometry scope without claiming actor-plane
            # support ownership. Never hide its rendering seam behind attr00.
            visible_contact=(a.get('surface_scope')=='observed_visible_placed_surface' and
                             b.get('surface_scope')=='observed_visible_placed_surface')
            independent=not visible_contact and (attributes[p][y*64+x]==0 or (b['role']==2 and attributes[p][b['y']*64+b['x']]==0))
            rec['scope']='observed_visible_placed_surface' if visible_contact else 'gameplay_plane_structure'
            if independent:
                rec['status']='independent_support_or_occluder'
            elif b['role']==2 and (slope>0)-(slope<0)!=(other_slope>0)-(other_slope<0):
                rec['status']='incompatible_facing_boundary'
            elif b['role'] not in (1,2,3) and not b['fallback_directions']:
                rec['status']='unresolved_contact_ownership'
            elif not slope and b['role']!=2:
                rec['status']='lateral_nonstructural_contact'
            elif a['base']==UNKNOWN or b['base']==UNKNOWN:
                rec['status']='missing_placement_base'
            else:
                steps = boundary_steps(a,dx,dy) if slope>0 else boundary_steps(b,-dx,-dy) if slope<0 and b['role']==2 else [0]*8
                expected = [s if slope>0 else -s for s in steps]
                actual=[b['base']+v-a['base']-u for u,v in zip(ae,be)]
                rec.update(edge_effective=[a['base']+u for u in ae],neighbor_edge_effective=[b['base']+v for v in be],
                           expected_steps=expected,actual_steps=actual)
                rec['status']='continuous' if actual==expected else 'effective_seam_conflict'
            edges.append(rec)
    counts={s:sum(e['status']==s for e in edges) for s in sorted({e['status'] for e in edges})}
    return {'edges':edges,'status_counts':counts,'wall_plane_denominator':sum(r['role']==2 for r in placed.values()),
            'per_layer_plane_wall_denominator':[[sum(r['role']==2 and r['layer']==l and r['plane']==p for r in placed.values()) for p in range(2)] for l in range(2)],
            'edge_denominator':len(edges),'conflicts':counts.get('effective_seam_conflict',0),
            'missing':sum(counts.get(s,0) for s in ('missing_local_source','missing_placement_base','unresolved_contact_ownership'))}


def validate(graph: dict, rise: int, height_map: list[int] | None = None,
             *, snapshot: RoomSnapshot | None = None, attributes: tuple[bytes, bytes] | None = None,
             profile: dict | None = None) -> dict:
    if snapshot is None or attributes is None or profile is None:
        raise ValueError("placement validation requires complete snapshot, collision planes, and profile")
    placements, missing, fallback_conflicts, meta = build_usage(
        graph, rise, snapshot, attributes, profile)
    elevation = graph["elevation"]
    map_missing, map_mismatches = [], []
    if height_map is not None:
        visible_map = height_map["visible"] if isinstance(height_map, dict) else height_map
        expected = {}
        for item in placements:
            index = item["y"] * SIZE + item["x"]
            expected[index] = max(expected.get(index, item["floor_base"]), item["floor_base"])
            if isinstance(height_map, dict):
                for plane in [item["plane"]] if "plane" in item else item.get("planes", []):
                    actual = height_map["planes"][plane][index]
                    if actual != item["floor_base"]:
                        map_mismatches.append([plane, item["x"], item["y"], item["floor_base"], actual])
        for index, expected_value in sorted(expected.items()):
            actual = visible_map[index]
            if actual == UNKNOWN:
                map_missing.append([index % SIZE, index // SIZE, expected_value])
            elif actual != expected_value:
                map_mismatches.append([index % SIZE, index // SIZE, expected_value, actual])
    supports = [p for p in placements if p["kind"] == "walkable_support"]
    fallbacks = [p for p in placements if p["kind"] == "collidable_fallback"]
    conflicts = list(elevation.get("conflicting_components", [])) + fallback_conflicts
    if elevation.get("mixed_floor_conflicts", 0):
        conflicts.append({"kind": "mixed_floor_support", "count": elevation["mixed_floor_conflicts"]})
    if elevation.get("structural_conflicts", 0):
        issues = elevation.get("geometry_component_issues", [])
        conflicts.extend({"kind": "runtime_geometry_component", **issue} for issue in issues)
        accounted = sum(issue.get("nodes", 0) if issue.get("equality_mismatches", 0) else
                        issue.get("negative_base_nodes", 0) + issue.get("overflow_nodes", 0) for issue in issues)
        if not issues or elevation["structural_conflicts"] > accounted:
            conflicts.append({"kind": "runtime_structure", "count": elevation["structural_conflicts"] - accounted})
    unresolved = elevation.get("unresolved_stairs", [])
    unknown_walls = elevation.get("unknown_wall_cells", 0)
    seams = directional_seams(elevation.get('directional_geometry',[]),snapshot,attributes,profile)
    success = (not missing and not conflicts and not unresolved and not map_missing and
               not map_mismatches and not unknown_walls and not elevation.get("mixed_floor_unknown", 0) and
               not seams['conflicts'] and not seams['missing'])
    return {"format": "alttp-room-height-validation-v2", "room_id": graph.get("room_id"),
            "success": success, "rise": rise, **meta, 'directional_wall_continuity':seams,
            "required_walkable_placements": sum(meta["per_plane_walkable_denominator"]),
            "valid_walkable_placements": len(supports), "missing_placements": missing,
            "required_fallback_placements": len(fallbacks) + len(fallback_conflicts),
            "valid_fallback_placements": len(fallbacks), "fallback_conflicts": fallback_conflicts,
             "effective_height_placements": len(placements), "placements": placements,
             "mixed_floor_unknown": elevation.get("mixed_floor_unknown", 0),
             "mixed_floor_conflicts": elevation.get("mixed_floor_conflicts", 0),
             "mixed_floor_placements": elevation.get("mixed_floor_placements", []),
            "conflicting_components": conflicts, "unresolved_stairs": unresolved,
            "compiled_map_checked": height_map is not None,
            "solved_wall_cells": elevation.get("solved_wall_cells", 0),
            "unknown_wall_cells": unknown_walls,
            "unresolved_wall_placements": elevation.get("unresolved_wall_placements", []),
            "constraint_issues": elevation.get("constraint_issues", []),
            "relative_anchor_provenance": elevation.get("relative_anchor_provenance", []),
            "geometry_component_issues": elevation.get("geometry_component_issues", []),
            "issue_placements": elevation.get("issue_placements", []),
            "compiled_map_missing": map_missing, "compiled_map_mismatches": map_mismatches}


def diagnostic_svg(report: dict, path: Path) -> None:
    cells = {(p["x"], p["y"]): "#36b37e" if p["kind"] == "walkable_support" else "#f5a623"
             for p in report["placements"]}
    for p in report["missing_placements"]:
        cells[(p["x"], p["y"])] = "#b146c2"
    for p in report["fallback_conflicts"]:
        cells[(p["x"], p["y"])] = "#e53935"
    for p in report['directional_wall_continuity']['edges']:
        if p['status']=='effective_seam_conflict': cells[p['x'],p['y']]='#e53935'
        elif p['status'] in ('missing_local_source','missing_placement_base','unresolved_contact_ownership'):
            cells[p['x'],p['y']]='#b146c2'
    rects = "".join(f'<rect x="{x*8}" y="{y*8}" width="8" height="8" fill="{color}"/>'
                    for (x, y), color in cells.items())
    legend = html.escape("green=support orange=fallback purple=missing red=conflict")
    path.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="536" '
                    f'viewBox="0 0 512 536"><rect width="512" height="512" fill="#25202b"/>{rects}'
                    f'<text x="4" y="530" fill="white" font-size="12">{legend}</text></svg>\n')


def audit_doorframes(solution, snapshot, profile, template_data):
    """Inventory all marked placements, independently reject template/mask drift.

    Region offsets are not compiled tile-wide map entries. False pixels must
    remain precisely authored; no floor/threshold offset is admissible there.
    """
    assets = {a['tile_hash']: a for a in profile.get('assets', [])}
    required = {}
    for layer in range(2):
        for y in range(64):
            for x in range(64):
                c = snapshot.cell(layer, x, y)
                a = assets.get(c.tile_hash, {})
                if a.get('tile_role') == 'door' and int(a.get('doorframe_mask', '0'), 16):
                    required[layer, x, y] = (c, a)
    accepted, issues = set(), []
    # Independently attest placement-local FULL outer door bands. Interior
    # regions still require authored masks; no hash-wide activation/default.
    if template_data:
        candidates=list(solution.get('doorframe_attachments', []))
        # Enumerate missing parts too: accepted solver records are NOT the
        # denominator. Seed exact footprints with explicit canonical Door art,
        # avoiding sliding closed-wall templates and unmarked stair identities.
        for layer in range(2):
            for d,(start,end) in enumerate(((0x2716,0x2926),(0x29f0,0x2be8),(0x2c6a,0x2e32),(0x2e6a,0x3032))):
                w,h=(4,3) if d<2 else (3,4)
                for off in range(start,end+1,24):
                    first=int.from_bytes(template_data[off:off+2],'little')&0xdfff
                    for oy in range(65-h):
                        for ox in range(65-w):
                            if snapshot.cell(layer,ox,oy).word&0xdfff != first:
                                continue
                            cells=[snapshot.cell(layer,ox+x,oy+y) for x in range(w) for y in range(h)]
                            if not any(assets.get(c.tile_hash,{}).get('tile_role')=='door' for c in cells):
                                continue
                            if not all(c.word&0xdfff==int.from_bytes(template_data[off+2*n:off+2*n+2],'little')&0xdfff for n,c in enumerate(cells)):
                                continue
                            for c in cells:
                                candidates.append(dict(layer=layer,x=c.x,y=c.y,template_offset=off,split_dispatch=False))
        for r in candidates:
            key = r['layer'], r['x'], r['y']
            if key in required:
                continue
            c = snapshot.cell(*key)
            a = assets.get(c.tile_hash, {})
            local = a.get('height', [])
            off = r['template_offset']
            direction = next((d for d,(s,e) in enumerate(((0x2716,0x2926),(0x29f0,0x2be8),(0x2c6a,0x2e32),(0x2e6a,0x3032)))
                              if s <= off <= e and (off-s)%24 == 0), None)
            if direction is None or len(local) != 64 or 'doorframe_mask' in a or a.get('tile_role','unknown') not in ('unknown','door'):
                continue
            occ = a.get('occlusion', [255]*64)
            if len(occ)!=64 or any(o not in (0,255) or (o==0 and local[k]!=0) for k,o in enumerate(occ)):
                continue
            mask = sum(1<<k for k,o in enumerate(occ) if o==255)
            if not mask:
                continue
            minimum = min(v for k,v in enumerate(local) if mask>>k&1)
            if any(mask>>k&1 and v != minimum+7-(k//8 if direction<2 else k%8) for k,v in enumerate(local)):
                continue
            w,h = (4,3) if direction<2 else (3,4)
            owned = False
            for oy in range(max(0,c.y-h+1), min(c.y,64-h)+1):
                for ox in range(max(0,c.x-w+1), min(c.x,64-w)+1):
                    xx,yy = c.x-ox,c.y-oy
                    outer = ((xx in (0,w-1) or (yy==0 if direction==0 else yy==h-1)) if direction<2 else
                             (yy in (0,h-1) or (xx==0 if direction==2 else xx==w-1)))
                    def layer(x,y):
                        return (1 if (y==0 if direction==0 else y==h-1 if direction==1 else x==0 if direction==2 else x==w-1) else 0) if r.get('split_dispatch') else c.layer
                    owned |= outer and layer(xx,yy)==c.layer and all(
                        (snapshot.cell(layer(x,y),ox+x,oy+y).word&0xdfff)==int.from_bytes(template_data[off+2*(x*h+y):off+2*(x*h+y)+2],'little')&0xdfff
                        for x in range(w) for y in range(h))
            if owned:
                required[key] = (c, dict(a, doorframe_mask=f'{mask:016x}'))
    bases = {}
    for r in solution.get('doorframe_attachments', []):
        if not r.get('visible', True):
            continue
        bases.setdefault((r['layer'],r['x'],r['y']), set()).add(r['frame_base'])
    for r in solution.get('doorframe_attachments', []):
        key = r['layer'], r['x'], r['y']
        if key not in required:
            issues.append(dict(reason='unexpected_frame_region', record=r)); continue
        c, a = required[key]
        if r.get('visible', True) and len(bases[key]) != 1:
            issues.append(dict(reason='ambiguous_visible_frame_plane', record=r)); continue
        mask = int(a['doorframe_mask'], 16)
        local = a.get('height', [])
        expected = [v + (r['frame_base'] if mask >> k & 1 else 0) for k, v in enumerate(local)]
        if len(local) != 64 or int(r['mask'], 16) != mask or r['effective_height'] != expected:
            issues.append(dict(reason='frame_mask_or_opening_preservation', record=r)); continue
        off = r['template_offset']
        direction = next((d for d, (start, end) in enumerate(((0x2716,0x2926),(0x29f0,0x2be8),(0x2c6a,0x2e32),(0x2e6a,0x3032)))
                          if start <= off <= end and (off-start) % 24 == 0), None)
        exact = False
        if template_data and direction is not None:
            w, h = (4, 3) if direction < 2 else (3, 4)
            for oy in range(max(0,c.y-h+1), min(c.y,64-h)+1):
                for ox in range(max(0,c.x-w+1), min(c.x,64-w)+1):
                    def owner_layer(xx, yy):
                        if not r.get('split_dispatch', False):
                            return c.layer
                        return 1 if (yy==0 if direction==0 else yy==h-1 if direction==1 else xx==0 if direction==2 else xx==w-1) else 0
                    if owner_layer(c.x-ox,c.y-oy) != c.layer:
                        continue
                    exact |= all((snapshot.cell(owner_layer(xx,yy),ox+xx,oy+yy).word & 0xdfff) ==
                                 (int.from_bytes(template_data[off+2*(xx*h+yy):off+2*(xx*h+yy)+2], 'little') & 0xdfff)
                                 for xx in range(w) for yy in range(h))
        if not exact:
            issues.append(dict(reason='missing_intact_rom_door_template', record=r)); continue
        if r.get('visible', True):
            accepted.add(key)
    return dict(required_marked_placements=len(required), solved_marked_placements=len(accepted),
                missing_marked_placements=[list(k) for k in sorted(required.keys()-accepted)], issues=issues,
                region_source='placement_rom_doorframe_seam', opening_contract='unmarked_pixels_exactly_authored')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--graph", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--attributes", type=Path, required=True,
                        help="complete ALTPAT1 collision arrays matching the snapshot")
    parser.add_argument("--state", type=Path, required=True,
                        help="room traversal state pin (room, entrance, plane, collision mode)")
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--height-map", type=Path)
    parser.add_argument("--rom", type=Path, help="verified USA ROM for exact placed wall templates")
    parser.add_argument("--runtime-solution", type=Path,
                        help="complete-room output of alttp_runtime_room_heights; bases are height units, not levels")
    parser.add_argument('--provenance-snapshot', type=Path,
                        help='complete matched ALTPPD1 reference with sibling ALTPRV1 .prov')
    parser.add_argument('--atlas', type=Path, help='ROM-pinned object dispatch atlas for provenance proof')
    parser.add_argument("--rise", type=int, default=50)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--diagnostic-svg", type=Path)
    args = parser.parse_args()
    try:
        graph = json.loads(args.graph.read_text())
        if args.rom:
            rom = args.rom.read_bytes()
            if hashlib.sha256(rom).hexdigest() != '66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb':
                raise ValueError('unsupported template ROM fingerprint')
            graph['_verified_rom_template_data'] = rom[0x1b52:0x1b52+12876]
        state = json.loads(args.state.read_text())
        room = graph.get("room_id")
        snapshot = RoomSnapshot.read(args.snapshot)
        if snapshot.room != room or state.get("room_id") != room:
            raise ValueError("snapshot/state does not match graph room")
        if state.get("collision_mode") != 0:
            raise ValueError("only collision mode 0 is supported")
        if graph.get("state") is not None and graph["state"] != state:
            raise ValueError("pinned traversal state does not match graph")
        attribute_bytes = args.attributes.read_bytes()
        attributes = read_attributes(attribute_bytes, room)
        expected_digest = graph.get("attribute_sha256")
        if expected_digest and hashlib.sha256(attribute_bytes).hexdigest() != expected_digest:
            raise ValueError("collision arrays do not match graph attribute SHA-256")
        if args.provenance_snapshot or args.atlas:
            if not (args.provenance_snapshot and args.atlas and args.rom):
                raise ValueError('partial diagonal proof requires provenance snapshot, atlas and verified ROM')
            from alttp_room_geometry import RoomProvenance
            from alttp_diagonal_sets import inspect
            import sqlite3
            reference = RoomSnapshot.read(args.provenance_snapshot)
            if reference.room != room or any(reference.cell(l,x,y).word != snapshot.cell(l,x,y).word
                    for l in range(2) for y in range(64) for x in range(64)):
                raise ValueError('provenance complete final maps do not match pinned snapshot')
            provenance = RoomProvenance.read(Path(str(args.provenance_snapshot)+'.prov'), reference)
            with sqlite3.connect(f'file:{args.atlas}?mode=ro', uri=True) as db:
                pin = db.execute("SELECT value FROM run_info WHERE key='rom_sha256'").fetchone()
                if not pin or pin[0] != hashlib.sha256(rom).hexdigest():
                    raise ValueError('provenance atlas ROM mismatch')
                objects = [json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
            objects = {(o['layer'],o['ordinal']):o for o in objects if o['room_id']==room}
            # Writer provenance attests words, not animation graphics. Decode
            # identities from the EXACT pinned live VRAM, never reference art.
            assemblies, ownership = inspect(snapshot, provenance, objects, graph['_verified_rom_template_data'])
            dispatch_ownership = {}
            for assembly in assemblies:
                for placement in assembly['placements']:
                    key=(placement['layer'],placement['x'],placement['y'])
                    original=ownership.get(key,set())
                    if len(original)!=1:continue
                    claim=next(iter(original))
                    dispatch_ownership.setdefault(key,set()).add((*claim,assembly['writer'],assembly['handler'],
                                                                  assembly['template_offset']))
            graph['_verified_diagonal_ownership'] = dispatch_ownership
        with args.profile.open("rb") as source:
            profile = tomllib.load(source)
        if args.runtime_solution:
            solution = json.loads(args.runtime_solution.read_text())
            inventory = solution.get('directional_geometry',[])
            if len(inventory) != 16384 or len({(r['layer'],r['plane'],r['x'],r['y']) for r in inventory}) != 16384:
                raise ValueError('runtime directional geometry requires both complete BG layers and both planes')
            if solution.get("format") != "alttp-runtime-room-heights-v1" or solution.get("room_id") != room:
                raise ValueError("runtime solution room/format mismatch")
            if solution.get("link_xy") != state.get("link_xy") or solution.get("plane") != state.get("plane"):
                raise ValueError("runtime solution seed does not match pinned state")
            for key, source in (("snapshot_fnv1a64", args.snapshot), ("attributes_fnv1a64", args.attributes),
                                ("profile_fnv1a64", args.profile)):
                if solution.get(key) != _fnv(source.read_bytes()):
                    raise ValueError(f"runtime solution {key} does not match pinned input")
            words = b"".join(snapshot.cell(1, x, y).word.to_bytes(2, "little")
                             for y in range(64) for x in range(64))
            if solution.get("bg2_words_fnv1a64") != _fnv(words):
                raise ValueError("runtime decoded BG2 words do not match complete snapshot map")
            from alttp_diagonal_preview import diagonal_previews
            proofs=[diagonal_previews(snapshot,profile,graph.get('_verified_rom_template_data'),layer,
                                     owned=graph.get('_verified_diagonal_ownership')) for layer in range(2)]
            for record in inventory:
                if record.get('captured_local_source')=='placement_rom_diagonal_preview':
                    proof=proofs[record['layer']].get((record['x'],record['y']))
                    if (proof is None or record.get('captured_local_height')!=proof['height'] or
                        record.get('preview_intercept')!=proof['intercept'] or
                        record.get('preview_minimum')!=proof['minimum'] or record.get('preview_maximum')!=proof['maximum']):
                        raise ValueError(f"directional diagonal preview source lacks independent ROM proof: layer={record['layer']} cell=({record['x']},{record['y']})")
            graph["elevation"] = {**graph["elevation"], **solution}
        height_map = load_height_map(args.height_map, room) if args.height_map else None
        report = validate(graph, args.rise, height_map, snapshot=snapshot,
                          attributes=attributes, profile=profile)
        if args.runtime_solution:
            report['diagonal_sources'] = [dict(layer=l,x=x,y=y,**proof)
                                          for l, layer_proofs in enumerate(proofs)
                                          for (x,y),proof in sorted(layer_proofs.items())]
            report['diagonal_source_counts'] = dict(
                proved_placements=len(report['diagonal_sources']),
                partial_owned_tangent=sum(p.get('evidence',{}).get('source')=='partial_semantic_diagonal_owned_tangent'
                                         for layer_proofs in proofs for p in layer_proofs.values()))
            if args.provenance_snapshot:
                report['diagonal_provenance_inputs'] = dict(
                    snapshot=str(args.provenance_snapshot),snapshot_sha256=hashlib.sha256(args.provenance_snapshot.read_bytes()).hexdigest(),
                    provenance_sha256=hashlib.sha256(Path(str(args.provenance_snapshot)+'.prov').read_bytes()).hexdigest(),
                    atlas=str(args.atlas),atlas_sha256=hashlib.sha256(args.atlas.read_bytes()).hexdigest(),
                    graphics_source='exact pinned live snapshot; reference art never substituted',
                    complete_final_words_match=True)
            report['doorframes'] = audit_doorframes(solution, snapshot, profile, graph.get('_verified_rom_template_data'))
            report['persistent_support'] = dict(required=solution.get('persistent_support_required',[0,0]),
                                                missing=solution.get('persistent_support_missing',[0,0]),
                                                placements=solution.get('persistent_support',[]),
                                                scope='potential removed-pot Link bodies; not current movement reachability')
            if report['doorframes']['missing_marked_placements'] or report['doorframes']['issues']:
                report['success'] = False
            if any(report['persistent_support']['missing']):
                report['success'] = False
        if args.rom:
            report['template_evidence']={'rom_sha256':hashlib.sha256(rom).hexdigest(),
                'source_table_snes_address':'80:9b52','perimeter_template_offsets':[0x2c8,0x2d8,0x2e8,0x2f8],
                'corner_template_offsets':list(range(0xb66,0xd47,0x20)),
                'rail_trim_template_offset':0x688,'reversed_rail_trim_template_offset':0x694,
                'recognition':'exact_final_words_and_authored_roles'}
        report["inputs"] = {"graph": str(args.graph), "snapshot": str(args.snapshot),
                            "attributes": str(args.attributes), "state": str(args.state),
                            "profile": str(args.profile),
                            "snapshot_sha256": hashlib.sha256(args.snapshot.read_bytes()).hexdigest(),
                            "attributes_sha256": hashlib.sha256(attribute_bytes).hexdigest()}
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    args.output.write_text(rendered)
    if args.diagnostic_svg:
        diagnostic_svg(report, args.diagnostic_svg)
    print(rendered, end="")
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
