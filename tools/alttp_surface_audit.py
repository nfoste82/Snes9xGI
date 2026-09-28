#!/usr/bin/env python3
"""Batch-audit linked ALTTP dungeon rooms using explicit surface rules.

Exports are checked against their ALTPRV1 sidecars. This tool never promotes
unknown geometry to a height map or changes authored lighting metadata.
"""

from __future__ import annotations

import argparse
import base64
from collections import Counter, deque
import html
import json
from pathlib import Path
import subprocess
import struct
import tempfile
import tomllib
import zlib

from alttp_room_geometry import (RAW_ROW_BYTES, RoomProvenance, RoomSnapshot,
                                 STAIR_HIGH_SIDE, components, flat_art_candidates,
                                 room_stairs)
from alttp_surface_rules import scene_from_room, solve


def _png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def linked_room_image(renderer: Path, room: int, directory: Path, first_raw: Path) -> bytes:
    """Stitch linked 256x224 viewports directly in room-local coordinates.

    Unlike the older gallery, the linked raw format explicitly records its
    viewport and extracts screen colors after the PPU's variable side padding.
    """
    pixels = bytearray(512 * 512 * 3)
    for sy in (0, 224, 448):
        for sx in (0, 256):
            raw = first_raw if (sx, sy) == (0, 0) else directory / f"view-{room:03x}-{sx}-{sy}.raw"
            if raw != first_raw:
                # The linked renderer bounds the last viewport at y=288.
                viewport_y = min(sy, 288)
                subprocess.run([str(renderer), "--linked", hex(room), str(raw), "-1",
                                str(sx), str(viewport_y), "0"], cwd=renderer.parent,
                               check=True, capture_output=True, text=True)
            try:
                data = raw.read_bytes()
                actual_x, actual_y = struct.unpack_from("<HH", data, 10)
                if (actual_x, actual_y) != (sx, min(sy, 288)):
                    raise ValueError("linked viewport does not match its requested position")
                source_y = sy - actual_y
                for y in range(min(224 - source_y, 512 - sy)):
                    for x in range(256):
                        color = struct.unpack_from("<H", data, 14 + (source_y + y) * RAW_ROW_BYTES + 30 + x * 6)[0]
                        target = ((sy + y) * 512 + sx + x) * 3
                        pixels[target:target + 3] = bytes(((color & 31) * 255 // 31,
                                                            ((color >> 5) & 31) * 255 // 31,
                                                            ((color >> 10) & 31) * 255 // 31))
            finally:
                if raw != first_raw:
                    raw.unlink(missing_ok=True)
                    Path(str(raw) + ".prov").unlink(missing_ok=True)
    rows = b"".join(b"\0" + pixels[y * 512 * 3:(y + 1) * 512 * 3] for y in range(512))
    return (b"\x89PNG\r\n\x1a\n" +
            _png_chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0)) +
            _png_chunk(b"IDAT", zlib.compress(rows, 7)) + _png_chunk(b"IEND", b""))


def stair_records(atlas: Path, room: int, rise: int, provenance: RoomProvenance) -> list[dict]:
    """Only handlers with a known high-side relationship become constraints."""
    writers = {(obj["phase"], obj["ordinal"]): index + 1
               for index, obj in enumerate(provenance.objects) if not obj["is_door"]}
    records = []
    for stair in room_stairs(atlas, room):
        handler = stair["subtype"], stair["object_id"]
        if handler not in STAIR_HIGH_SIDE:
            continue
        writer = writers.get((stair["layer"], stair["ordinal"]))
        if writer is None:
            raise ValueError(f"room 0x{room:03x} stair {stair['id']} has no matching dispatched writer")
        records.append({"id": stair["id"], "layer": 1, "x": stair["x"], "y": stair["y"],
                        "high_side": STAIR_HIGH_SIDE[handler], "rise": rise, "writer": writer})
    return records


