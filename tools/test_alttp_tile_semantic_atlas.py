#!/usr/bin/env python3
"""Portable checks for the ALTTP tile semantic atlas."""

from __future__ import annotations

import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

from alttp_tile_semantic_atlas import (AnnotationStore, AtlasError, ROOM_INDEX_FORMAT,
                                       RoomOccurrenceIndex, TileCatalog,
                                       validate_annotation)


ROM_SHA256 = "1" * 64


def make_database(path: Path) -> None:
    connection = sqlite3.connect(path)
    connection.executescript("""
        CREATE TABLE run_info (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE decoded_tile (content_id TEXT PRIMARY KEY, bit_depth INTEGER, indices BLOB);
        CREATE TABLE graphics_source (source_kind TEXT, pack_index INTEGER, tile_index INTEGER,
          conversion TEXT, snes_address INTEGER, content_id TEXT);
        CREATE TABLE overworld_base_tile_usage (quadrant_index INTEGER, x INTEGER, y INTEGER,
          area_id INTEGER, tile_word INTEGER, vram_slot INTEGER, pack_index INTEGER,
          pack_tile_index INTEGER, conversion TEXT, content_id TEXT, status TEXT);
    """)
    connection.executemany("INSERT INTO run_info VALUES (?, ?)",
                           [("atlas_schema_version", "1"), ("rom_sha256", ROM_SHA256)])
    connection.executemany("INSERT INTO decoded_tile VALUES (?, ?, ?)",
                           [("bg", 4, bytes(range(16)) * 4),
                            ("sprite", 4, bytes(64))])
    connection.executemany("INSERT INTO graphics_source VALUES (?, ?, ?, ?, ?, ?)", [
        ("background", 1, 2, "low", 0x1000, "bg"),
        ("background", 2, 3, "high", 0x2000, "bg"),
        ("sprite", 4, 5, "native", 0x3000, "bg"),
        ("sprite", 6, 7, "native", 0x4000, "sprite")])
    connection.execute("INSERT INTO overworld_base_tile_usage VALUES "
                       "(1, 2, 3, 4, 5, 6, 1, 2, 'low', 'bg', 'resolved')")
    connection.commit()
    connection.close()


