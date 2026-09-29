import unittest

from alttp_validate_room_heights import validate


class Cell:
    def __init__(self, x, y, tile_hash="floor", hflip=False, vflip=False):
        self.x, self.y, self.tile_hash = x, y, tile_hash
        self.hflip, self.vflip = hflip, vflip


class Snapshot:
    room = 0x60
    def __init__(self):
        self.cells = [[], [Cell(x, y) for y in range(64) for x in range(64)]]
    def cell(self, layer, x, y):
        return self.cells[layer][y * 64 + x]


def graph(solved=None):
    return {"format": "alttp-doorway-graph-review-v1", "room_id": 0x60,
            "planes": [{"reachable_body_cells": [[1, 2], [2, 2]]},
                       {"reachable_body_cells": [[1, 2]]}],
            "elevation": {"cells_plane_x_y_level": solved or [],
                          "conflicting_components": [], "unresolved_stairs": []}}


def attrs():
    result = [bytearray([3] * 4096), bytearray([3] * 4096)]
    for plane in result:
        plane[2 * 64 + 1] = plane[2 * 64 + 2] = 0
    return tuple(bytes(p) for p in result)


PROFILE = {"assets": [{"tile_hash": "floor", "height": [0] * 64}], "asset_groups": {}}
SOLVED = [[0, 1, 2, 1], [0, 2, 2, 1], [1, 1, 2, 0]]


class RoomHeightValidationTests(unittest.TestCase):
    def test_requires_every_walkable_placement_on_each_plane(self):
        report = validate(graph(SOLVED[:-1]), 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertEqual(report["missing_placements"][0]["plane"], 1)

    def test_false_pass_every_hash_has_placeholder_but_no_placement_solution(self):
        # Regression for the old gate: existence of a zero array on the hash is
        # not effective geometry when no placement level/base was assigned.
        report = validate(graph([]), 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertEqual(len(report["missing_placements"]), 3)
        self.assertEqual(report["valid_walkable_placements"], 0)

    def test_plane_overlap_and_complete_effective_geometry(self):
        report = validate(graph(SOLVED), 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertTrue(report["success"])
        self.assertEqual(report["per_plane_walkable_denominator"], [2, 1])
        self.assertEqual(report["plane_overlap_cells"], 1)
        self.assertEqual(report["valid_walkable_placements"], 3)

    def test_fallback_requires_placement_ramp_and_direction(self):
        collision = [bytearray(p) for p in attrs()]
        collision[0][2 * 64 + 3] = 1
        report = validate(graph(SOLVED), 50, snapshot=Snapshot(),
                          attributes=tuple(bytes(p) for p in collision), profile=PROFILE)
        ramps = [p for p in report["placements"] if p["kind"] == "collidable_fallback"]
        self.assertTrue(ramps)
        self.assertTrue(all(p["local_source"] == "placement_ramp_0_7" for p in ramps))
        self.assertTrue(all(min(p["local_height"]) == 0 and max(p["local_height"]) == 7 for p in ramps))

    def test_same_level_corner_composes_minimum_outward_ramps(self):
        collision = [bytearray(p) for p in attrs()]
        collision[0][2 * 64] = 1  # West of support (1,2).
        collision[0][1 * 64 + 1] = 1  # North of the same support.
        # Make the northwest corner itself border both solved cells (1,2) and
        # (2,1) in a tiny explicit graph.
        fixture = graph([[0, 1, 2, 1], [0, 2, 1, 1], [0, 2, 2, 1]])
        fixture["planes"][0]["reachable_body_cells"] = [[1, 2], [2, 1], [2, 2]]
        fixture["planes"][1]["reachable_body_cells"] = []
        collision[0][1 * 64 + 2] = 0
        report = validate(fixture, 50, snapshot=Snapshot(),
                          attributes=tuple(bytes(p) for p in collision), profile=PROFILE)
        corner = next(p for p in report["placements"]
                      if p["kind"] == "collidable_fallback" and p["x"] == 1 and p["y"] == 1)
        self.assertEqual(corner["displayed_directions"], ["west", "north"])
        self.assertEqual(corner["composition"], "minimum_outward_ramps")
        self.assertEqual(corner["local_height"][0], 7)
        self.assertEqual(corner["local_height"][7], 0)
        self.assertEqual(corner["local_height"][56], 0)
        self.assertEqual(corner["local_height"][63], 0)

    def test_compiled_map_must_contain_effective_base(self):
        height_map = [255] * 4096
        report = validate(graph(SOLVED), 50, height_map, snapshot=Snapshot(),
                          attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertTrue(report["compiled_map_missing"])


if __name__ == "__main__":
    unittest.main()