def stair_evidence(snapshot: RoomSnapshot, provenance: RoomProvenance,
                   stairs: list[dict]) -> dict:
    """Account for every claimed 4x4 assembly and both two-cell landings."""
    result = []
    for stair in sorted(stairs, key=lambda item: item["id"]):
        x, y = stair["x"], stair["y"]
        if not (0 <= x <= 60 and 1 <= y <= 59):
            result.append({**stair, "status": "outside_supported_footprint"})
            continue
        def sample(xx: int, yy: int) -> dict:
            cell = snapshot.cell(stair["layer"], xx, yy)
            writer = provenance.owner(stair["layer"], xx, yy)
            source = provenance.objects[writer - 1] if 0 < writer <= len(provenance.objects) else None
            if writer == 0:
                source_role = "default_layout"
            elif writer == 0xffff:
                source_role = "untraced_update"
            elif writer == stair["writer"]:
                source_role = "stair_handler"
            elif source and source["phase"] > 1:
                source_role = "later_room_layer_or_door"
            else:
                source_role = "other_object"
            return {"at": [xx, yy], "tile_hash": cell.tile_hash, "word": cell.word,
                    "hflip": cell.hflip, "vflip": cell.vflip, "writer": writer,
                    "source_role": source_role,
                    "writer_phase": source["phase"] if source else None,
                    "writer_ordinal": source["ordinal"] if source else None}
        footprint = [sample(xx, yy) for yy in range(y, y + 4) for xx in range(x, x + 4)]
        north = [sample(xx, y - 1) for xx in (x + 1, x + 2)]
        south = [sample(xx, y + 4) for xx in (x + 1, x + 2)]
        writers = sorted(set(cell["writer"] for cell in footprint))
        source_count = sum(cell["writer"] == stair["writer"] for cell in footprint)
        result.append({**stair, "status": "review" if source_count else "missing_source_writer",
                       "source_writer_cells": source_count, "overwritten_footprint_cells": 16 - source_count,
                       "footprint": footprint,
                       "footprint_writers": writers, "north_landing": north,
                       "south_landing": south, "higher_landing": stair["high_side"],
                       "lower_landing": "south" if stair["high_side"] == "north" else "north"})
    return {"format": "alttp-stair-evidence-v1", "room_id": snapshot.room,
            "stairs": result,
            "limits": ["A value-changing writer is not complete support provenance.",
                       "Landing samples are not classified as walkable by their tile hash.",
                        "4x4 footprints and north/south landing positions need handler validation."]}


