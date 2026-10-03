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
    if (not (v1 or v2) or
            struct.unpack_from("<H", data, 8)[0] != room):
        raise ValueError("height map is not ALTPHM1/2 for the requested room")
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


def build_usage(graph: dict, rise: int, snapshot: RoomSnapshot, attributes: tuple[bytes, bytes],
                profile: dict) -> tuple[list[dict], list[dict], list[dict], dict]:
    """Build the mandatory placement-level result for the runtime floor/fallback slice."""
    if graph.get("format") != "alttp-doorway-graph-review-v1":
        raise ValueError("expected alttp-doorway-graph-review-v1")
    planes, elevation = graph.get("planes"), graph.get("elevation")
    if not isinstance(planes, list) or len(planes) != 2 or not isinstance(elevation, dict):
        raise ValueError("graph must contain two planes and an elevation solution")
    assets, wall_hashes = _profile(profile)
    obstacle_hashes = set(profile.get("asset_groups", {}).get("obstacles", {}).get("tile_hashes", []))
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
        if plane not in (0, 1) or not (0 <= x < SIZE and 0 <= y < SIZE) or level < 0:
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
        if base > 254:
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
            if attributes[plane][index] in (1, 2) and placed.tile_hash not in wall_hashes:
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
        if cell.tile_hash in obstacle_hashes:
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
    for record in elevation.get("structural_placements", []):
        x, y, base = record.get("x"), record.get("y"), record.get("floor_base")
        if not all(isinstance(v, int) for v in (x, y, base)) or not (0 <= x < 64 and 0 <= y < 64 and 0 <= base < 255):
            raise ValueError("invalid structural placement")
        cell = snapshot.cell(1, x, y)
        if record.get("kind") != "authored_wall":
            continue # Obstacles are independently verified from support contacts above.
        if cell.tile_hash not in wall_hashes:
            raise ValueError("runtime wall is not classified in pinned profile")
        local = assets.get(cell.tile_hash)
        if local is None:
            missing.append({"x": x, "y": y, "reason": "missing_wall_local_height"})
            continue
        placements.append({"kind": "authored_wall", "x": x, "y": y, "floor_base": base,
                           "planes": record.get("planes", []),
                           "tile_hash": cell.tile_hash, "local_source": "authored_asset",
                           "local_min": min(local), "local_max": max(local),
                           "effective_min": base + min(local), "effective_max": base + max(local)})
    meta = {"per_plane_walkable_denominator": denominators,
            "plane_overlap_cells": len({(x, y) for _, x, y in walkable if
                                        (0, x, y) in walkable and (1, x, y) in walkable})}
    return placements, missing, conflicts, meta


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
            expected[index] = max(expected.get(index, 0), item["floor_base"])
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
    if elevation.get("structural_conflicts", 0):
        issues = elevation.get("geometry_component_issues", [])
        conflicts.extend({"kind": "runtime_geometry_component", **issue} for issue in issues)
        accounted = sum(issue.get("nodes", 0) if issue.get("equality_mismatches", 0) else
                        issue.get("negative_base_nodes", 0) + issue.get("overflow_nodes", 0) for issue in issues)
        if not issues or elevation["structural_conflicts"] > accounted:
            conflicts.append({"kind": "runtime_structure", "count": elevation["structural_conflicts"] - accounted})
    unresolved = elevation.get("unresolved_stairs", [])
    unknown_walls = elevation.get("unknown_wall_cells", 0)
    success = not missing and not conflicts and not unresolved and not map_missing and not map_mismatches and not unknown_walls
    return {"format": "alttp-room-height-validation-v2", "room_id": graph.get("room_id"),
            "success": success, "rise": rise, **meta,
            "required_walkable_placements": sum(meta["per_plane_walkable_denominator"]),
            "valid_walkable_placements": len(supports), "missing_placements": missing,
            "required_fallback_placements": len(fallbacks) + len(fallback_conflicts),
            "valid_fallback_placements": len(fallbacks), "fallback_conflicts": fallback_conflicts,
            "effective_height_placements": len(placements), "placements": placements,
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
    rects = "".join(f'<rect x="{x*8}" y="{y*8}" width="8" height="8" fill="{color}"/>'
                    for (x, y), color in cells.items())
    legend = html.escape("green=support orange=fallback purple=missing red=conflict")
    path.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="536" '
                    f'viewBox="0 0 512 536"><rect width="512" height="512" fill="#25202b"/>{rects}'
                    f'<text x="4" y="530" fill="white" font-size="12">{legend}</text></svg>\n')


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
    parser.add_argument("--runtime-solution", type=Path,
                        help="complete-room output of alttp_runtime_room_heights; bases are height units, not levels")
    parser.add_argument("--rise", type=int, default=50)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--diagnostic-svg", type=Path)
    args = parser.parse_args()
    try:
        graph = json.loads(args.graph.read_text())
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
        with args.profile.open("rb") as source:
            profile = tomllib.load(source)
        if args.runtime_solution:
            solution = json.loads(args.runtime_solution.read_text())
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
            graph["elevation"] = {**graph["elevation"], **solution}
        height_map = load_height_map(args.height_map, room) if args.height_map else None
        report = validate(graph, args.rise, height_map, snapshot=snapshot,
                          attributes=attributes, profile=profile)
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
