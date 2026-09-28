#!/usr/bin/env python3
"""Prototype ALTTP room geometry from a linked reference-engine room export.

The prototype solves stair-connected flat-surface components, extracts
per-pixel wall facings, and proposes wall/door placement offsets from authored
height seams. Unanchored floors and ambiguous walls remain unresolved. Output
is an offline review artifact, not a live lighting profile or a replacement
for Snes9x's room masks.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
from dataclasses import dataclass
import json
from pathlib import Path
import sqlite3
import statistics
import struct
import tomllib
import zlib


SIDE = 64
RAW_ROWS = 224
RAW_WIDTH = 256
RAW_ROW_BYTES = 3 * 5 * 2 + RAW_WIDTH * 3 * 2
RAW_VRAM_OFFSET = 14 + RAW_ROWS * RAW_ROW_BYTES
RAW_BYTES = RAW_VRAM_OFFSET + 0x8000 * 2 + 0x100 * 2
STAIR_IDS = {(1, value) for value in (0x1B, 0x1C, 0x1D)} | {
    (2, value) for value in (0x31, 0x32, 0x33)
}
# The room object's [N]/[S] handler determines the upper landing. Authored
# tread values are a cross-check: tile art can be flipped or reused elsewhere.
STAIR_HIGH_SIDE = {(1, 0x1B): "north", (1, 0x1C): "south",
				   (1, 0x1D): "south", (2, 0x31): "north",
                   (2, 0x32): "north", (2, 0x33): "north"}
# Architectural decorations drawn over a wall. Their own tile hashes may be
# neutral, but the room object ties the whole assembly to a nearby landing.
WALL_OVERLAY_IDS = {(0, 0x05), (0, 0x3A), (1, 0x1E)}
DIRECTIONS = {(1, 0): "east", (-1, 0): "west", (0, 1): "south", (0, -1): "north"}
FACING_VECTORS = {name: vector for vector, name in DIRECTIONS.items()}


@dataclass(frozen=True)
class Cell:
    x: int
    y: int
    layer: int
    word: int
    tile_hash: str
    hflip: bool
    vflip: bool


@dataclass(frozen=True)
class FaceRegion:
    facing: tuple[int, int]
    mask: int


class RoomSnapshot:
    def __init__(self, data: bytes):
        if len(data) != RAW_BYTES or data[:8] != b"ALTPPD1\0":
            raise ValueError("expected a complete ALTPPD1 linked room export")
        self.room, self.viewport_x, self.viewport_y = struct.unpack_from("<HHH", data, 8)
        if self.room >= 320:
            raise ValueError("invalid dungeon room ID")
        self.vram = struct.unpack_from("<32768H", data, RAW_VRAM_OFFSET)
        self.layers = []
        for layer in range(2):
            hscroll, vscroll, map_address, tile_address, flags = struct.unpack_from(
                "<HHHHH", data, 14 + layer * 10
            )
            if not (flags & 3) == 3:
                raise ValueError("geometry export needs a full 64x64 BG map")
            self.layers.append((map_address, tile_address))
        self._hash_cache: dict[int, str] = {}
        self.cells = tuple(self._cells(layer) for layer in range(2))

    @classmethod
    def read(cls, path: Path) -> RoomSnapshot:
        return cls(path.read_bytes())

    def _tile_hash(self, address: int) -> str:
        cached = self._hash_cache.get(address)
        if cached is not None:
            return cached
        value = 14695981039346656037
        for byte in (1, 1, 4, 8, 8):
            value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        for y in range(8):
            lo = self.vram[(address + y) & 0x7FFF]
            hi = self.vram[(address + y + 8) & 0x7FFF]
            for x in range(8):
                bit = 7 - x
                index = ((lo >> bit) & 1) | (((lo >> (bit + 8)) & 1) << 1)
                index |= (((hi >> bit) & 1) << 2) | (((hi >> (bit + 8)) & 1) << 3)
                value = ((value ^ index) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        result = f"v1:4bpp:{value:016x}"
        self._hash_cache[address] = result
        return result

    def _cells(self, layer: int) -> tuple[Cell, ...]:
        map_address, tile_address = self.layers[layer]
        result = []
        for y in range(SIDE):
            for x in range(SIDE):
                address = (map_address + (y >> 5) * 0x800 + (x >> 5) * 0x400 +
                           (y & 31) * 32 + (x & 31)) & 0x7FFF
                word = self.vram[address]
                art_address = (tile_address + (word & 0x3FF) * 16) & 0x7FFF
                result.append(Cell(x, y, layer, word, self._tile_hash(art_address),
                                   bool(word & 0x4000), bool(word & 0x8000)))
        return tuple(result)

    def cell(self, layer: int, x: int, y: int) -> Cell:
        return self.cells[layer][y * SIDE + x]


class RoomProvenance:
    """Last room-loader writer for each BG cell, exported beside ALTPPD1."""

    def __init__(self, data: bytes, snapshot: RoomSnapshot):
        if len(data) < 12 or data[:8] != b"ALTPRV1\0":
            raise ValueError("expected an ALTPRV1 room provenance export")
        room, count = struct.unpack_from("<HH", data, 8)
        expected = 12 + count * 10 + 2 * SIDE * SIDE * 2 * 2
        if room != snapshot.room or len(data) != expected:
            raise ValueError("room provenance does not match the linked room export")
        self.objects = [dict(zip(("phase", "ordinal", "source_offset", "raw", "is_door"),
                                 struct.unpack_from("<5H", data, 12 + index * 10)))
                        for index in range(count)]
        offset = 12 + count * 10
        all_owners = struct.unpack_from(f"<{2 * SIDE * SIDE}H", data, offset)
        self.owners = (all_owners[:SIDE * SIDE], all_owners[SIDE * SIDE:])
        if any(owner > count and owner != 0xffff for owner in all_owners):
            raise ValueError("room provenance contains an unknown writer")
        offset += 2 * SIDE * SIDE * 2
        final_words = struct.unpack_from(f"<{2 * SIDE * SIDE}H", data, offset)
        for layer in range(2):
            if any(cell.word != final_words[layer * SIDE * SIDE + index]
                   for index, cell in enumerate(snapshot.cells[layer])):
                raise ValueError("room tilemap changed after the provenance export")

    @classmethod
    def read(cls, path: Path, snapshot: RoomSnapshot) -> RoomProvenance:
        return cls(path.read_bytes(), snapshot)

    def owner(self, layer: int, x: int, y: int) -> int:
        return self.owners[layer][y * SIDE + x]


def face_regions(normal_xyz: list[int] | tuple[int, ...]) -> tuple[FaceRegion, ...]:
    """Split an authored 8x8 normal layer into cardinal wall-face masks.

    Weak XY tilt and front-facing top pixels are intentionally excluded.
    One corner tile can yield two or more regions, each retaining its pixels.
    """
    if not normal_xyz:
        return ()
    if len(normal_xyz) != 192:
        raise ValueError("a tile normal layer needs 192 bytes")
    masks: dict[tuple[int, int], int] = defaultdict(int)
    for pixel in range(64):
        x, y, z = (normal_xyz[3 * pixel + i] - 128 for i in range(3))
        if max(abs(x), abs(y)) < 48 or max(abs(x), abs(y)) < abs(z) * 0.7:
            continue
        facing = ((1 if x > 0 else -1), 0) if abs(x) >= abs(y) else (
            0, (1 if y > 0 else -1)
        )
        masks[facing] |= 1 << pixel
    return tuple(FaceRegion(facing, mask) for facing, mask in sorted(masks.items()))


def transform_face(region: FaceRegion, hflip: bool, vflip: bool) -> FaceRegion:
    """Apply the BG word's flips to both a face direction and its pixel mask."""
    mask = 0
    for y in range(8):
        for x in range(8):
            if region.mask & (1 << (y * 8 + x)):
                px, py = (7 - x if hflip else x), (7 - y if vflip else y)
                mask |= 1 << (py * 8 + px)
    fx, fy = region.facing
    return FaceRegion((-fx if hflip else fx, -fy if vflip else fy), mask)