def floor_coverage(snapshot: RoomSnapshot, provenance: RoomProvenance,
                   evidence: dict, profile: dict, attributes: bytes | None = None,
                   review_anchors: list[dict] | None = None) -> dict:
    """Review-only connected flat-art candidates reached from stair landings.

    This deliberately does not classify walkability or publish solved heights.
    Only the two sampled cells on *each* landing may seed a component, and
    contradictory stair relationships leave the affected components uncolored.
    """
    assets = {asset["tile_hash"]: asset for asset in profile["assets"]}
    hashes, inferred = flat_art_candidates(snapshot, profile, assets)
    if attributes is not None:
        if len(attributes) != 10 + 2 * 64 * 64 or attributes[:8] != b"ALTPAT1\0" or (
                struct.unpack_from("<H", attributes, 8)[0] != snapshot.room):
            raise ValueError("room collision attributes do not match the linked room")
    primary_attributes = attributes[10:4106] if attributes is not None else None
    secondary_attributes = attributes[4106:] if attributes is not None else None
    ids, found = components(snapshot.cells[1], hashes, provenance.owners[1],
                            set(profile["asset_groups"]["wall_tops"]["tile_hashes"]))
    # An overlay object may overwrite BG2 with a neutral fill while BG1 still
    # supplies the visible wall/bridge. BG2's apparent flatness is not evidence
    # of a visible floor. Preserve these positions as uncertain, including any
    # stair landing whose BG2 support is overwritten.
    hidden = set()
    for index, cell in enumerate(snapshot.cells[1]):
        writer = provenance.owners[1][index]
        if (0 < writer <= len(provenance.objects) and
                provenance.objects[writer - 1]["phase"] > 1 and
                snapshot.cells[0][index].word != cell.word):
            hidden.add(index)
    for index in hidden:
        ids[index] = -1
    # Reconnect only candidate cells that remain visible after overlay
    # filtering. In particular, one large BG2 component must not join two
    # otherwise separate floors through a covered corridor.
    visible_ids = [-1] * 4096
    visible_parts = []
    for start in range(4096):
        if ids[start] < 0 or visible_ids[start] >= 0:
            continue
        origin = ids[start]
        part = len(visible_parts)
        visible_ids[start] = part
        queue = deque([start])
        visible_parts.append([])
        while queue:
            index = queue.popleft()
            visible_parts[part].append(index)
            x, y = index % 64, index // 64
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if 0 <= nx < 64 and 0 <= ny < 64:
                    neighbor = ny * 64 + nx
                    if ids[neighbor] == origin and visible_ids[neighbor] == -1:
                        visible_ids[neighbor] = part
                        queue.append(neighbor)
    ids = visible_ids
    graph: dict[int, list[tuple[int, int]]] = {}
    links = []
    for stair in evidence["stairs"]:
        if stair["status"] != "review":
            continue
        landings = {}
        for side in ("north", "south"):
            values = {ids[y * 64 + x] for cell in stair[f"{side}_landing"]
                      for x, y in (cell["at"],)}
            if len(values) == 1 and -1 not in values:
                landings[side] = values.pop()
        if len(landings) != 2 or landings["north"] == landings["south"]:
            links.append({"stair": stair["id"], "status": "unresolved_landing"})
            continue
        high = landings[stair["higher_landing"]]
        low = landings[stair["lower_landing"]]
        graph.setdefault(low, []).append((high, 1))
        graph.setdefault(high, []).append((low, -1))
        links.append({"stair": stair["id"], "status": "provisional" if
                      stair["overwritten_footprint_cells"] else "candidate",
                      "high_component": high, "low_component": low})
    levels: dict[int, int] = {}
    conflicts = []
    for start in sorted(graph):
        if start in levels:
            continue
        relative = {start: 0}
        queue = [start]
        conflict = False
        for current in queue:
            for neighbor, delta in graph[current]:
                proposed = relative[current] + delta
                if neighbor in relative and relative[neighbor] != proposed:
                    conflict = True
                elif neighbor not in relative:
                    relative[neighbor] = proposed
                    queue.append(neighbor)
        if conflict:
            conflicts.append(sorted(relative))
        elif len(set(relative.values())) == 2:
            base = min(relative.values())
            levels.update({component: value - base for component, value in relative.items()})
    anchored = {}
    for anchor in review_anchors or []:
        x, y = anchor.get("at", (None, None))
        if (type(x) is not int or type(y) is not int or not 0 <= x < 64 or not 0 <= y < 64 or
                anchor.get("level") not in ("upper", "lower") or not anchor.get("evidence")):
            raise ValueError("review anchor needs a tile position, level and evidence")
        part = ids[y * 64 + x]
        if part < 0 or (part in levels and ("upper" if levels[part] == 1 else "lower") != anchor["level"]) or (
                part in anchored and anchored[part] != anchor["level"]):
            raise ValueError("review anchor conflicts with stair relation or has no candidate surface")
        anchored[part] = anchor["level"]
    cells = [{"at": [index % 64, index // 64],
              "level": ("upper" if levels[component] == 1 else "lower") if component in levels else anchored[component],
              "component": component, "evidence": "stair_relation" if component in levels else "review_anchor"}
             for index, component in enumerate(ids) if component in levels or component in anchored]
    unanchored = [{"at": [index % 64, index // 64], "component": component,
                   "foreground_attribute": primary_attributes[index] if primary_attributes else None,
                   "lower_plane_attribute": secondary_attributes[index] if secondary_attributes else None}
                  for index, component in enumerate(ids) if component >= 0 and component not in levels
                  and component not in anchored]
    collision = Counter()
    if primary_attributes is not None and secondary_attributes is not None:
        for item in cells:
            x, y = item["at"]
            index = y * 64 + x
            collision[(item["level"], primary_attributes[index], secondary_attributes[index])] += 1
    colored_positions = {y * 64 + x for item in cells for x, y in (item["at"],)}
    hidden_candidates = [{"at": [index % 64, index // 64],
                          "foreground_tile": snapshot.cells[0][index].word & 0x3ff,
                          "bg2_tile": snapshot.cells[1][index].word & 0x3ff,
                          "foreground_attribute": primary_attributes[index] if primary_attributes else None,
                          "lower_plane_attribute": secondary_attributes[index] if secondary_attributes else None}
                         for index in sorted(hidden - colored_positions)
                         if snapshot.cells[1][index].tile_hash in hashes]
    return {"format": "alttp-floor-coverage-review-v1", "room_id": snapshot.room,
            "source": "candidate flat-art components seeded by two stair landing cells per side",
            "status": "review_only", "cells": cells, "unanchored_candidates": unanchored,
            "review_anchors": [{**anchor, "component": ids[anchor["at"][1] * 64 + anchor["at"][0]]}
                               for anchor in review_anchors or []],
            "hidden_bg2_candidates": len(hidden_candidates),
            "covered_candidates": hidden_candidates,
            "collision_attribute_counts": [dict(level=level, foreground=fg, lower_plane=lower, count=count)
                                           for (level, fg, lower), count in sorted(collision.items())],
            "stair_links": links,
            "conflicting_components": conflicts, "candidate_components": len(found),
            "inferred_flat_art_hashes": sorted(inferred),
            "limits": ["Attribute 00 occurs on walkable floors and walls; it is not a floor mask.",
                       "BG2 hidden by later overlays is excluded from visible-floor coloring.",
                       "Disconnected flat-art candidates have no stair-derived height.",
                       "Wall-border and covered/under-bridge cells need structural support evidence.",
                       "Flat art and last-writer boundaries are not collision or walkability proof.",
                       "A stair overwritten by another object supplies provisional orientation only.",
                       "Uncolored cells are not classified as walls or non-walkable.",
                       "Upper/lower labels are relative; no absolute height map is published."]}


def write_stair_overlay(path: Path, evidence: dict) -> None:
    """Data-only, full-room 512x512 location map; no ROM artwork is copied."""
    pixels = bytearray(bytes((36, 40, 48)) * (512 * 512))
    def paint(x: int, y: int, rgb: tuple[int, int, int]) -> None:
        if not (0 <= x < 64 and 0 <= y < 64):
            return
        for row in range(y * 8, y * 8 + 8):
            for col in range(x * 8, x * 8 + 8):
                offset = (row * 512 + col) * 3
                pixels[offset:offset + 3] = bytes(rgb)
    for stair in evidence["stairs"]:
        if stair["status"] != "review":
            continue
        for cell in stair["footprint"]:
            paint(*cell["at"], (230, 148, 71))
        for side in ("north", "south"):
            for cell in stair[f"{side}_landing"]:
                paint(*cell["at"], (73, 188, 115) if side == stair["higher_landing"]
                      else (75, 125, 214))
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    rows = b"".join(b"\0" + pixels[y * 512 * 3:(y + 1) * 512 * 3] for y in range(512))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows, 7)) + chunk(b"IEND", b""))


