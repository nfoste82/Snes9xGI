#!/usr/bin/env python3
"""Portable checks for the ALTTP tile semantic atlas."""

from __future__ import annotations

import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
import tomllib
from http.server import ThreadingHTTPServer
import threading
import urllib.request
import urllib.error

from alttp_tile_semantic_atlas import (AnnotationStore, AtlasError, ROOM_INDEX_FORMAT,
                                       RoomOccurrenceIndex, TileCatalog,
                                       validate_annotation, make_handler)


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
    def test_door_semantic_save_preserves_independent_masks_unless_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, marker = root / 'profile.toml', root / 'marker.json'
            ids = ['v1:4bpp:0123456789abcdef', 'v1:4bpp:fedcba9876543210',
                   'v1:4bpp:1111111111111111']
            masks = ['3f3f3f3f1f1f1f1f', '0000000000000000', None]
            protected = dict(height=list(range(64)), height_sampling='linear',
                             occlusion=[0, 255] * 32, normal_xyz=[128, 128, 255] * 64,
                             emission_rgba=[0] * 256, geometry_by_flip=True,
                             flip_horz_height=[7] * 64)
            text = f'schema_version = 25\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
            for key, mask in zip(ids, masks):
                text += f'\n[[assets]]\ntile_hash = "{key}"\ntile_role = "door"\n'
                for field, value in protected.items():
                    text += f'{field} = {json.dumps(value)}\n'
                if mask is not None:
                    text += f'doorframe_mask = "{mask}"\n'
            profile.write_text(text)
            marker.write_text(json.dumps(dict(profile=str(profile))))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)

            def check(expected_masks, role):
                assets = tomllib.loads(profile.read_text())['assets']
                self.assertEqual([a.get('doorframe_mask') for a in assets], expected_masks)
                for asset in assets:
                    self.assertEqual(asset['tile_role'], role)
                    self.assertEqual({k: asset[k] for k in protected}, protected)
                self.assertFalse(profile.with_name(profile.name + '.semantics.tmp').exists())

            # Group semantics must not copy one member's mask onto another,
            # invent an unknown mask, or erase explicit empty authoring.
            store.save_group(ids, dict(role='door', note='note only'), store.refresh())
            check(masks, 'door')
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
            store.save_group(ids, dict(role='door', note='after reload'), store.refresh())
            check(masks, 'door')
            # An explicit empty mask is a replacement, not omission.
            store.save_group(ids, dict(role='door', doorframe_mask='0000000000000000'), store.refresh())
            check(['0000000000000000'] * 3, 'door')
            store.save_group(ids, dict(role='door', doorframe_mask='ffffffffffffffff'), store.refresh())
            check(['ffffffffffffffff'] * 3, 'door')
            store.save_group(ids, dict(role='other'), store.refresh())
            check([None] * 3, 'other')
            # Returning to Door must not resurrect a mask removed by role change.
            store.save_group(ids, dict(role='door'), store.refresh())
            check([None] * 3, 'door')

    def test_registered_schema22_semantic_save_preserves_group_records(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); profile=root/'profile.toml'; marker=root/'marker.json'
            h='v1:4bpp:0123456789abcdef'
            group=(f'\n[[obstacle_groups]]\nname = "example"\n[[obstacle_groups.members]]\ntile_hash = "{h}"\n'
                   'x = -2\ny = 3\nz = -1\nhflip = true\nvflip = false\nsupport_contact = true\n')
            text=f'schema_version = 22\n[game]\nrom_sha256 = "{ROM_SHA256}"\n[[assets]]\ntile_hash = "{h}"\nheight = [7]\n'+group
            profile.write_text(text);marker.write_text(json.dumps(dict(profile=str(profile))))
            store=AnnotationStore(root/'annotations.json',ROM_SHA256,marker);revision=store.refresh()
            store.save_group([h],dict(role='obstacle',note='external semantic editing'),revision)
            parsed=tomllib.loads(profile.read_text())
            self.assertEqual(parsed['schema_version'],22)
            self.assertEqual(parsed['obstacle_groups'],tomllib.loads(text)['obstacle_groups'])
            self.assertIn(group.strip(),profile.read_text())
            self.assertEqual(parsed['assets'][0]['height'],[7])
            # Invalid membership/contact evidence must be refused before any
            # sidecar/profile mutation, including profile registration refresh.
            invalid=profile.read_text().replace('support_contact = true','support_contact = false')
            profile.write_text(invalid)
            with self.assertRaisesRegex(AtlasError,'explicit support_contact'):store.refresh()
            self.assertEqual(profile.read_text(),invalid)

    def test_stale_browser_put_is_rejected_after_native_save(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256)
            server = ThreadingHTTPServer(('127.0.0.1', 0),
                                         make_handler(TileCatalog(database), store, None, root))
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            url = f'http://127.0.0.1:{server.server_port}'
            try:
                with urllib.request.urlopen(url + '/api/catalog') as response:
                    revision = json.load(response)['revision']
                external = AnnotationStore(store.path, ROM_SHA256)
                external.save('bg', {'role': 'obstacle'})
                request = urllib.request.Request(url + '/api/annotation/bg',
                                                 data=b'{"role":"floor"}', method='PUT',
                                                 headers={'If-Match': revision})
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(request)
                self.assertEqual(error.exception.code, 409)
                error.exception.close()
                self.assertEqual(AnnotationStore(store.path, ROM_SHA256).tiles['bg']['role'], 'obstacle')
            finally:
                server.shutdown()
                server.server_close()
                thread.join()

    def test_profile_refresh_and_bidirectional_save_preserve_other_layers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / 'profile.toml'
            profile.write_text('schema_version = 20\n[game]\nrom_sha256 = "' + ROM_SHA256 +
                               '"\n[[assets]]\ntile_hash = "bg"\nheight = [1, 2, 3]\n'
                               'tile_role = "wall"\ntile_semantics_authored = true\n'
                               'tile_semantic_normals = ["north"]\n')
            marker = root / 'marker.json'
            marker.write_text(json.dumps({'profile': str(profile)}))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
            old_revision = store.refresh()
            self.assertEqual(store.tiles['bg']['normals'], ['north'])
            store.save_group(['bg'], {'role': 'obstacle', 'note': 'changed\nsecond line'}, old_revision)
            parsed = tomllib.loads(profile.read_text())
            self.assertEqual(parsed['assets'][0]['height'], [1, 2, 3])
            self.assertEqual(parsed['assets'][0]['tile_role'], 'obstacle')
            self.assertEqual(parsed['assets'][0]['tile_semantic_note'], 'changed\nsecond line')
            with self.assertRaisesRegex(AtlasError, 'metadata changed'):
                store.save_group(['bg'], {'role': 'floor'}, old_revision)
            profile.write_text(profile.read_text().replace('"obstacle"', '"floor"'))
            with self.assertRaisesRegex(AtlasError, 'profile changed'):
                store.save_group(['bg'], {'role': 'other'})
            store.refresh()
            self.assertEqual(store.tiles['bg']['role'], 'floor')
            self.assertEqual(json.loads(store.path.read_text())['tiles']['bg']['role'], 'floor')
            store.save_group(['bg'], {'role': 'unknown'})
            store.refresh()
            self.assertEqual(store.tiles['bg']['role'], 'unknown')
            profile.write_text(profile.read_text() + '\n[[rules]]\ntile_hash = "bg"\nmaterial = "stone"\n')
            store.refresh()
            store.save_group(['new'], {'role': 'floor'})
            parsed = tomllib.loads(profile.read_text())
            self.assertEqual(parsed['assets'][-1]['tile_hash'], 'new')
            self.assertEqual(parsed['rules'][0]['material'], 'stone')
            self.assertLess(profile.read_text().index('tile_hash = "new"'), profile.read_text().index('[[rules]]'))

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
            store.save("bg", {"role": "obstacle", "normals": [],
                              "context_dependent": True, "note": "upper railing"})
            loaded = AnnotationStore(path, ROM_SHA256)
            self.assertEqual(loaded.tiles["bg"]["role"], "obstacle")
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