def load_tile_semantics(path: Path) -> dict[str, tuple[FaceRegion, ...]]:
    """Read optional canonical face masks; placement flips are applied later."""
    data = json.loads(path.read_text())
    if data.get("format") != "alttp-tile-surface-semantics-v1" or not isinstance(data.get("faces"), dict):
        raise ValueError("invalid tile surface semantics file")
    result = {}
    for tile_hash, entries in data["faces"].items():
        if not tile_hash.startswith("v1:4bpp:") or not isinstance(entries, list) or not entries:
            raise ValueError(f"invalid face entries for {tile_hash}")
        regions, used = [], 0
        for entry in entries:
            facing = FACING_VECTORS.get(entry.get("facing"))
            try:
                mask = int(entry["mask"], 16)
            except (KeyError, TypeError, ValueError) as error:
                raise ValueError(f"invalid face mask for {tile_hash}") from error
            if facing is None or not 0 < mask < 1 << 64 or mask & used:
                raise ValueError(f"invalid or overlapping face region for {tile_hash}")
            used |= mask
            regions.append(FaceRegion(facing, mask))
        result[tile_hash] = tuple(regions)
    return result


def tile_face_regions(tile_hash: str, assets: dict[str, dict],
                      semantics: dict[str, tuple[FaceRegion, ...]]) -> tuple[FaceRegion, ...]:
    return semantics.get(tile_hash) or face_regions(assets.get(tile_hash, {}).get("normal_xyz", []))