def write_collision_zero_review(path: Path, room: int, background: bytes,
                                attributes: bytes) -> None:
    """Display attribute 00 independently on both gameplay collision planes."""
    if (len(attributes) != 10 + 2 * 64 * 64 or attributes[:8] != b"ALTPAT1\0" or
            struct.unpack_from("<H", attributes, 8)[0] != room):
        raise ValueError("room collision attributes do not match the linked room")
    source = "data:image/png;base64," + base64.b64encode(background).decode("ascii")
    planes = (attributes[10:4106], attributes[4106:])
    pieces = ['<svg xmlns="http://www.w3.org/2000/svg" width="1056" height="615" '
              'viewBox="0 0 1056 615" style="background:#151923;font-family:system-ui,sans-serif">',
              f'<text x="16" y="26" fill="white" font-size="19">Room 0x{room:03x}: collision attribute 00</text>']
    for plane, (values, title) in enumerate(zip(planes, ("Upper gameplay plane", "Lower gameplay plane"))):
        x0 = 16 + plane * 524
        count = values.count(0)
        pieces.append(f'<text x="{x0}" y="50" fill="#cbd3dd" font-size="14">'
                      f'{title}: {count:,}/4,096 tiles</text>')
        pieces.append(f'<image href="{source}" x="{x0}" y="62" width="512" height="512"/>')
        pieces.append(f'<rect x="{x0}" y="62" width="512" height="512" fill="#000" opacity="0.22"/>')
        for index, value in enumerate(values):
            if value != 0:
                continue
            x, y = index % 64, index // 64
            other = planes[1 - plane][index]
            different = other != 0
            fill = "#fca55e" if different else "#51d5ed"
            pieces.append(f'<rect x="{x0 + x * 8}" y="{62 + y * 8}" width="8" height="8" '
                          f'fill="{fill}" opacity="0.36"><title>({x},{y}) '
                          f'{title}: 00; other plane: {other:02x}</title></rect>')
    pieces.extend(['<text x="16" y="602" fill="#cbd3dd" font-size="13">'
                   'Cyan: 00 on both planes. Orange: 00 on this plane only. '
                   '00 does not establish reachability or floor height.</text>', '</svg>'])
    path.write_text("\n".join(pieces) + "\n")


