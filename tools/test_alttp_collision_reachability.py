import unittest
import json
import os
from pathlib import Path
import subprocess
import tempfile

from alttp_collision_reachability import (body_cells, floor_levels, occupied_cells, plain_footprint,
                                          reachable, read_attributes, review,
                                          doorway_graph, doorway_review, stair_edges_from_trace)


class CollisionReachabilityTest(unittest.TestCase):
    def test_stairs_assign_relative_floor_levels(self):
        positions = [{(x, 10) for x in range(4)} | {(x, 30) for x in range(4)}, set()]
        stairs = [{"from": [0, 1, 10], "to": [0, 1, 30]}]
        result = floor_levels(positions, stairs,
                              [{"tile_xy": [0, 2], "high_side": "north"}])
        levels = {(plane, x, y): level for plane, x, y, level
                  in result["cells_plane_x_y_level"]}
        self.assertEqual(result["solved_components"], 2)
        self.assertEqual(levels[(0, 0, 2)], 1)
        self.assertEqual(levels[(0, 0, 6)], 0)

    def test_unoriented_stair_keeps_components_unsolved(self):
        positions = [{(1, 10), (1, 30)}, set()]
        result = floor_levels(positions, [{"from": [0, 1, 10], "to": [0, 1, 30]}], [])
        self.assertEqual(result["cells_plane_x_y_level"], [])
        self.assertEqual(result["unresolved_stairs"][0]["reason"], "missing_orientation")

    def test_room055_wet_stair_raises_south_landing(self):
        graph = Path("build/remaster-geometry/surface-audit-room-local/room-055-doorway-graph.json")
        if not graph.is_file():
            self.skipTest("room 055 local reachability review is unavailable")
        result = json.loads(graph.read_text())
        levels = {(plane, x, y): level for plane, x, y, level
                  in result["elevation"]["cells_plane_x_y_level"]}
        self.assertEqual(levels[(0, 15, 49)], 0)
        self.assertEqual(levels[(0, 15, 56)], 1)

    def test_stairs_only_expand_when_source_is_reached(self):
        plane = bytes(0 if 4 <= x < 10 and 4 <= y < 12 else 1
                      for y in range(64) for x in range(64))
        edge = {"from": [0, 40, 40], "to": [1, 40, 40]}
        graph, positions = doorway_graph((plane, plane), (0, 40, 40), [edge])
        self.assertTrue(graph["stair_edges"][0]["reached"])
        self.assertIn((40, 40), positions[1])
        edge["from"] = [0, 200, 200]
        graph, positions = doorway_graph((plane, plane), (0, 40, 40), [edge])
        self.assertFalse(graph["stair_edges"][0]["reached"])
        self.assertEqual(positions[1], set())
        self.assertTrue(all(32 <= x < 65 for x, _ in positions[0]))

    def test_trace_edge_is_directed_and_spans_complete_staircase(self):
        trace = [{"frame": i, "plane": p, "x": 40, "y": y, "submodule": sub}
                 for i, p, y, sub in ((0, 0, 80, 0), (1, 0, 79, 16),
                                      (2, 1, 78, 16), (3, 1, 40, 0))]
        edges = stair_edges_from_trace(trace, 96)
        self.assertEqual(len(edges), 1)
        self.assertEqual(edges[0]["from"], [0, 40, 80])
        self.assertEqual(edges[0]["to"], [1, 40, 40])
        self.assertEqual(stair_edges_from_trace(trace[:-1], 96), [])

    def test_room060_door_stairs_and_north_underpass(self):
        renderer_name = os.environ.get("ALTTP_ROOM_RENDERER")
        if not renderer_name:
            self.skipTest("set ALTTP_ROOM_RENDERER to initialized adapter")
        renderer = Path(renderer_name).resolve()
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory) / "door.raw"
            route = Path(directory) / "route.raw"
            north = Path(directory) / "north.raw"
            for path, args in ((raw, []), (route, ["N160,S120"]), (north, ["N300"])):
                subprocess.run([str(renderer), "--linked", "0x60", str(path), "-1", "0", "0", "0", *args],
                               cwd=renderer.parent, check=True, capture_output=True)
            data = Path(str(raw) + ".attr").read_bytes()
            state = json.loads(Path(str(raw) + ".state.json").read_text())
            trace = [json.loads(line) for line in Path(str(route) + ".route.jsonl").read_text().splitlines()]
            result = doorway_review(96, data, state, trace)
            self.assertEqual(state["link_xy"], [376, 472])
            self.assertEqual(result["playback_check"]["missing_positions"], [])
            self.assertEqual(len(result["graph"]["stair_edges"]), 2)
            self.assertTrue(all(edge["reached"] for edge in result["graph"]["stair_edges"]))
            _, positions = doorway_graph(read_attributes(data, 96), (0, 376, 472),
                                         stair_edges_from_trace(trace, 96))
            north_trace = [json.loads(line) for line in Path(str(north) + ".route.jsonl").read_text().splitlines()]
            for item in north_trace:
                if item["submodule"] == 0:
                    self.assertIn((item["x"], item["y"]), positions[item["plane"]])
            # Lower passage exists beneath the visible transverse bridge.
            self.assertIn((376, 144), positions[1])
            self.assertIn((376, 144), positions[0])  # separately walkable upper bridge
            self.assertNotIn((264, 200), positions[0])  # outer wall
            self.assertNotIn((264, 200), positions[1])
            self.assertEqual(read_attributes(data, 96),
                             read_attributes(Path(str(route) + ".attr").read_bytes(), 96))
            body = set(map(tuple, result["planes"][0]["reachable_body_cells"]))
            centers = set(map(tuple, result["planes"][0]["south_center_cells"]))
            # Cells below and right of both torch pedestals are covered by a
            # reachable collision body even though its south-center sample
            # cannot enter those cells.
            for cell in ((44, 56), (45, 56), (46, 54), (46, 55),
                         (50, 56), (51, 56), (52, 54), (52, 55)):
                self.assertIn(cell, body)
                self.assertNotIn(cell, centers)

    def test_room_and_plane_header(self):
        data = b"ALTPAT1\0" + bytes((0x60, 0)) + bytes(4096) + bytes([1]) * 4096
        upper, lower = read_attributes(data, 0x60)
        self.assertEqual((upper[0], lower[0]), (0, 1))
        with self.assertRaises(ValueError):
            read_attributes(data, 0x61)

    def test_full_footprint_stops_at_collision_boundary(self):
        # Wall at x=4, but the full-footprint guard keeps Link's body left of it.
        plane = bytes(1 if x == 4 or y >= 6 else 0
                      for y in range(64) for x in range(64))
        self.assertTrue(plain_footprint(plane, 8, 8))
        self.assertFalse(plain_footprint(plane, 17, 8))
        positions = reachable(plane, [(8, 8)])
        self.assertTrue(positions)
        self.assertTrue(all(x <= 16 for x, _ in positions))
        self.assertNotIn((5, 3), map(tuple, occupied_cells(positions)))

    def test_body_projection_fills_cells_below_and_right_of_collider(self):
        plane = bytes(1 if (x, y) == (4, 4) else 0
                      for y in range(64) for x in range(64))
        positions = {(24, 24), (32, 24), (24, 32)}
        body = set(map(tuple, body_cells(positions, plane)))
        self.assertNotIn((4, 4), body)
        self.assertIn((5, 4), body)  # right of collider
        self.assertIn((4, 5), body)  # below collider

    def test_nonblocking_art_is_indistinguishable_from_plain_ground(self):
        # This is the important limit of the algorithm, even if the cells
        # represent a pictured pillar, wall, or upper bridge.
        plane = bytes(4096)
        positions = reachable(plane, [(8, 8)], stride=4)
        self.assertIn((30, 30), map(tuple, occupied_cells(positions)))
        with self.assertRaisesRegex(ValueError, "seed"):
            reachable(bytes([1]) * 4096, [(8, 8)])

    def test_other_plane_is_separate(self):
        data = b"ALTPAT1\0" + bytes((0x60, 0)) + bytes(4096) + bytes([1]) * 4096
        result = review(0x60, data, [[(8, 8)], []], 4)
        self.assertGreater(result["planes"][0]["reachable_foot_positions"], 0)
        self.assertEqual(result["planes"][1]["reachable_body_cells"], [])


if __name__ == "__main__":
    unittest.main()