def write_semantics_template(path: Path, face_hashes: set[str],
                             assets: dict[str, dict]) -> None:
    faces = {}
    for tile_hash in sorted(face_hashes):
        faces[tile_hash] = [{"facing": DIRECTIONS[region.facing],
                             "mask": f"0x{region.mask:016x}"}
                            for region in face_regions(assets.get(tile_hash, {}).get("normal_xyz", []))]
    path.write_text(json.dumps({"format": "alttp-tile-surface-semantics-v1",
                                "faces": faces}, indent=2) + "\n")


def components(cells: tuple[Cell, ...], hashes: set[str],
               owners: tuple[int, ...] | None = None,
               structural_hashes: set[str] | None = None) -> tuple[list[int], list[dict]]:
    """Find connected flat-art regions without crossing a wall or staircase."""
    ids = [-1] * (SIDE * SIDE)
    found = []
    for start, cell in enumerate(cells):
        if cell.tile_hash not in hashes or ids[start] != -1:
            continue
        component_id = len(found)
        ids[start] = component_id
        queue = deque([start])
        count = 0
        bounds = [SIDE, SIDE, 0, 0]
        while queue:
            index = queue.popleft()
            x, y = index % SIDE, index // SIDE
            count += 1
            bounds = [min(bounds[0], x), min(bounds[1], y),
                      max(bounds[2], x), max(bounds[3], y)]
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if not (0 <= nx < SIDE and 0 <= ny < SIDE):
                    continue
                neighbor = ny * SIDE + nx
                if (ids[neighbor] == -1 and cells[neighbor].tile_hash in hashes and
                    not (owners is not None and structural_hashes and
                         owners[index] != owners[neighbor] and
                         (cells[index].tile_hash in structural_hashes or
                          cells[neighbor].tile_hash in structural_hashes))):
                    ids[neighbor] = component_id
                    queue.append(neighbor)
        found.append({"id": component_id, "tiles": count, "bounds": bounds})
    return ids, found


def room_stairs(atlas: Path, room: int, expected_rom_sha256: str | None = None) -> list[dict]:
    with sqlite3.connect(f"file:{atlas}?mode=ro", uri=True) as connection:
        atlas_hash = connection.execute(
            "SELECT value FROM run_info WHERE key='rom_sha256'"
        ).fetchone()
        if not atlas_hash or (expected_rom_sha256 and atlas_hash[0] != expected_rom_sha256):
            raise ValueError("ROM atlas does not match the selected profile")
        records = (json.loads(row[0]) for row in connection.execute(
            "SELECT data_json FROM world_record WHERE kind='dungeon_objects'"
        ))
        return [record for record in records if record["room_id"] == room and
                (record["subtype"], record["object_id"]) in STAIR_IDS]


def room_object_roles(atlas: Path, room: int,
                      expected_rom_sha256: str | None = None) -> dict[int, tuple[int, int]]:
    """Map layer-1 object ordinals to semantic handler IDs for provenance."""
    with sqlite3.connect(f"file:{atlas}?mode=ro", uri=True) as connection:
        atlas_hash = connection.execute(
            "SELECT value FROM run_info WHERE key='rom_sha256'"
        ).fetchone()
        if not atlas_hash or (expected_rom_sha256 and atlas_hash[0] != expected_rom_sha256):
            raise ValueError("ROM atlas does not match the selected profile")
        records = (json.loads(row[0]) for row in connection.execute(
            "SELECT data_json FROM world_record WHERE kind='dungeon_objects'"
        ))
        return {record["ordinal"]: (record["subtype"], record["object_id"])
                for record in records if record["room_id"] == room and record["layer"] == 1}


