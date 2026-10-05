#!/usr/bin/env python3
"""Inventory dungeon rooms that may need placement-based height offsets.

Reads the ROM-derived world records written by alttp_atlas.py. The result is a
review queue, not a generated height mask: the game's combat floor bits do not
encode the visible outline of an elevated platform or its wall faces.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import sqlite3


# Catalog subtypes 1 and 2 dispatch to LoadType1ObjectSubtype3 and
# LoadType1ObjectSubtype2 respectively. These IDs are verified against the
# reference engine's staircase handlers; the former subtype-2 0x20 clue was
# a lit torch, not a staircase. Ordinary carpet trim is retained only as a
# descriptive count, not evidence of elevation.
FLOOR_TRIM_OBJECTS = {(0, value) for value in (0x33, 0x34, 0x70, 0x71)}
IN_ROOM_STAIRS = ({(1, value) for value in (0x1B, 0x1C, 0x1D, 0x33)} |
                  {(2, value) for value in (0x31, 0x32, 0x33)})
BETWEEN_ROOM_STAIRS = ({(1, value) for value in (*range(0x1E, 0x22), *range(0x26, 0x2A))} |
                       {(2, value) for value in (0x2D, 0x2E, 0x2F, *range(0x38, 0x3C))})
WATER_OBJECTS = ({(0, value) for value in (*range(0x3F, 0x45), 0x79, 0x7A)} |
                 {(2, value) for value in (0x35, 0x36, 0x37)} |
                 {(1, value) for value in (0x00, 0x01, 0x02, 0x5B)})


def read_records(connection: sqlite3.Connection, kind: str) -> list[dict]:
    return [json.loads(row[0]) for row in connection.execute(
        "SELECT data_json FROM world_record WHERE kind = ?", (kind,)
    )]


def inventory(atlas: Path) -> dict:
    with sqlite3.connect(f"file:{atlas}?mode=ro", uri=True) as connection:
        rooms = read_records(connection, "dungeon_rooms")
        objects = read_records(connection, "dungeon_objects")
        sprites = read_records(connection, "dungeon_sprites")
        source_hash = connection.execute(
            "SELECT value FROM run_info WHERE key = 'rom_sha256'"
        ).fetchone()
    if len(rooms) != 320 or not source_hash:
        raise ValueError("atlas needs all 320 dungeon rooms and a ROM hash")

    room_objects: dict[int, list[dict]] = defaultdict(list)
    room_sprites: dict[int, list[dict]] = defaultdict(list)
    for item in objects:
        room_objects[item["room_id"]].append(item)
    for item in sprites:
        if item["record_kind"] in ("sprite", "overlord"):
            room_sprites[item["room_id"]].append(item)

    results = []
    for room in sorted(rooms, key=lambda item: item["room_id"]):
        room_id = room["room_id"]
        object_kinds = {(item["subtype"], item["object_id"])
                        for item in room_objects[room_id]}
        sprite_floors = sorted({item["floor"] for item in room_sprites[room_id]})
        evidence = []
        if sprite_floors == [0, 1]:
            evidence.append("sprites_on_both_combat_planes")
        elif sprite_floors == [1]:
            evidence.append("sprites_on_lower_combat_plane")
        if room["bg2_mode"] == 6:
            evidence.append("bg2_mode_6")
        if room["collision"]:
            evidence.append("special_collision_mode")
        water_related = (room["collision"] == 4 or room["effect"] == 3 or
                         bool(object_kinds & WATER_OBJECTS))
        if object_kinds & IN_ROOM_STAIRS:
            evidence.append("in_room_stairs")
        if object_kinds & BETWEEN_ROOM_STAIRS:
            evidence.append("between_room_stairs")

        # Every evidence type is deliberately a broad candidate signal. Room
        # 0x60 and 0x61 are the only masks validated against user captures.
        results.append({
            "room_id": room_id,
            "room_hex": f"0x{room_id:03x}",
            "source_address": f"0x{room['source_address']:06x}",
            "bg2_mode": room["bg2_mode"],
            "collision_mode": room["collision"],
            "lights_out": room["lights_out"],
            "water_related": water_related,
            "effect": room["effect"],
            "room_tags": [room["tag0"], room["tag1"]],
            "floor_patterns": [room["floor1"], room["floor2"]],
            "object_count": len(room_objects[room_id]),
            "sprite_count": len(room_sprites[room_id]),
            "layer_object_counts": {
                str(layer): sum(item["layer"] == layer for item in room_objects[room_id])
                for layer in (1, 2, 3)
            },
            "sprite_floors": sprite_floors,
            "floor_trim_objects": sum((item["subtype"], item["object_id"]) in FLOOR_TRIM_OBJECTS
                                      for item in room_objects[room_id]),
            "in_room_stairs": sum((item["subtype"], item["object_id"]) in IN_ROOM_STAIRS
                                  for item in room_objects[room_id]),
            "between_room_stairs": sum((item["subtype"], item["object_id"]) in BETWEEN_ROOM_STAIRS
                                       for item in room_objects[room_id]),
            "evidence": evidence,
            "validated_height_mask": room_id in (0x60, 0x61),
        })

    counts = Counter(key for room in results for key in room["evidence"])
    return {
        "rom_sha256": source_hash[0],
        "method": "ROM room headers, room objects, and sprite placement floor bits",
        "room_count": len(results),
        "review_candidate_count": sum(bool(room["evidence"]) for room in results),
        "evidence_counts": dict(sorted(counts.items())),
        "rooms": results,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--atlas", type=Path, required=True,
                        help="ROM-derived SQLite atlas from alttp_atlas.py")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = inventory(args.atlas)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"{report['room_count']} rooms; "
          f"{report['review_candidate_count']} flagged for review; "
          f"saved {args.output}")


if __name__ == "__main__":
    main()