def write_stair_review(path: Path, evidence: dict, background: bytes,
                       floors: dict | None = None) -> None:
    """Show every ROM stair against the room art, with source links on hover."""
    source = "data:image/png;base64," + base64.b64encode(background).decode("ascii")
    pieces = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="640" '
              f'viewBox="0 0 1024 640" style="background:#151923;font-family:system-ui,sans-serif">',
              f'<image href="{source}" x="0" y="0" width="512" height="512"/>',
              '<rect x="0" y="0" width="512" height="512" fill="#000" opacity="0.18"/>',
              f'<text x="535" y="35" fill="white" font-size="20">Room 0x{evidence["room_id"]:03x}: stair evidence</text>',
               '<text x="535" y="70" fill="#b4bdcb">Light green = upper; dark green = lower (candidates)</text>',
              '<text x="535" y="94" fill="#b4bdcb">Orange = 4×4 assumed stair footprint</text>',
               '<text x="535" y="118" fill="#b4bdcb">Gold = unanchored; amber = covered; pink = overwritten stair</text>',
               '<text x="535" y="145" fill="#fcbf72">Flat-art proposals, not verified walkable floors.</text>']
    if floors:
        for cell in floors["unanchored_candidates"]:
            x, y = cell["at"]
            pieces.append(f'<rect x="{x * 8 + 1}" y="{y * 8 + 1}" width="6" height="6" '
                          f'fill="none" stroke="#ffd480" stroke-opacity="0.55" stroke-width="0.8">'
                          f'<title>Unanchored flat-art candidate; component {cell["component"]}; '
                          f'foreground collision={cell["foreground_attribute"]}, '
                          f'lower-plane collision={cell["lower_plane_attribute"]}; '
                          f'walkability and height unverified</title></rect>')
        for cell in floors["covered_candidates"]:
            x, y = cell["at"]
            pieces.append(f'<rect x="{x * 8}" y="{y * 8}" width="8" height="8" '
                          f'fill="#f0bf64" opacity="0.15"><title>Covered BG2 flat-art candidate; '
                          f'BG1 tile={cell["foreground_tile"]}, BG2 tile={cell["bg2_tile"]}; '
                          f'foreground collision={cell["foreground_attribute"]}, '
                          f'lower-plane collision={cell["lower_plane_attribute"]}; '
                          f'no height assigned</title></rect>')
        for cell in floors["cells"]:
            x, y = cell["at"]
            color = "#6fe8a8" if cell["level"] == "upper" else "#32976d"
            opacity = "0.38" if cell["evidence"] == "stair_relation" else "0.26"
            pieces.append(f'<rect x="{x * 8}" y="{y * 8}" width="8" height="8" '
                          f'fill="{color}" opacity="{opacity}"><title>{cell["level"]} candidate; '
                          f'{cell["evidence"]}; flat component {cell["component"]}</title></rect>')
        upper = sum(cell["level"] == "upper" for cell in floors["cells"])
        lower = len(floors["cells"]) - upper
        status = " (stair connection provisional)" if any(
            link["status"] == "provisional" for link in floors["stair_links"]) else ""
        pieces.append(f'<text x="535" y="{505 if len(evidence["stairs"]) > 2 else 315}" '
                      f'fill="#b4bdcb" font-size="13">'
                      f'Flat candidates: {upper} upper, {lower} lower{status}</text>')
        unanchored = len(floors["unanchored_candidates"])
        pieces.append(f'<text x="535" y="{525 if len(evidence["stairs"]) > 2 else 335}" '
                      f'fill="#fcbf72" font-size="13">'
                      f'{unanchored} disconnected candidates unresolved; '
                      f'{floors["hidden_bg2_candidates"]} covered BG2 candidates unknown.</text>')
    for index, stair in enumerate(evidence["stairs"]):
        if stair["status"] != "review":
            continue
        y = 180 + index * 100
        pieces.append(f'<text x="535" y="{y}" fill="white" font-size="15">'
                      f'{html.escape(stair["id"])} at ({stair["x"]},{stair["y"]})</text>')
        pieces.append(f'<text x="535" y="{y + 22}" fill="#b4bdcb">'
                      f'high = {stair["high_side"]}; footprint writers = '
                      f'{html.escape(str(stair["footprint_writers"]))}</text>')
        for kind, cells, color in (("footprint", stair["footprint"], "#ffad45"),
                                    ("north_landing", stair["north_landing"],
                                     "#6fe8a8" if stair["high_side"] == "north" else "#32976d"),
                                    ("south_landing", stair["south_landing"],
                                     "#6fe8a8" if stair["high_side"] == "south" else "#32976d")):
            for cell in cells:
                x, cell_y = cell["at"]
                title = html.escape(f'{kind}: ({x},{cell_y}) {cell["tile_hash"]} '
                                    f'writer={cell["writer"]} phase={cell["writer_phase"]} '
                                    f'ordinal={cell["writer_ordinal"]} source={cell["source_role"]}')
                display_color = "#e762c2" if kind == "footprint" and cell["source_role"] != "stair_handler" else color
                pieces.append(f'<rect x="{x * 8}" y="{cell_y * 8}" width="8" height="8" '
                              f'fill="{display_color}" opacity="0.52" stroke="white" stroke-width="0.4">'
                              f'<title>{title}</title></rect>')
        pieces.append(f'<rect x="{stair["x"] * 8}" y="{stair["y"] * 8}" width="32" '
                      f'height="32" fill="none" stroke="white" stroke-width="2"/>')
    pieces.extend(['<text x="20" y="549" fill="#cbd3dd">'
                   'Room-local linked view; flat-art candidates are not verified walkable floors.</text>',
                   '<text x="20" y="575" fill="#cbd3dd">'
                   'A stair direction alone does not establish which disconnected room floor is at 0.</text>',
                   '</svg>'])
    path.write_text("\n".join(pieces) + "\n")