def _median_height(profile_assets: dict[str, dict], tile_hash: str) -> float | None:
    height = profile_assets.get(tile_hash, {}).get("height")
    if not height or len(height) != 64:
        return None
    ordered = sorted(height)
    return (ordered[31] + ordered[32]) / 2


def _endpoint(ids: list[int], x: int, y: int, side: str) -> int | None:
    yy = y - 1 if side == "north" else y + 4
    if not 0 <= yy < SIDE:
        return None
    candidates = Counter(ids[yy * SIDE + xx] for xx in (x + 1, x + 2)
                         if 0 <= xx < SIDE and ids[yy * SIDE + xx] >= 0)
    return candidates.most_common(1)[0][0] if candidates else None


def stair_constraints(snapshot: RoomSnapshot, stairs: list[dict], ids: list[int],
                      tread_hashes: set[str], assets: dict[str, dict],
                      rise: int) -> tuple[list[dict], list[dict]]:
    constraints, unresolved = [], []
    for stair in stairs:
        x, y = stair["x"], stair["y"]
        if not (0 <= x <= SIDE - 4 and 0 <= y <= SIDE - 4):
            unresolved.append({"object": stair["id"], "reason": "stair outside tilemap"})
            continue
        north, south = _endpoint(ids, x, y, "north"), _endpoint(ids, x, y, "south")
        def tread(row: int) -> float | None:
            heights = [_median_height(assets, snapshot.cell(1, xx, row).tile_hash)
                       for xx in (x + 1, x + 2)
                       if snapshot.cell(1, xx, row).tile_hash in tread_hashes]
            known = [height for height in heights if height is not None]
            return sum(known) / len(known) if known else None
        top, bottom = tread(y), tread(y + 3)
        if north is None or south is None or north == south or top is None or bottom is None:
            unresolved.append({"object": stair["id"], "reason": "missing distinct floor endpoints or tread heights",
                               "north_component": north, "south_component": south,
                               "north_tread": top, "south_tread": bottom})
            continue
        high_side = STAIR_HIGH_SIDE.get((stair["subtype"], stair["object_id"]))
        art_high_side = "north" if top > bottom else "south"
        if high_side is None or abs(top - bottom) < 4 or high_side != art_high_side:
            unresolved.append({"object": stair["id"], "reason": "stair handler and authored tread slope disagree",
                               "handler_high_side": high_side,
                               "art_high_side": art_high_side,
                               "north_tread": top, "south_tread": bottom})
            continue
        high, low = (north, south) if high_side == "north" else (south, north)
        constraints.append({"object": stair["id"], "at": [x, y],
                            "north_component": north, "south_component": south,
                            "north_tread": top, "south_tread": bottom,
                            "handler_high_side": high_side,
                            "low_component": low, "high_component": high, "rise": rise})
    return constraints, unresolved


def solve_levels(component_count: int, constraints: list[dict]) -> tuple[list[int | None], list[dict]]:
    """Solve component heights, anchoring each stair-connected low end at zero."""
    graph: dict[int, list[tuple[int, int, str]]] = defaultdict(list)
    for item in constraints:
        low, high, rise = item["low_component"], item["high_component"], item["rise"]
        graph[low].append((high, rise, item["object"]))
        graph[high].append((low, -rise, item["object"]))
    levels: list[int | None] = [None] * component_count
    conflicts = []
    visited = set()
    for start in graph:
        if start in visited:
            continue
        relative = {start: 0}
        queue = deque([start])
        affected = set()
        while queue:
            current = queue.popleft()
            visited.add(current)
            for neighbor, delta, source in graph[current]:
                proposed = relative[current] + delta
                if neighbor in relative and relative[neighbor] != proposed:
                    conflicts.append({"object": source, "components": [current, neighbor],
                                      "expected": relative[neighbor], "proposed": proposed})
                    affected.update(relative)
                elif neighbor not in relative:
                    relative[neighbor] = proposed
                    queue.append(neighbor)
        if affected:
            continue
        minimum = min(relative.values())
        for component, level in relative.items():
            levels[component] = level - minimum
    return levels, conflicts


def flat_art_candidates(snapshot: RoomSnapshot, profile: dict,
                        assets: dict[str, dict]) -> tuple[set[str], set[str]]:
    """Start with known flat art, then include frequent zero-height +Z art.

    This is only a proposal for a walkable surface; stair connectivity must
    anchor a component before it gets a height. Object provenance and gameplay
    collision will be needed to reject decorative flat art in later slices.
    """
    declared = set(profile["asset_groups"]["wall_tops"]["tile_hashes"])
    counts = Counter(cell.tile_hash for cell in snapshot.cells[1])
    zero_height = [0] * 64
    front = [128, 128, 255] * 64
    inferred = {tile_hash for tile_hash, count in counts.items()
                if count >= 16 and
                assets.get(tile_hash, {}).get("height") == zero_height and
                assets.get(tile_hash, {}).get("normal_xyz") == front}
    return declared | inferred, inferred - declared


