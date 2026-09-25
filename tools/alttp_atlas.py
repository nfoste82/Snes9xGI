#!/usr/bin/env python3
"""ROM-backed ALTTP asset inventory and first-pass candidate profile writer.

This program keeps a local SQLite atlas and coverage report. An optional
hash-keyed metadata pass preserves authored fields and emits a separate profile.
ROM bytes and extracted artwork are never written into the source tree by default.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import sys
import tomllib
import zipfile

from alttp_atlas_graphics import extract_graphics
from alttp_atlas_dungeon import catalog_default_layouts, expand_dungeon_room
from alttp_atlas_overworld import extract_overworld
from alttp_atlas_overworld_gfx import BaseOverworldGraphics
from alttp_atlas_profile import generate_profile, ProfileGenerationError
from alttp_atlas_world import catalog_world


class AtlasError(Exception):
    pass


SUPPORTED_US_SHA256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb"
REFERENCE_COMMIT = "fbbb3f967a51fafe642e6140d0753979e73b4090"


class Rom:
    def __init__(self, path: Path):
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as archive:
                members = [name for name in archive.namelist()
                           if name.lower().endswith((".sfc", ".smc"))]
                if len(members) != 1:
                    raise AtlasError("ROM ZIP must contain exactly one .sfc or .smc member")
                data = archive.read(members[0])
        else:
            data = path.read_bytes()
        self.had_copier_header = len(data) % 0x8000 == 0x200
        if self.had_copier_header:
            data = data[0x200:]
        if len(data) != 0x100000:
            raise AtlasError(f"expected a 1 MiB USA ROM, found {len(data)} bytes")
        self.sha256 = hashlib.sha256(data).hexdigest()
        self.data = data

    @staticmethod
    def file_offset(snes_address: int) -> int:
        if not 0 <= snes_address <= 0xFFFFFF or not snes_address & 0x8000:
            raise AtlasError(f"invalid LoROM address ${snes_address:06X}")
        return ((snes_address >> 16) & 0x7F) * 0x8000 + (snes_address & 0x7FFF)

    def get_byte(self, snes_address: int) -> int:
        offset = self.file_offset(snes_address)
        if offset >= len(self.data):
            raise AtlasError(f"ROM read past end at ${snes_address:06X}")
        return self.data[offset]

    def read_bytes(self, snes_address: int, count: int) -> bytes:
        if count < 0 or count > len(self.data):
            raise AtlasError(f"invalid ROM read length {count}")
        result = bytearray()
        address = snes_address
        for _ in range(count):
            result.append(self.get_byte(address))
            address += 1
            if not address & 0x8000:
                address += 0x8000
        return bytes(result)

    def get_word(self, snes_address: int) -> int:
        return int.from_bytes(self.read_bytes(snes_address, 2), "little")

    def get_24(self, snes_address: int) -> int:
        return int.from_bytes(self.read_bytes(snes_address, 3), "little")


SCHEMA = """
PRAGMA foreign_keys = ON;
CREATE TABLE run_info (key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE decoded_tile (
  content_id TEXT PRIMARY KEY,
  bit_depth INTEGER NOT NULL,
  indices BLOB NOT NULL CHECK(length(indices) = 64)
);
CREATE TABLE graphics_source (
  source_kind TEXT NOT NULL,
  pack_index INTEGER,
  tile_index INTEGER NOT NULL,
  conversion TEXT NOT NULL,
  snes_address INTEGER NOT NULL,
  content_id TEXT NOT NULL REFERENCES decoded_tile(content_id),
  PRIMARY KEY (source_kind, pack_index, tile_index, conversion)
);
CREATE INDEX graphics_source_content ON graphics_source(content_id);
CREATE TABLE world_record (
  kind TEXT NOT NULL,
  record_key TEXT NOT NULL,
  source_address INTEGER NOT NULL,
  data_json TEXT NOT NULL,
  PRIMARY KEY (kind, record_key)
);
CREATE TABLE overworld_quadrant (
  quadrant_index INTEGER PRIMARY KEY CHECK(quadrant_index BETWEEN 0 AND 159),
  high_stream_address INTEGER NOT NULL,
  low_stream_address INTEGER NOT NULL,
  high_stream_length INTEGER NOT NULL,
  low_stream_length INTEGER NOT NULL,
  high_decoded_length INTEGER NOT NULL,
  low_decoded_length INTEGER NOT NULL,
  map32_ids BLOB NOT NULL CHECK(length(map32_ids) = 512),
  map16_ids BLOB NOT NULL CHECK(length(map16_ids) = 2048),
  map8_words BLOB NOT NULL CHECK(length(map8_words) = 8192)
);
CREATE TABLE overworld_base_tile_usage (
  quadrant_index INTEGER NOT NULL REFERENCES overworld_quadrant(quadrant_index),
  x INTEGER NOT NULL CHECK(x BETWEEN 0 AND 63),
  y INTEGER NOT NULL CHECK(y BETWEEN 0 AND 63),
  area_id INTEGER NOT NULL,
  tile_word INTEGER NOT NULL,
  vram_slot INTEGER,
  pack_index INTEGER,
  pack_tile_index INTEGER,
  conversion TEXT,
  content_id TEXT REFERENCES decoded_tile(content_id),
  status TEXT NOT NULL,
  PRIMARY KEY (quadrant_index, x, y)
);
CREATE INDEX overworld_usage_content ON overworld_base_tile_usage(content_id);
CREATE INDEX overworld_usage_status ON overworld_base_tile_usage(status);
CREATE TABLE overworld_animated_tile_usage (
  quadrant_index INTEGER NOT NULL REFERENCES overworld_quadrant(quadrant_index),
  x INTEGER NOT NULL CHECK(x BETWEEN 0 AND 63),
  y INTEGER NOT NULL CHECK(y BETWEEN 0 AND 63),
  phase INTEGER NOT NULL CHECK(phase BETWEEN 0 AND 2),
  area_id INTEGER NOT NULL,
  pack_index INTEGER NOT NULL,
  pack_tile_index INTEGER NOT NULL,
  content_id TEXT NOT NULL REFERENCES decoded_tile(content_id),
  PRIMARY KEY (quadrant_index, x, y, phase)
);
CREATE INDEX overworld_animated_content ON overworld_animated_tile_usage(content_id);
CREATE TABLE placed_sprite_type (
  family TEXT NOT NULL,
  type_id INTEGER NOT NULL,
  placement_count INTEGER NOT NULL,
  first_source_address INTEGER NOT NULL,
  PRIMARY KEY (family, type_id)
);
CREATE TABLE dungeon_partial_tilemap (
  room_id INTEGER PRIMARY KEY CHECK(room_id BETWEEN 0 AND 319),
  source_address INTEGER NOT NULL,
  bg1_words BLOB NOT NULL CHECK(length(bg1_words) = 8192),
  bg2_words BLOB NOT NULL CHECK(length(bg2_words) = 8192),
  complete INTEGER NOT NULL CHECK(complete = 0)
);
CREATE TABLE dungeon_object_write (
  object_id TEXT PRIMARY KEY,
  room_id INTEGER NOT NULL REFERENCES dungeon_partial_tilemap(room_id),
  source_address INTEGER NOT NULL,
  bg TEXT NOT NULL,
  writes_json TEXT NOT NULL
);
CREATE TABLE dungeon_expansion_gap (
  room_id INTEGER NOT NULL REFERENCES dungeon_partial_tilemap(room_id),
  gap_id TEXT NOT NULL,
  kind TEXT NOT NULL,
  source_address INTEGER NOT NULL,
  reason TEXT NOT NULL,
  PRIMARY KEY (room_id, gap_id)
);
CREATE TABLE profile_asset (
  content_id TEXT PRIMARY KEY,
  has_height INTEGER NOT NULL,
  has_normal INTEGER NOT NULL,
  has_occlusion INTEGER NOT NULL,
  has_emission INTEGER NOT NULL
);
CREATE TABLE finding (
  code TEXT NOT NULL,
  subject TEXT NOT NULL,
  detail TEXT NOT NULL,
  PRIMARY KEY (code, subject)
);
"""


def load_profile(path: Path) -> tuple[dict, str]:
    text = path.read_text()
    profile = tomllib.loads(text)
    sha = profile.get("game", {}).get("rom_sha256")
    if not isinstance(sha, str) or len(sha) != 64:
        raise AtlasError("profile has no valid game.rom_sha256")
    return profile, sha.lower()


def add_graphics(connection: sqlite3.Connection, graphics: list) -> dict:
    unique: dict[str, bytes] = {}
    counts: dict[str, int] = {}
    for tile in graphics:
        indices = bytes(tile.indices)
        if len(indices) != 64:
            raise AtlasError(f"decoded tile {tile.content_id} has {len(indices)} pixels")
        if tile.content_id in unique and unique[tile.content_id] != indices:
            raise AtlasError(f"content hash collision: {tile.content_id}")
        if tile.content_id not in unique:
            connection.execute("INSERT INTO decoded_tile VALUES (?, ?, ?)",
                               (tile.content_id, tile.bit_depth, indices))
            unique[tile.content_id] = indices
        connection.execute("INSERT INTO graphics_source VALUES (?, ?, ?, ?, ?, ?)",
                           (tile.source_kind, tile.pack_index, tile.tile_index,
                            tile.conversion, tile.snes_address, tile.content_id))
        counts[tile.source_kind] = counts.get(tile.source_kind, 0) + 1
    return {"sources_by_kind": counts, "unique_content_ids": len(unique)}


def add_world(connection: sqlite3.Connection, world: dict) -> dict:
    counts = {}
    for kind, records in sorted(world.items()):
        counts[kind] = len(records)
        for ordinal, record in enumerate(records):
            if (not isinstance(record, dict) or "source_address" not in record
                    or not isinstance(record.get("id"), str)):
                raise AtlasError(f"world {kind} record {ordinal} lacks identity/provenance")
            connection.execute("INSERT INTO world_record VALUES (?, ?, ?, ?)",
                               (kind, record["id"], record["source_address"],
                                json.dumps(record, sort_keys=True, separators=(",", ":"))))
    return counts


def _pack_words(values: tuple[int, ...], count: int) -> bytes:
    if len(values) != count or any(not 0 <= value <= 0xFFFF for value in values):
        raise AtlasError(f"expected {count} valid 16-bit tile words")
    return b"".join(value.to_bytes(2, "little") for value in values)


def add_overworld(connection: sqlite3.Connection, quadrants: list) -> dict:
    if len(quadrants) != 160 or {quadrant.index for quadrant in quadrants} != set(range(160)):
        raise AtlasError("overworld extraction did not return all 160 quadrants")
    for quadrant in quadrants:
        connection.execute(
            "INSERT INTO overworld_quadrant VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (quadrant.index, quadrant.high_stream_address, quadrant.low_stream_address,
             quadrant.high_stream_length, quadrant.low_stream_length,
             quadrant.high_decoded_length, quadrant.low_decoded_length,
             _pack_words(quadrant.map32_ids, 256),
             _pack_words(quadrant.map16_ids, 1024),
             _pack_words(quadrant.map8_words, 4096)),
        )
    return {"quadrants": len(quadrants), "map32_cells": len(quadrants) * 256,
            "map16_cells": len(quadrants) * 1024,
            "map8_cells": len(quadrants) * 4096,
            "nonstandard_plane_lengths": [quadrant.index for quadrant in quadrants
                                          if quadrant.high_decoded_length != 256
                                          or quadrant.low_decoded_length != 256]}


def add_overworld_base_usage(connection: sqlite3.Connection, graphics: list,
                             world: dict, quadrants: list) -> dict:
    resolver = BaseOverworldGraphics(graphics, world["overworld_areas"], quadrants)
    statuses: dict[str, int] = {}
    max_tile_number = 0
    animated_variants = 0
    for area_id in sorted(resolver.areas):
        for tile in resolver.iter_area_tiles(area_id):
            connection.execute(
                "INSERT INTO overworld_base_tile_usage VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (tile.quadrant_index, tile.x, tile.y, area_id, tile.word,
                 tile.vram_slot, tile.pack_index, tile.pack_tile_index,
                 tile.conversion, tile.content_id, tile.status),
            )
            statuses[tile.status] = statuses.get(tile.status, 0) + 1
            max_tile_number = max(max_tile_number, tile.tile_number)
            if tile.status == "animated_vram_slot":
                for phase in range(3):
                    animated = resolver.resolve_animated_phase(tile, phase)
                    connection.execute(
                        "INSERT INTO overworld_animated_tile_usage VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                        (tile.quadrant_index, tile.x, tile.y, phase, area_id,
                         animated.pack_index, animated.pack_tile_index,
                         animated.content_id),
                    )
                    animated_variants += 1
    if sum(statuses.values()) != 160 * 4096:
        raise AtlasError("base overworld usage does not cover all Map8 positions")
    distinct_content_ids = connection.execute(
        "SELECT COUNT(DISTINCT content_id) FROM overworld_base_tile_usage "
        "WHERE content_id IS NOT NULL").fetchone()[0]
    animated_content_ids = connection.execute(
        "SELECT COUNT(DISTINCT content_id) FROM overworld_animated_tile_usage").fetchone()[0]
    return {"area_heads": len(resolver.areas), "base_map8_words": sum(statuses.values()),
            "statuses": statuses, "distinct_content_ids": distinct_content_ids,
            "resolved_animation_phase_variants": animated_variants,
            "distinct_animation_content_ids": animated_content_ids,
            "ordinary_positions_with_art_across_phases":
                statuses.get("resolved", 0) + statuses.get("animated_vram_slot", 0),
            "maximum_tile_number": max_tile_number,
            "unresolved_runtime_layers": ["active animation phase selection", "overlays and persistent edits",
                                          "palette changes, fades, and color math"]}


def add_placed_sprite_types(connection: sqlite3.Connection, world: dict) -> dict:
    usages: dict[tuple[str, int], tuple[int, int]] = {}
    for kind in ("dungeon_sprites", "overworld_sprites"):
        for record in world[kind]:
            type_id = record.get("type_id")
            if type_id is None:
                continue  # Key markers are attached to a previous placement.
            family = ("overlord" if record.get("record_kind") == "overlord" else
                      "dungeon_sprite" if kind == "dungeon_sprites" else "overworld_sprite")
            key = (family, type_id)
            count, first_address = usages.get(key, (0, record["source_address"]))
            usages[key] = (count + 1, first_address)
    for (family, type_id), (count, address) in sorted(usages.items()):
        connection.execute("INSERT INTO placed_sprite_type VALUES (?, ?, ?, ?)",
                           (family, type_id, count, address))
    return {family: sum(key[0] == family for key in usages)
            for family in ("dungeon_sprite", "overworld_sprite", "overlord")}


def add_dungeons(connection: sqlite3.Connection, rom: Rom, world: dict) -> dict:
    objects_by_room: dict[int, list] = {room_id: [] for room_id in range(320)}
    doors_by_room: dict[int, list] = {room_id: [] for room_id in range(320)}
    for record in world["dungeon_objects"]:
        objects_by_room[record["room_id"]].append(record)
    for record in world["dungeon_doors"]:
        doors_by_room[record["room_id"]].append(record)
    gap_counts: dict[str, int] = {}
    written_objects = 0
    for room in world["dungeon_rooms"]:
        room_id = room["room_id"]
        result = expand_dungeon_room(rom, room, objects_by_room[room_id], doors_by_room[room_id])
        if result["complete"] or result["room_id"] != room_id:
            raise AtlasError(f"invalid partial dungeon map for room {room_id}")
        connection.execute("INSERT INTO dungeon_partial_tilemap VALUES (?, ?, ?, ?, 0)",
                           (room_id, result["source_address"],
                            _pack_words(result["bg1"], 4096),
                            _pack_words(result["bg2"], 4096)))
        for item in result["object_writes"]:
            connection.execute("INSERT INTO dungeon_object_write VALUES (?, ?, ?, ?, ?)",
                               (item["id"], room_id, item["source_address"], item["bg"],
                                json.dumps(item["tile_writes"], separators=(",", ":"))))
            written_objects += 1
        for gap in result["unsupported"]:
            connection.execute("INSERT INTO dungeon_expansion_gap VALUES (?, ?, ?, ?, ?)",
                               (room_id, gap["id"], gap["kind"], gap["source_address"],
                                gap["reason"]))
            gap_counts[gap["kind"]] = gap_counts.get(gap["kind"], 0) + 1
    return {"partial_rooms": len(world["dungeon_rooms"]),
            "supported_object_writes": written_objects, "unexpanded": gap_counts,
            "complete_rooms": 0}


def add_profile(connection: sqlite3.Connection, profile: dict) -> dict:
    assets = profile.get("assets", [])
    for asset in assets:
        content_id = asset["tile_hash"]
        connection.execute("INSERT INTO profile_asset VALUES (?, ?, ?, ?, ?)",
                           (content_id, int("height" in asset), int("normal_xyz" in asset),
                            int("occlusion" in asset), int("emission_rgba" in asset)))
    return {
        "profile_assets": len(assets),
        "profile_assets_with_height": sum("height" in asset for asset in assets),
        "profile_assets_with_normals": sum("normal_xyz" in asset for asset in assets),
        "profile_assets_with_occlusion": sum("occlusion" in asset for asset in assets),
        "profile_assets_with_emission": sum("emission_rgba" in asset for asset in assets),
    }


def compare_inventory(connection: sqlite3.Connection, path: Path) -> dict:
    inventory = json.loads(path.read_text())
    assets = inventory.get("assets", [])
    if not isinstance(assets, list):
        raise AtlasError("inventory.assets is not a list")
    exact = 0
    missing = 0
    mismatch = 0
    for asset in assets:
        content_id = asset["id"]
        indices = bytes(pixel for row in asset["indices"] for pixel in row)
        if len(indices) != 64:
            raise AtlasError(f"inventory tile {content_id} is not 8x8")
        row = connection.execute("SELECT indices FROM decoded_tile WHERE content_id = ?",
                                 (content_id,)).fetchone()
        if row is None:
            missing += 1
            connection.execute("INSERT INTO finding VALUES (?, ?, ?)",
                               ("capture_unmapped", content_id,
                                "Captured tile has no decoded source in current atlas"))
        elif row[0] != indices:
            mismatch += 1
            connection.execute("INSERT INTO finding VALUES (?, ?, ?)",
                               ("capture_mismatch", content_id,
                                "Same content ID has different decoded palette indices"))
        else:
            exact += 1
    return {"captured_tiles": len(assets), "exact": exact, "missing": missing,
            "mismatch": mismatch}


def write_atlas(rom: Rom, profile: dict, database: Path,
                inventory: Path | None) -> dict:
    database.parent.mkdir(parents=True, exist_ok=True)
    temporary = database.with_name(database.name + ".tmp")
    if temporary.exists():
        temporary.unlink()
    try:
        connection = sqlite3.connect(temporary)
        try:
            connection.executescript(SCHEMA)
            connection.execute("INSERT INTO run_info VALUES (?, ?)", ("atlas_schema_version", "1"))
            connection.execute("INSERT INTO run_info VALUES (?, ?)", ("rom_sha256", rom.sha256))
            connection.execute("INSERT INTO run_info VALUES (?, ?)", ("rom_bytes", str(len(rom.data))))
            connection.execute("INSERT INTO run_info VALUES (?, ?)",
                               ("input_had_copier_header", str(int(rom.had_copier_header))))
            connection.execute("INSERT INTO run_info VALUES (?, ?)",
                               ("zelda3_reference_commit", REFERENCE_COMMIT))
            graphics_records = extract_graphics(rom)
            graphics = add_graphics(connection, graphics_records)
            world_records = catalog_world(rom)
            world_records.update(catalog_default_layouts(rom))
            world = add_world(connection, world_records)
            sprite_types = add_placed_sprite_types(connection, world_records)
            dungeons = add_dungeons(connection, rom, world_records)
            quadrants = extract_overworld(rom)
            overworld = add_overworld(connection, quadrants)
            overworld_usage = add_overworld_base_usage(connection, graphics_records,
                                                       world_records, quadrants)
            authored = add_profile(connection, profile)
            captured = compare_inventory(connection, inventory) if inventory else None
            decoded_profile = connection.execute(
                "SELECT COUNT(*) FROM profile_asset JOIN decoded_tile USING (content_id)").fetchone()[0]
            cross_family_art = connection.execute(
                "SELECT COUNT(*) FROM (SELECT content_id FROM graphics_source "
                "GROUP BY content_id HAVING COUNT(DISTINCT source_kind) > 1)").fetchone()[0]
            repeated_art = connection.execute(
                "SELECT COUNT(*) FROM (SELECT content_id FROM graphics_source "
                "GROUP BY content_id HAVING COUNT(*) > 1)").fetchone()[0]
            unmapped_profile = [row[0] for row in connection.execute(
                "SELECT content_id FROM profile_asset LEFT JOIN decoded_tile USING (content_id) "
                "WHERE decoded_tile.content_id IS NULL ORDER BY content_id")]
            for content_id in unmapped_profile:
                connection.execute("INSERT INTO finding VALUES (?, ?, ?)",
                                   ("profile_unmapped", content_id,
                                    "Profile asset has no decoded source in current atlas"))
            counts = {"rom_sha256": rom.sha256, "graphics": graphics,
                      "world": world, "dungeons": dungeons, "overworld": overworld,
                      "overworld_base_usage": overworld_usage,
                      "placed_sprite_types": sprite_types, **authored,
                      "profile_ids_decoded": decoded_profile,
                      "profile_ids_unmapped": unmapped_profile,
                      "art_reuse": {"ids_with_multiple_rom_sources": repeated_art,
                                    "ids_shared_across_graphics_families": cross_family_art},
                      "inventory": captured,
                      "coverage_gates": {
                          "graphics_streams": "decoded from known USA pointer tables",
                          "dungeon_tilemaps": "floor and ceiling subset expanded; all 320 rooms incomplete, see dungeon_expansion_gap",
                          "overworld_base_map": "ordinary base positions and three animation phase variants assigned; special entries, overlays, and active runtime phase unresolved",
                          "logical_sprite_frames": "not enumerated",
                          "metadata_proposals": "not generated",
                          "profile_output": "not written",
                      }}
            connection.commit()
        finally:
            connection.close()
        os.replace(temporary, database)
        return counts
    except BaseException:
        if temporary.exists():
            temporary.unlink()
        raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Inventory ALTTP ROM assets and write an optional candidate profile")
    parser.add_argument("--rom", type=Path, required=True, help="user-supplied USA .sfc/.smc or ZIP")
    parser.add_argument("--profile", type=Path, required=True, help="remaster TOML profile")
    parser.add_argument("--database", type=Path, required=True, help="local SQLite output")
    parser.add_argument("--report", type=Path, required=True, help="local JSON coverage report")
    parser.add_argument("--output-profile", type=Path,
                        help="write a first-pass hash-keyed candidate profile")
    parser.add_argument("--inventory", type=Path, help="optional Snes9x tile-inventory.json")
    options = parser.parse_args(argv)
    try:
        output_paths = {options.database.resolve(), options.report.resolve()}
        if len(output_paths) != 2 or output_paths & {options.rom.resolve(), options.profile.resolve()}:
            raise AtlasError("database and report must be distinct from each other and from inputs")
        if options.inventory and options.inventory.resolve() in output_paths:
            raise AtlasError("database and report must not replace the input inventory")
        if options.output_profile:
            candidate_path = options.output_profile.resolve()
            forbidden = output_paths | {options.rom.resolve(), options.profile.resolve()}
            if options.inventory:
                forbidden.add(options.inventory.resolve())
            if candidate_path in forbidden:
                raise AtlasError("candidate profile must be distinct from all inputs and outputs")
        rom = Rom(options.rom)
        profile, expected_sha = load_profile(options.profile)
        if rom.sha256 != expected_sha:
            raise AtlasError("ROM SHA-256 does not match the profile")
        if rom.sha256 != SUPPORTED_US_SHA256:
            raise AtlasError("this atlas decoder supports only the verified unmodified USA ROM")
        counts = write_atlas(rom, profile, options.database, options.inventory)
        if options.output_profile:
            counts["profile_generation"] = generate_profile(
                options.database, options.profile, options.output_profile)
            counts["coverage_gates"]["metadata_proposals"] = (
                "first-pass hash-keyed layers for all decoded tiles; in-game review needed")
            counts["coverage_gates"]["profile_output"] = "candidate profile written"
        options.report.parent.mkdir(parents=True, exist_ok=True)
        temporary_report = options.report.with_name(options.report.name + ".tmp")
        temporary_report.write_text(json.dumps(counts, indent=2, sort_keys=True) + "\n")
        os.replace(temporary_report, options.report)
        print(json.dumps(counts, indent=2, sort_keys=True))
        return 0
    except (AtlasError, ProfileGenerationError, OSError, ValueError, KeyError, RuntimeError,
            sqlite3.Error, zipfile.BadZipFile) as error:
        print(f"alttp-atlas: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