def audit_rooms(renderer: Path, rules: dict, room_ids: list[int], output: Path,
                atlas: Path | None = None, rise: int = 50,
                floor_profile: Path | None = None,
                floor_review_anchors: Path | None = None) -> dict:
    if atlas is not None and rise <= 0:
        raise ValueError("floor rise must be positive")
    if floor_profile is not None and atlas is None:
        raise ValueError("floor coverage needs a ROM atlas with oriented stairs")
    if floor_review_anchors is not None and floor_profile is None:
        raise ValueError("review anchors need a floor profile")
    profile = tomllib.loads(floor_profile.read_text()) if floor_profile is not None else None
    review = json.loads(floor_review_anchors.read_text()) if floor_review_anchors is not None else None
    if review is not None and (review.get("format") != "alttp-floor-review-anchors-v1" or
                               not isinstance(review.get("rooms"), dict)):
        raise ValueError("invalid floor review anchor format")
    output.mkdir(parents=True, exist_ok=True)
    records = []
    totals = Counter()
    failures = 0
    with tempfile.TemporaryDirectory(prefix="surface-audit-") as directory:
        for room in sorted(set(room_ids)):
            if not 0 <= room < 320:
                raise ValueError("dungeon room IDs must be 0..319")
            raw = Path(directory) / f"room-{room:03x}.raw"
            sidecar = Path(str(raw) + ".prov")
            try:
                process = subprocess.run([str(renderer), "--linked", hex(room), str(raw),
                                          "-1", "0", "0", "0"], cwd=renderer.parent,
                                         check=True, capture_output=True, text=True)
                snapshot = RoomSnapshot.read(raw)
                provenance = RoomProvenance.read(sidecar, snapshot)
                scene = scene_from_room(snapshot, provenance)
                floors = None
                if atlas is not None:
                    scene["stairs"] = stair_records(atlas, room, rise, provenance)
                    evidence = stair_evidence(snapshot, provenance, scene["stairs"])
                    (output / f"room-{room:03x}-stair-evidence.json").write_text(
                        json.dumps(evidence, sort_keys=True, indent=2) + "\n")
                    write_stair_overlay(output / f"room-{room:03x}-stair-overlay.png", evidence)
                    attr_path = Path(str(raw) + ".attr")
                    if profile and not attr_path.is_file():
                        raise ValueError("floor review requires the renderer's ALTPAT1 collision sidecar")
                    floors = floor_coverage(snapshot, provenance, evidence, profile,
                                            attr_path.read_bytes() if profile else None,
                                            review.get("rooms", {}).get(f"0x{room:03x}", []) if review else None
                                            ) if profile else None
                    if floors:
                        (output / f"room-{room:03x}-floor-coverage.json").write_text(
                            json.dumps(floors, sort_keys=True, indent=2) + "\n")
                    background = linked_room_image(renderer, room, Path(directory), raw)
                    write_stair_review(output / f"room-{room:03x}-stair-review.svg", evidence, background, floors)
                    if profile:
                        write_collision_zero_review(output / f"room-{room:03x}-collision-00.svg",
                                                    room, background, attr_path.read_bytes())
                report = solve(scene, rules)
                name = f"room-{room:03x}-surface-report.json"
                (output / name).write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
                totals.update(report["region_counts"])
                records.append({"room_id": room, "status": "publishable" if report["publishable"] else "review",
                                "report": name, "coverage": report["region_counts"],
                                "contact_constraints": report["contact_constraints"],
                                "stair_constraints": len(report["stair_constraints"]),
                                "stair_objects": len(scene.get("stairs", [])),
                                "stair_evidence": f"room-{room:03x}-stair-evidence.json" if atlas else None,
                                "stair_overlay": f"room-{room:03x}-stair-overlay.png" if atlas else None,
                                 "stair_review": f"room-{room:03x}-stair-review.svg"
                                 if atlas else None,
                                  "floor_coverage": f"room-{room:03x}-floor-coverage.json"
                                  if floors else None,
                                  "collision_00": f"room-{room:03x}-collision-00.svg"
                                  if floors else None,
                                "issues": len(report["issues"]), "renderer": process.stdout.strip()})
            except (OSError, ValueError, subprocess.CalledProcessError) as error:
                failures += 1
                records.append({"room_id": room, "status": "capture_error", "reason": str(error)})
    manifest = {"format": "alttp-surface-audit-v1", "rooms": records,
                "requested_rooms": len(set(room_ids)), "capture_failures": failures,
                "coverage": dict(sorted(totals.items())),
                "limitations": ["Default-state reference-renderer captures are not gameplay validation.",
                                "Missing templates and anchors remain unknown or relative-only.",
                                "ALTPRV1 ownership records value-changing dispatches, not complete support relationships."]}
    (output / "manifest.json").write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--rules", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rooms", nargs="+", type=lambda text: int(text, 0), required=True)
    parser.add_argument("--atlas", type=Path, help="ROM-matched atlas supplying oriented stair handlers")
    parser.add_argument("--floor-profile", type=Path,
                        help="candidate profile supplying flat-art metadata for review-only floor coverage")
    parser.add_argument("--floor-review-anchors", type=Path,
                        help="explicit reviewed elevations for disconnected candidate floor components")
    parser.add_argument("--rise", type=int, default=50, help="declared floor-to-floor rise (default: 50)")
    args = parser.parse_args()
    result = audit_rooms(args.renderer.resolve(), json.loads(args.rules.read_text()), args.rooms,
                         args.output.resolve(), args.atlas.resolve() if args.atlas else None,
                         args.rise, args.floor_profile.resolve() if args.floor_profile else None,
                         args.floor_review_anchors.resolve() if args.floor_review_anchors else None)
    for room in result["rooms"]:
        print(f"0x{room['room_id']:03x}: {room['status']} {room.get('coverage', room.get('reason'))}")


if __name__ == "__main__":
    main()