def _placed_heights(cell: Cell, assets: dict[str, dict]) -> list[int] | None:
    canonical = assets.get(cell.tile_hash, {}).get("height")
    if not canonical or len(canonical) != 64:
        return None
    return [canonical[(7 - y if cell.vflip else y) * 8 +
                      (7 - x if cell.hflip else x)]
            for y in range(8) for x in range(8)]


def seam_offset(cell: Cell, neighbor: Cell, dx: int, dy: int,
                floor_level: int, assets: dict[str, dict], rise: int) -> int | None:
    """Offset implied by corresponding pixel heights on a shared tile edge."""
    wall = _placed_heights(cell, assets)
    floor = _placed_heights(neighbor, assets)
    if wall is None or floor is None:
        return None
    differences = []
    for index in range(8):
        wx, wy = ((7, index) if dx == 1 else (0, index) if dx == -1 else
                  (index, 7) if dy == 1 else (index, 0))
        fx, fy = ((0, index) if dx == 1 else (7, index) if dx == -1 else
                  (index, 0) if dy == 1 else (index, 7))
        differences.append(floor_level + floor[fy * 8 + fx] - wall[wy * 8 + wx])
    estimate = statistics.median(differences)
    rounded = round(estimate / rise) * rise
    return rounded if 0 <= rounded <= 255 and abs(estimate - rounded) <= 4 else None


def attachment_proposals(snapshot: RoomSnapshot, provenance: RoomProvenance,
                         floor_rows: list[list[int | None]], groups: dict,
                         assets: dict[str, dict], rise: int,
                         object_roles: dict[int, tuple[int, int]] | None = None) -> list[dict]:
    """Suggest object-scoped offsets without flattening any tile's pixel data.

    These are review candidates. A wall bordering both floors, or an object
    with no solved neighboring surface, has no proposed offset.
    """
    faces = set(groups["wall_faces"]["tile_hashes"])
    tops = set(groups["wall_tops"]["tile_hashes"])
    rails = set(groups["stair_rails"]["tile_hashes"])
    treads = set(groups["stair_treads"]["tile_hashes"])
    owned: dict[int, dict] = {}
    for layer in range(2):
        for cell in snapshot.cells[layer]:
            writer = provenance.owner(layer, cell.x, cell.y)
            if writer in (0, 0xffff):
                continue
            record = provenance.objects[writer - 1]
            item = owned.setdefault(writer, {"writer": writer, "object": record,
                                             "roles": Counter(), "bg_cells": [0, 0],
                                             "bg2_positions": set(),
                                             "neighbor_levels": Counter(),
                                             "seam_offsets": Counter(),
                                             "unmatched_seams": 0})
            item["bg_cells"][layer] += 1
            if layer == 1:
                item["bg2_positions"].add((cell.x, cell.y))
            if cell.tile_hash in faces:
                item["roles"]["wall_face"] += 1
            if cell.tile_hash in tops:
                item["roles"]["wall_top"] += 1
            if cell.tile_hash in rails:
                item["roles"]["stair_rail"] += 1
            if cell.tile_hash in treads:
                item["roles"]["stair_tread"] += 1
            if record["is_door"]:
                item["roles"]["door"] += 1
            if record.get("phase") == 1 and (object_roles or {}).get(record.get("ordinal")) in WALL_OVERLAY_IDS:
                item["roles"]["wall_overlay"] += 1
            if layer != 1:
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                xx, yy = cell.x + dx, cell.y + dy
                if not (0 <= xx < SIDE and 0 <= yy < SIDE and
                        provenance.owner(layer, xx, yy) != writer and
                        floor_rows[yy][xx] is not None):
                    continue
                level = floor_rows[yy][xx]
                item["neighbor_levels"][level] += 1
                if cell.tile_hash not in faces | tops:
                    continue
                offset = seam_offset(cell, snapshot.cell(layer, xx, yy), dx, dy,
                                     level, assets, rise)
                if offset is None:
                    item["unmatched_seams"] += 1
                else:
                    item["seam_offsets"][offset] += 1
    result = []
    for item in owned.values():
        roles = item["roles"]
        if not (roles["wall_face"] or roles["door"] or roles["stair_rail"] or
                roles["wall_overlay"]):
            continue
        adjacent = item["neighbor_levels"]
        seams = item["seam_offsets"]
        door_distance = None
        if (roles["door"] or roles["wall_overlay"]) and not seams and not adjacent:
            # Door thresholds are commonly two cells from the room's flat
            # artwork, with a door jamb between them. This is a spatial clue
            # only; it stays a review proposal until the wall is solved.
            for distance in range(1, 4):
                nearby = Counter()
                for x, y in item["bg2_positions"]:
                    for dx in range(-distance, distance + 1):
                        dy = distance - abs(dx)
                        for yy in ({y - dy, y + dy} if dy else {y}):
                            xx = x + dx
                            if (0 <= xx < SIDE and 0 <= yy < SIDE and
                                (xx, yy) not in item["bg2_positions"] and
                                floor_rows[yy][xx] is not None):
                                nearby[floor_rows[yy][xx]] += 1
                if nearby:
                    adjacent = nearby
                    door_distance = distance
                    break
        if roles["stair_tread"] or roles["stair_rail"]:
            status, offset = "transition", None
        elif len(seams) == 1:
            status, offset = "seam_supported", next(iter(seams))
        elif seams:
            status, offset = "conflicting_seam_offsets", None
        elif roles["door"] and len(adjacent) == 1:
            status, offset = "nearest_door_threshold", next(iter(adjacent))
        elif roles["wall_overlay"] and len(adjacent) == 1:
            status, offset = "wall_overlay_floor", next(iter(adjacent))
        elif adjacent:
            status, offset = "unmatched_floor_contact", None
        else:
            status, offset = "no_solved_neighbor", None
        result.append({"writer": item["writer"], "object": item["object"],
                       "roles": dict(roles), "bg_cells": item["bg_cells"],
                       "neighbor_levels": dict(sorted(adjacent.items())),
                       "seam_offsets": dict(sorted(seams.items())),
                       "unmatched_seams": item["unmatched_seams"],
                       "nearby_floor_distance": door_distance,
                       "door_floor_distance": door_distance if roles["door"] else None,
                       "status": status, "proposed_height_offset": offset,
                       "applied_to_live_profile": False})
    return sorted(result, key=lambda item: item["writer"])


