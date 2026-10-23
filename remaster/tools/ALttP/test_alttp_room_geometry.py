#!/usr/bin/env python3
"""Portable checks for the first ALTTP room geometry constraints."""

from __future__ import annotations

from types import SimpleNamespace
from pathlib import Path
import json
import struct
import tempfile
import unittest

from alttp_room_geometry import (
    Cell, RoomProvenance, attachment_proposals, components, face_regions, flat_art_candidates,
    load_tile_semantics, solve_levels, transform_face,
    seam_offset,
)


class RoomGeometryTests(unittest.TestCase):
    def test_corner_keeps_two_facings_and_mirrors_both_pixel_masks(self) -> None:
        normals = []
        for y in range(8):
            for x in range(8):
                if y == 0:
                    normals.extend((128, 128, 255))  # flat cap
                elif x < 4:
                    normals.extend((255, 128, 128))  # east-facing wall face
                else:
                    normals.extend((128, 255, 128))  # south-facing wall face
        regions = face_regions(normals)
        self.assertEqual({region.facing for region in regions}, {(1, 0), (0, 1)})
        self.assertEqual(sum(region.mask.bit_count() for region in regions), 56)
        flipped = [transform_face(region, True, True) for region in regions]
        self.assertEqual({region.facing for region in flipped}, {(-1, 0), (0, -1)})
        self.assertEqual(sum(region.mask.bit_count() for region in flipped), 56)
        self.assertTrue(all(region.mask & (255 << 56) == 0 for region in flipped))

    def test_conflicting_stair_loop_stays_unresolved(self) -> None:
        constraints = [
            {"object": "stairs-a", "low_component": 0, "high_component": 1, "rise": 50},
            {"object": "stairs-b", "low_component": 1, "high_component": 0, "rise": 50},
        ]
        levels, conflicts = solve_levels(2, constraints)
        self.assertEqual(levels, [None, None])
        self.assertTrue(conflicts)

    def test_repeated_zero_height_art_excludes_raised_out_of_bounds_tile(self) -> None:
        front = [128, 128, 255] * 64
        layer = ([Cell(0, 0, 1, 0, "flat", False, False)] * 16 +
                 [Cell(0, 0, 1, 0, "out", False, False)] * 40)
        snapshot = SimpleNamespace(cells=((), layer))
        profile = {"asset_groups": {"wall_tops": {"tile_hashes": []}}}
        assets = {"flat": {"height": [0] * 64, "normal_xyz": front},
                  "out": {"height": [50] * 64, "normal_xyz": front}}
        candidates, inferred = flat_art_candidates(snapshot, profile, assets)
        self.assertEqual(candidates, {"flat"})
        self.assertEqual(inferred, {"flat"})

    def test_object_writer_boundary_separates_identical_wall_top_art(self) -> None:
        cells = tuple(Cell(i % 64, i // 64, 1, 0, "top" if i in (0, 1) else "other",
                           False, False) for i in range(4096))
        joined, _ = components(cells, {"top"})
        self.assertEqual(joined[0], joined[1])
        owners = (1, 2) + (0,) * 4094
        separated, found = components(cells, {"top"}, owners, {"top"})
        self.assertNotEqual(separated[0], separated[1])
        self.assertEqual([item["tiles"] for item in found], [1, 1])

    def test_provenance_rejects_a_tilemap_from_another_room_state(self) -> None:
        cells = tuple(Cell(i % 64, i // 64, 1, 0, "art", False, False)
                      for i in range(4096))
        snapshot = SimpleNamespace(room=85, cells=(cells, cells))
        data = (b"ALTPRV1\0" + struct.pack("<HH", 85, 1) +
                struct.pack("<5H", 1, 0, 0, 0, 0) +
                struct.pack("<8192H", *([0] * 8192)) +
                struct.pack("<8192H", *([0] * 8192)))
        self.assertEqual(len(RoomProvenance(data, snapshot).objects), 1)
        changed = bytearray(data)
        struct.pack_into("<H", changed, len(changed) - 2, 1)
        with self.assertRaisesRegex(ValueError, "tilemap changed"):
            RoomProvenance(bytes(changed), snapshot)

    def test_semantic_corner_override_keeps_two_masks(self) -> None:
        value = {"format": "alttp-tile-surface-semantics-v1",
                 "faces": {"v1:4bpp:abc": [
                     {"facing": "east", "mask": "0x0000000000000001"},
                     {"facing": "south", "mask": "0x0000000000000002"}]}}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "semantics.json"
            path.write_text(json.dumps(value))
            regions = load_tile_semantics(path)["v1:4bpp:abc"]
            self.assertEqual({region.facing for region in regions}, {(1, 0), (0, 1)})
            self.assertEqual({region.facing for region in
                              (transform_face(item, True, True) for item in regions)},
                             {(-1, 0), (0, -1)})
            value["faces"]["v1:4bpp:abc"][1]["mask"] = "0x1"
            path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ValueError, "overlapping"):
                load_tile_semantics(path)

    def test_wall_touching_both_levels_is_not_given_one_offset(self) -> None:
        cells = tuple(Cell(i % 64, i // 64, 1, 0,
                           "face" if i == 65 else "door" if i == 5 * 64 + 5 else "other",
                           False, False) for i in range(4096))
        snapshot = SimpleNamespace(cells=(cells, cells),
                                   cell=lambda layer, x, y: cells[y * 64 + x])
        owners = {(1, 1): 1, (5, 5): 2}
        provenance = SimpleNamespace(
            owner=lambda layer, x, y: owners.get((x, y), 0),
            objects=[{"is_door": 0}, {"is_door": 1}])
        rows = [[None] * 64 for _ in range(64)]
        rows[1][0], rows[1][2], rows[5][7] = 0, 50, 50
        groups = {"wall_faces": {"tile_hashes": ["face"]},
                  "wall_tops": {"tile_hashes": []},
                  "stair_rails": {"tile_hashes": []},
                  "stair_treads": {"tile_hashes": []}}
        assets = {"face": {"height": [0] * 64}, "other": {"height": [0] * 64}}
        result = {item["writer"]: item for item in
                  attachment_proposals(snapshot, provenance, rows, groups, assets, 50)}
        self.assertEqual(result[1]["status"], "conflicting_seam_offsets")
        self.assertIsNone(result[1]["proposed_height_offset"])
        self.assertEqual(result[2]["status"], "nearest_door_threshold")
        self.assertEqual(result[2]["door_floor_distance"], 2)
        self.assertEqual(result[2]["proposed_height_offset"], 50)

    def test_wall_edge_height_uses_placed_flip_before_proposing_offset(self) -> None:
        assets = {"face": {"height": [x * 47 // 7 for y in range(8) for x in range(8)]},
                  "floor": {"height": [0] * 64}}
        floor = Cell(1, 0, 1, 0, "floor", False, False)
        normal = Cell(0, 0, 1, 0, "face", False, False)
        flipped = Cell(0, 0, 1, 0, "face", True, False)
        self.assertEqual(seam_offset(normal, floor, 1, 0, 50, assets, 50), 0)
        self.assertEqual(seam_offset(flipped, floor, 1, 0, 50, assets, 50), 50)

    def test_wall_overlay_object_inherits_nearest_solved_landing(self) -> None:
        cells = tuple(Cell(i % 64, i // 64, 1, 0, "overlay", False, False)
                      for i in range(4096))
        snapshot = SimpleNamespace(cells=(cells, cells),
                                   cell=lambda layer, x, y: cells[y * 64 + x])
        provenance = SimpleNamespace(
            owner=lambda layer, x, y: 1 if (x, y) == (5, 4) else 0,
            objects=[{"phase": 1, "ordinal": 56, "is_door": 0}])
        floors = [[None] * 64 for _ in range(64)]
        floors[6][5] = 50
        groups = {name: {"tile_hashes": []} for name in
                  ("wall_faces", "wall_tops", "stair_rails", "stair_treads")}
        proposals = attachment_proposals(snapshot, provenance, floors, groups,
                                         {"overlay": {"height": [0] * 64}}, 50,
                                         {56: (0, 0x05)})
        self.assertEqual(len(proposals), 1)
        self.assertEqual(proposals[0]["status"], "wall_overlay_floor")
        self.assertEqual(proposals[0]["nearby_floor_distance"], 2)
        self.assertEqual(proposals[0]["proposed_height_offset"], 50)


if __name__ == "__main__":
    unittest.main()