class TileSemanticAtlasTests(unittest.TestCase):
    def test_roles_require_their_exact_wall_normals(self) -> None:
        self.assertEqual(validate_annotation(
            {"role": "wall", "normals": ["north"]})["normals"], ["north"])
        self.assertEqual(validate_annotation(
            {"role": "wall_corner", "normals": ["north", "east"],
             "corner_type": "inside"})["normals"],
                         ["north", "east"])
        self.assertEqual(validate_annotation(
            {"role": "wall", "normals": ["north_east"]})["normals"],
                         ["north_east"])
        with self.assertRaisesRegex(AtlasError, "requires 2"):
            validate_annotation({"role": "wall_corner", "normals": ["north"],
                                 "corner_type": "outside"})
        with self.assertRaisesRegex(AtlasError, "distinct"):
            validate_annotation({"role": "wall_corner", "normals": ["north", "north"],
                                 "corner_type": "outside"})
        with self.assertRaisesRegex(AtlasError, "cardinal"):
            validate_annotation({"role": "wall_corner", "normals": ["north", "south_east"],
                                 "corner_type": "outside"})
        with self.assertRaisesRegex(AtlasError, "corner_type"):
            validate_annotation({"role": "wall_corner", "normals": ["north", "east"]})
        with self.assertRaisesRegex(AtlasError, "requires 0"):
            validate_annotation({"role": "floor", "normals": ["north"]})

    def test_legacy_corner_loads_as_incomplete_but_cannot_be_resaved(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.json"
            path.write_text(json.dumps({
                "format": "alttp-tile-semantics-v1", "rom_sha256": ROM_SHA256,
                "tiles": {"bg": {"role": "wall_corner",
                                    "normals": ["north", "east"]}}}))
            store = AnnotationStore(path, ROM_SHA256)
            self.assertEqual(store.tiles["bg"]["corner_type"], "unknown")
            with self.assertRaisesRegex(AtlasError, "corner_type"):
                store.save("bg", store.tiles["bg"])

    def test_catalog_lists_unique_background_art_and_reuse(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "atlas.sqlite"
            make_database(database)
            catalog = TileCatalog(database)
            tiles = catalog.tiles({})
            self.assertEqual([tile["content_id"] for tile in tiles], ["bg"])
            self.assertEqual(tiles[0]["source_count"], 2)
            self.assertEqual(tiles[0]["family_count"], 2)
            self.assertEqual(tiles[0]["packs"], [1, 2])
            self.assertEqual((tiles[0]["first_pack"], tiles[0]["first_tile"]), (1, 2))
            self.assertEqual(tiles[0]["placement_count"], 1)
            self.assertEqual(len(tiles[0]["indices"]), 64)
            detail = catalog.detail("bg")
            self.assertEqual(len(detail["sources"]), 3)
            self.assertEqual(detail["alternatives"], [])

    def test_conversion_aliases_share_annotations_and_hide_low_variant(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "atlas.sqlite"
            make_database(database)
            connection = sqlite3.connect(database)
            connection.executemany("INSERT INTO decoded_tile VALUES (?, ?, ?)",
                                   [("low", 4, bytes([1]) * 64),
                                    ("high", 4, bytes([9]) * 64)])
            connection.executemany("INSERT INTO graphics_source VALUES (?, ?, ?, ?, ?, ?)", [
                ("background", 7, 8, "low", 0x5000, "low"),
                ("background", 7, 8, "high", 0x5000, "high")])
            connection.commit()
            connection.close()
            catalog = TileCatalog(database)
            annotation = {"role": "floor", "normals": [],
                          "context_dependent": False, "note": ""}
            tiles = {tile["content_id"]: tile for tile in catalog.tiles({"high": annotation})}
            self.assertEqual(catalog.semantic_group("low"), ["high", "low"])
            self.assertTrue(tiles["low"]["low_variant"])
            self.assertFalse(tiles["high"]["low_variant"])
            self.assertEqual(tiles["low"]["annotation"], annotation)

            path = Path(directory) / "annotations.json"
            store = AnnotationStore(path, ROM_SHA256)
            store.save_group(catalog.semantic_group("high"), annotation)
            self.assertEqual(store.tiles, {"high": annotation, "low": annotation})

    def test_store_round_trip_is_atomic_and_rom_bound(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.json"
            store = AnnotationStore(path, ROM_SHA256)
            store.save("bg", {"role": "stair_top", "normals": [],
                              "context_dependent": True, "note": "upper cap"})
            loaded = AnnotationStore(path, ROM_SHA256)
            self.assertEqual(loaded.tiles["bg"]["role"], "stair_top")
            self.assertFalse(path.with_name(path.name + ".tmp").exists())
            value = json.loads(path.read_text())
            value["rom_sha256"] = "2" * 64
            path.write_text(json.dumps(value))
            with self.assertRaisesRegex(AtlasError, "fingerprint"):
                AnnotationStore(path, ROM_SHA256)

    def test_empty_unknown_removes_saved_annotation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.json"
            store = AnnotationStore(path, ROM_SHA256)
            store.save("bg", {"role": "floor"})
            store.save("bg", {"role": "unknown"})
            self.assertNotIn("bg", store.tiles)

    def test_room_occurrences_group_rooms_and_preserve_layers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rooms.sqlite"
            connection = sqlite3.connect(path)
            connection.executescript("""
                CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE occurrence (content_id TEXT, room_id INTEGER, layer INTEGER,
                  x INTEGER, y INTEGER, hflip INTEGER, vflip INTEGER);
                CREATE TABLE room_status (room_id INTEGER PRIMARY KEY, status TEXT, detail TEXT);
                CREATE TABLE loaded_art (content_id TEXT, room_id INTEGER,
                  PRIMARY KEY (content_id, room_id));
            """)
            connection.executemany("INSERT INTO metadata VALUES (?, ?)",
                                   [("format", ROOM_INDEX_FORMAT),
                                    ("rom_sha256", ROM_SHA256)])
            connection.executemany("INSERT INTO occurrence VALUES (?, ?, ?, ?, ?, ?, ?)",
                                   [("bg", room, 0, 1, 2, 0, 0) for room in range(320)])
            connection.executemany("INSERT INTO room_status VALUES (?, 'indexed', '')",
                                   ((room,) for room in range(320)))
            connection.execute("INSERT INTO occurrence VALUES ('bg', 3, 1, 4, 5, 1, 0)")
            connection.executemany("INSERT INTO loaded_art VALUES ('bg', ?)", [(27,), (3,)])
            connection.commit()
            connection.close()
            index = RoomOccurrenceIndex(path, ROM_SHA256)
            rooms = index.rooms("bg")
            self.assertEqual(rooms[0], {"room_id": 3, "placement_count": 2,
                                        "bg1_count": 1, "bg2_count": 1})
            self.assertEqual(index.cells("bg", 3)[1],
                             {"layer": 1, "x": 4, "y": 5, "hflip": 1, "vflip": 0})
            self.assertEqual(index.loaded_rooms("bg"), [3, 27])

    def test_obscured_room_is_last_preview_choice(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rooms.sqlite"
            connection = sqlite3.connect(path)
            connection.executescript("""
                CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE occurrence (content_id TEXT, room_id INTEGER, layer INTEGER,
                  x INTEGER, y INTEGER, hflip INTEGER, vflip INTEGER);
                CREATE TABLE room_status (room_id INTEGER PRIMARY KEY, status TEXT, detail TEXT);
                CREATE TABLE loaded_art (content_id TEXT, room_id INTEGER,
                  PRIMARY KEY (content_id, room_id));
            """)
            connection.executemany("INSERT INTO metadata VALUES (?, ?)",
                                   [("format", ROOM_INDEX_FORMAT),
                                    ("rom_sha256", ROM_SHA256)])
            connection.executemany("INSERT INTO room_status VALUES (?, 'indexed', '')",
                                   ((room,) for room in range(320)))
            connection.executemany("INSERT INTO occurrence VALUES ('bg', ?, 0, 0, 0, 0, 0)",
                                   [(27,)] * 10 + [(42,)])
            connection.commit()
            connection.close()
            self.assertEqual([room["room_id"] for room in
                              RoomOccurrenceIndex(path, ROM_SHA256).rooms("bg")], [42, 27])


if __name__ == "__main__":
    unittest.main()