def geometry_report(snapshot: RoomSnapshot, profile: dict, stairs: list[dict],
                    provenance: RoomProvenance | None = None,
                    semantics: dict[str, tuple[FaceRegion, ...]] | None = None,
                    object_roles: dict[int, tuple[int, int]] | None = None) -> dict:
    groups = profile["asset_groups"]
    assets = {asset["tile_hash"]: asset for asset in profile["assets"]}
    flat_hashes, inferred_flat = flat_art_candidates(snapshot, profile, assets)
    tread_hashes = set(groups["stair_treads"]["tile_hashes"])
    face_hashes = set(groups["wall_faces"]["tile_hashes"])
    semantics = semantics or {}
    structural_hashes = set(groups["wall_tops"]["tile_hashes"])
    ids, found = components(snapshot.cells[1], flat_hashes,
                            provenance.owners[1] if provenance else None,
                            structural_hashes if provenance else None)
    rise = int(profile.get("lighting_space", {}).get("upper_floor_height", 50))
    if not 0 < rise <= 255:
        raise ValueError("profile needs a positive upper-floor rise")
    constraints, unresolved = stair_constraints(snapshot, stairs, ids, tread_hashes, assets, rise)
    levels, conflicts = solve_levels(len(found), constraints)
    for component in found:
        component["height"] = levels[component["id"]]
        if provenance:
            writers = Counter(provenance.owners[1][index] for index, value in enumerate(ids)
                              if value == component["id"])
            component["source_writers"] = dict(sorted(writers.items()))
    facings = Counter()
    wall_cells = 0
    corner_cells = 0
    for cell in snapshot.cells[1]:
        if cell.tile_hash not in face_hashes:
            continue
        wall_cells += 1
        regions = tile_face_regions(cell.tile_hash, assets, semantics)
        transformed = [transform_face(region, cell.hflip, cell.vflip) for region in regions]
        facings.update(DIRECTIONS[region.facing] for region in transformed)
        corner_cells += len(transformed) > 1
    level_rows = [[levels[ids[y * SIDE + x]] if ids[y * SIDE + x] >= 0 else None
                   for x in range(SIDE)] for y in range(SIDE)]
    proposals = attachment_proposals(snapshot, provenance, level_rows, groups,
                                     assets, rise, object_roles) if provenance else []
    proposal_rows = [row.copy() for row in level_rows]
    proposed_writers = {item["writer"]: item["proposed_height_offset"] for item in proposals
                        if item["proposed_height_offset"] is not None}
    if provenance:
        for cell in snapshot.cells[1]:
            if proposal_rows[cell.y][cell.x] is not None or cell.tile_hash in tread_hashes:
                continue
            writer = provenance.owner(1, cell.x, cell.y)
            if writer in proposed_writers:
                proposal_rows[cell.y][cell.x] = proposed_writers[writer]
    return {"format": "alttp-room-geometry-prototype-v2", "room_id": snapshot.room,
            "tilemap_side": SIDE, "source": "linked room BG2 tilemap plus ROM stair records",
            "height_reference": "relative to the lowest stair-connected flat component",
            "limits": ["Flat-surface candidates combine authored wall_tops with frequent zero-height +Z art.",
                       "Only stair-connected flat components receive a height.",
                       "Wall/door attachment offsets are review proposals and are not applied.",
                       "Collision attributes are not yet exported.",
                       "Stair art can be reused in reversed placements; its authored ramp is provisional direction evidence."],
            "provenance": ({"available": True, "object_count": len(provenance.objects),
                            "untraced_cells": sum(owner == 0xffff for layer in provenance.owners
                                                  for owner in layer),
                            "objects": provenance.objects,
                            "bg2_writer_rows": [list(provenance.owners[1][y * SIDE:(y + 1) * SIDE])
                                                for y in range(SIDE)],
                            "structural_art_writer_boundaries_blocked": True}
                           if provenance else {"available": False,
                                               "structural_art_writer_boundaries_blocked": False}),
            "preview_legend": {
                "blue": "flat component at relative height 0",
                "green": "flat component above height 0",
                "gold": "flat art without a solved height",
                "orange": "stair artwork",
                "red": "east-facing wall pixels",
                "purple": "west-facing wall pixels",
                "olive": "south-facing wall pixels",
                "teal": "north-facing wall pixels",
                "gray": "wall pixels without a classified facing",
            },
            "flat_art_hashes": sorted(flat_hashes),
            "inferred_flat_art_hashes": sorted(inferred_flat),
            "components": found, "stair_constraints": constraints,
            "unresolved_stairs": unresolved, "conflicts": conflicts,
            "wall_face_cells": wall_cells, "corner_face_cells": corner_cells,
            "wall_orientation_overrides": sorted(set(semantics) & face_hashes),
            "classified_wall_facings": dict(facings), "floor_height_rows": level_rows,
            "attachment_proposals": proposals,
            "suggested_bg2_offset_rows": proposal_rows,
            "source_capture_uses_proposals": False}


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))


def write_preview(path: Path, report: dict, snapshot: RoomSnapshot, profile: dict,
                  semantics: dict[str, tuple[FaceRegion, ...]] | None = None) -> None:
    """Write a tile-level review map; no ROM artwork is copied into the PNG."""
    groups = profile["asset_groups"]
    face_hashes = set(groups["wall_faces"]["tile_hashes"])
    stair_hashes = set(groups["stair_treads"]["tile_hashes"]) | set(
        groups["stair_rails"]["tile_hashes"])
    assets = {asset["tile_hash"]: asset for asset in profile["assets"]}
    semantics = semantics or {}
    flat_hashes, _ = flat_art_candidates(snapshot, profile, assets)
    face_cache: dict[tuple[str, bool, bool], tuple[FaceRegion, ...]] = {}
    face_colors = {(1, 0): (194, 101, 110), (-1, 0): (130, 98, 191),
                   (0, 1): (120, 147, 91), (0, -1): (86, 162, 173)}
    pixels = bytearray()
    for pixel_y in range(512):
        y = pixel_y // 8
        for pixel_x in range(512):
            x = pixel_x // 8
            cell = snapshot.cell(1, x, y)
            level = report["floor_height_rows"][y][x]
            if level is not None:
                rgb = (66, 171, 107) if level else (80, 125, 198)
            elif cell.tile_hash in flat_hashes:
                rgb = (191, 154, 64)
            elif cell.tile_hash in stair_hashes:
                rgb = (218, 134, 87)
            elif cell.tile_hash in face_hashes:
                key = cell.tile_hash, cell.hflip, cell.vflip
                if key not in face_cache:
                    canonical = tile_face_regions(cell.tile_hash, assets, semantics)
                    face_cache[key] = tuple(transform_face(region, cell.hflip, cell.vflip)
                                            for region in canonical)
                px, py = pixel_x & 7, pixel_y & 7
                rgb = (96, 99, 112)
                for region in face_cache[key]:
                    if region.mask & (1 << (py * 8 + px)):
                        rgb = face_colors[region.facing]
                        break
            else:
                rgb = (34, 39, 46)
            pixels.extend(rgb)
    rows = b"".join(b"\0" + pixels[y * 512 * 3:(y + 1) * 512 * 3]
                    for y in range(512))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     _png_chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0)) +
                     _png_chunk(b"IDAT", zlib.compress(rows, 8)) + _png_chunk(b"IEND", b""))


def write_offset_preview(path: Path, report: dict) -> None:
    """A data-only map of floor and attachment offsets proposed for BG2."""
    floors = report["floor_height_rows"]
    proposals = report["suggested_bg2_offset_rows"]
    pixels = bytearray()
    for y in range(SIDE):
        for _ in range(8):
            for x in range(SIDE):
                offset = proposals[y][x]
                if offset is None:
                    color = (34, 39, 46)
                elif floors[y][x] is not None:
                    color = (66, 171, 107) if offset else (80, 125, 198)
                else:
                    color = (133, 215, 146) if offset else (88, 104, 152)
                pixels.extend(color * 8)
    rows = b"".join(b"\0" + pixels[y * 512 * 3:(y + 1) * 512 * 3]
                    for y in range(512))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     _png_chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0)) +
                     _png_chunk(b"IDAT", zlib.compress(rows, 8)) + _png_chunk(b"IEND", b""))


def write_height_map(path: Path, report: dict) -> None:
    """Compact BG2 placement offsets for a proposed inspection capture."""
    rows = report["suggested_bg2_offset_rows"]
    if len(rows) != SIDE or any(len(row) != SIDE for row in rows):
        raise ValueError("height proposal map is not 64 by 64 cells")
    values = []
    for row in rows:
        for value in row:
            if value is not None and not 0 <= value < 255:
                raise ValueError("height proposal offset is outside byte range")
            values.append(255 if value is None else value)
    path.write_bytes(b"ALTPHM1\0" + struct.pack("<H", report["room_id"]) + bytes(values))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--atlas", required=True, type=Path)
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--provenance", type=Path,
                        help="ALTPRV1 object-writer export; defaults to RAW.prov")
    parser.add_argument("--semantics", type=Path,
                        help="optional editable per-tile wall face masks and orientations")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    snapshot = RoomSnapshot.read(args.raw)
    provenance_path = args.provenance or Path(str(args.raw) + ".prov")
    if not provenance_path.is_file():
        raise ValueError("geometry reconstruction requires the renderer's ALTPRV1 provenance export")
    provenance = RoomProvenance.read(provenance_path, snapshot)
    with args.profile.open("rb") as source:
        profile = tomllib.load(source)
    semantics = load_tile_semantics(args.semantics) if args.semantics else {}
    stairs = room_stairs(args.atlas, snapshot.room,
                         profile.get("game", {}).get("rom_sha256"))
    report = geometry_report(snapshot, profile, stairs, provenance, semantics,
                             room_object_roles(args.atlas, snapshot.room,
                                               profile.get("game", {}).get("rom_sha256")))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "geometry.json").write_text(json.dumps(report, indent=2) + "\n")
    write_preview(args.output / "geometry.png", report, snapshot, profile, semantics)
    write_offset_preview(args.output / "height-proposals.png", report)
    write_height_map(args.output / "height-proposals.bin", report)
    write_semantics_template(args.output / "tile-semantics-template.json",
                             set(profile["asset_groups"]["wall_faces"]["tile_hashes"]),
                             {asset["tile_hash"]: asset for asset in profile["assets"]})
    solved = sum(component["tiles"] for component in report["components"]
                 if component["height"] is not None)
    print(f"room 0x{snapshot.room:03x}: {len(report['components'])} flat components; "
          f"{solved} tiles assigned; {len(report['stair_constraints'])} stair constraints; "
          f"{report['wall_face_cells']} wall-face tiles inventoried; "
          f"{len(report['attachment_proposals'])} attachment proposals; output {args.output}")


if __name__ == "__main__":
    main()
