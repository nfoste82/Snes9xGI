#!/usr/bin/env python3
"""Whole-scene checks for conservative, order-independent surface rules."""

from __future__ import annotations

from copy import deepcopy
import json
import unittest

from alttp_surface_rules import FULL_MASK, solve
from alttp_surface_audit import (floor_coverage, linked_room_image, stair_evidence,
                                 write_stair_overlay, write_stair_review)
from types import SimpleNamespace
from pathlib import Path
import tempfile
import os
import subprocess
import tomllib
import zlib


def region(name, role, contacts=None, height=0, mask=FULL_MASK):
    return {"id": name, "role": role, "mask": hex(mask), "height": height,
            "contacts": contacts or {}}


def contact(role, rise, optional=True):
    return {"role": role, "rise": rise, "optional": optional}


def scene(hashes, anchors=None):
    width = len(hashes)
    return {"format": "alttp-surface-scene-v1", "width": width, "height": 1,
            "rom_sha256": "synthetic-rom", "state_key": "test", "geometry_evidence": "test fixture",
            "placements": [{"layer": layer, "x": x, "y": 0, "tile_hash":
                            "empty" if layer == 0 else tile, "hflip": False,
                            "vflip": False, "writer": 0}
                           for layer in range(2) for x, tile in enumerate(hashes)],
            "anchors": anchors or []}


def templates():
    return {"format": "alttp-surface-rules-v1", "templates": [
        {"id": "empty-art", "tile_hash": "empty", "regions": [region("art", "other")]},
        {"id": "floor", "tile_hash": "floor", "regions": [region("surface", "floor", {
            "west": contact("stair", 0), "east": contact("stair", 50)})]},
        {"id": "stairs", "tile_hash": "stair", "regions": [region("ramp", "stair", {
            "west": contact("floor", -50), "east": contact("floor", 0)})]},
    ]}


def floor_offsets(report):
    return {item["at"][1]: item["height_offset"] for item in report["audit"]
            if item.get("role") == "floor"}


def stair_scene(high_side="north", anchor_side="north", anchor_height=50):
    s = {"format": "alttp-surface-scene-v1", "width": 6, "height": 6,
         "rom_sha256": "synthetic-rom", "state_key": "test", "geometry_evidence": "test fixture",
         "placements": [{"layer": layer, "x": x, "y": y,
                         "tile_hash": ("floor" if layer == 1 and y in (0, 5) and x in (2, 3)
                                       else "stair" if layer == 1 and 1 <= x <= 4 and 1 <= y <= 4
                                       else "empty"), "hflip": False, "vflip": False, "writer": 0}
                        for layer in range(2) for y in range(6) for x in range(6)],
         "stairs": [{"id": "handler-stair", "layer": 1, "x": 1, "y": 1,
                     "high_side": high_side, "rise": 50}],
         "anchors": [{"layer": 1, "x": 2, "y": 0 if anchor_side == "north" else 5,
                      "region": "surface", "height": anchor_height,
                      "evidence": "independent floor reference"}]}
    rules = {"format": "alttp-surface-rules-v1", "templates": [
        {"id": "floor", "tile_hash": "floor", "regions": [region("surface", "floor", {
            "east": contact("floor", 0), "west": contact("floor", 0)})]},
        {"id": "stair-art", "tile_hash": "stair", "regions": [region("ramp", "stair")]},
        {"id": "unmodeled", "tile_hash": "empty", "regions": [region("art", "excluded")]},
    ]}
    return s, rules


class SurfaceRulesTests(unittest.TestCase):
    def test_stair_orientation_solves_upper_entrance_without_using_entry_side(self):
        s, rules = stair_scene(anchor_side="north")
        report = solve(s, rules)
        floors = {(item["at"][1], item["at"][2]): item["height_offset"]
                  for item in report["audit"] if item.get("role") == "floor"}
        self.assertEqual(floors, {(2, 0): 50, (3, 0): 50, (2, 5): 0, (3, 5): 0})
        self.assertEqual(len(report["stair_constraints"]), 18)
        self.assertFalse(report["issues"])
        self.assertTrue(report["publishable"])

    def test_reversed_stair_and_lower_anchor(self):
        s, rules = stair_scene(high_side="south", anchor_side="north", anchor_height=0)
        report = solve(s, rules)
        floors = {(item["at"][1], item["at"][2]): item["height_offset"]
                  for item in report["audit"] if item.get("role") == "floor"}
        self.assertEqual(floors[(2, 0)], 0)
        self.assertEqual(floors[(2, 5)], 50)
        self.assertEqual(floors[(3, 0)], 0)

    def test_stair_and_landing_order_changes_do_not_change_the_answer(self):
        s, rules = stair_scene()
        expected = solve(s, rules)
        s["placements"].reverse()
        rules["templates"].reverse()
        self.assertEqual(expected, solve(s, rules))

    def test_missing_stair_landing_is_a_room_wide_failure(self):
        s, rules = stair_scene()
        s["placements"][6 * 6 + 5 * 6 + 3]["tile_hash"] = "empty"
        report = solve(s, rules)
        self.assertEqual(report["stair_constraints"], [])
        self.assertTrue(any(issue["reason"] == "unclassified_stair_landings" for issue in report["issues"]))
        self.assertFalse(report["publishable"])

    def test_partial_stair_footprint_does_not_count_as_complete(self):
        s, rules = stair_scene()
        s["placements"][6 * 6 + 2 * 6 + 2]["tile_hash"] = "empty"
        report = solve(s, rules)
        self.assertTrue(any(issue["reason"] == "unclassified_stair_footprint"
                            for issue in report["issues"]))
        self.assertFalse(report["publishable"])

    def test_overwritten_stair_art_requires_support_evidence(self):
        s, rules = stair_scene()
        s["stairs"][0]["writer"] = 7
        for cell in s["placements"]:
            if cell["layer"] == 1 and cell["tile_hash"] == "stair":
                cell["writer"] = 7
        s["placements"][6 * 6 + 2 * 6 + 2]["writer"] = 8
        report = solve(s, rules)
        self.assertTrue(any(issue["reason"] == "overwritten_stair_region_requires_support_provenance"
                            for issue in report["issues"]))
        self.assertEqual(report["stair_constraints"], [])

    def test_same_stair_rule_solves_two_rises_and_is_order_independent(self):
        source = scene(["floor", "stair", "floor", "stair", "floor"],
                       [{"layer": 1, "x": 0, "y": 0, "region": "surface",
                         "height": 0, "evidence": "reviewed lower landing"}])
        rules = templates()
        report = solve(source, rules)
        self.assertEqual(floor_offsets(report), {0: 0, 2: 50, 4: 100})
        self.assertEqual(report["contact_constraints"], 4)
        self.assertFalse(report["issues"])
        reordered = deepcopy(source)
        reordered["placements"].reverse()
        rules["templates"].reverse()
        self.assertEqual(json.dumps(report, sort_keys=True),
                         json.dumps(solve(reordered, rules), sort_keys=True))

    def test_unanchored_component_does_not_become_ground_zero(self):
        report = solve(scene(["floor", "stair", "floor"]), templates())
        self.assertEqual(report["contact_constraints"], 2)
        self.assertEqual(floor_offsets(report), {0: None, 2: None})
        self.assertTrue(any(item["status"] == "relative_only" and item["relative_offset"] == 50
                            for item in report["audit"]))
        self.assertFalse(report["publishable"])

    def test_incompatible_second_anchor_invalidates_entire_connected_group(self):
        s = scene(["floor", "stair", "floor"], [
            {"layer": 1, "x": x, "y": 0, "region": "surface", "height": 0,
             "evidence": f"independent check {x}"} for x in (0, 2)])
        report = solve(s, templates())
        self.assertTrue(any(item["reason"] == "conflicting_anchors" for item in report["issues"]))
        self.assertTrue(all(item["status"] == "conflict" for item in report["audit"]
                            if item.get("role") in ("floor", "stair")))

    def test_unclassified_tile_is_reported_even_next_to_solved_floor(self):
        report = solve(scene(["floor", "unrecognized", "floor"], [
            {"layer": 1, "x": 0, "y": 0, "region": "surface", "height": 0,
             "evidence": "known landing"}]), templates())
        self.assertEqual(report["placements"], 6)
        self.assertIn({"at": [1, 1, 0], "status": "unknown", "reason": "no_semantic_template", "writer": 0},
                      report["audit"])
        self.assertIsNone(floor_offsets(report)[2])

    def test_entire_contact_must_agree_not_just_its_median(self):
        rules = templates()
        rules["templates"].append({"id": "broken-wall", "tile_hash": "wall",
                                   "regions": [region("face", "wall", {
                                       "east": contact("floor", 0, False)},
                                       [0] * 32 + [100] * 32)]})
        rules["templates"][1]["regions"][0]["contacts"]["west"] = contact("wall", 0, False)
        report = solve(scene(["wall", "floor"]), rules)
        self.assertTrue(any(item["reason"] == "inconsistent_pixel_heights" for item in report["issues"]))
        self.assertEqual(report["contact_constraints"], 0)

    def test_flip_transforms_contact_and_mask_and_detects_missing_contact(self):
        rules = {"format": "alttp-surface-rules-v1", "templates": [
            {"id": "left", "tile_hash": "face", "regions": [region("surface", "wall", {
                "west": contact("wall", 0, False)}, mask=sum(1 << (8*y) for y in range(8)))]},
            {"id": "right", "tile_hash": "other", "regions": [region("surface", "wall", {
                "west": contact("wall", 0, False)}, mask=sum(1 << (8*y) for y in range(8)))]},
            {"id": "empty", "tile_hash": "empty", "regions": [region("art", "other")]},
        ]}
        s = scene(["face", "other"])
        s["placements"][2]["hflip"] = True
        report = solve(s, rules)
        self.assertEqual(report["contact_constraints"], 1)
        self.assertFalse(report["issues"])
        s["placements"][2]["hflip"] = False
        report = solve(s, rules)
        self.assertTrue(any(issue["reason"] == "missing_contact" for issue in report["issues"]))

    def test_optional_edge_cannot_hide_incompatible_claimed_neighbor(self):
        rules = {"format": "alttp-surface-rules-v1", "templates": [
            {"id": "a", "tile_hash": "a", "regions": [region("face", "wall", {
                "east": contact("wall", 0)})]},
            {"id": "b", "tile_hash": "b", "regions": [region("face", "wall", {
                "west": contact("wall", -50)})]},
            {"id": "empty", "tile_hash": "empty", "regions": [region("art", "other")]},
        ]}
        report = solve(scene(["a", "b"]), rules)
        self.assertTrue(any(issue["reason"] == "incompatible_contact" for issue in report["issues"]))
        self.assertEqual(report["contact_constraints"], 0)

    def test_requires_complete_maps_and_rejects_overlapping_semantics(self):
        s = scene(["floor"])
        s["placements"].pop()
        with self.assertRaisesRegex(ValueError, "every BG1 and BG2"):
            solve(s, templates())
        s = scene(["floor"])
        rules = templates()
        rules["templates"][1]["regions"].append(region("duplicate", "floor"))
        with self.assertRaisesRegex(ValueError, "overlapping"):
            solve(s, rules)

    def test_same_art_with_two_usages_requires_explicit_template_selection(self):
        rules = templates()
        rules["templates"].append({"id": "alternate-floor", "tile_hash": "floor",
                                   "regions": [region("surface", "floor", height=5)]})
        s = scene(["floor"])
        report = solve(s, rules)
        self.assertTrue(any(item.get("reason") == "ambiguous_template_usage"
                            for item in report["audit"]))
        s["placements"][1]["template_id"] = "alternate-floor"
        report = solve(s, rules)
        self.assertEqual(report["region_counts"].get("unknown", 0), 0)
        self.assertEqual(report["region_counts"]["relative_only"], 2)

    def test_unconsumed_anchor_is_a_reported_error(self):
        s = scene(["unrecognized"], [{"layer": 1, "x": 0, "y": 0,
                                      "region": "floor", "height": 0,
                                      "evidence": "review"}])
        report = solve(s, templates())
        self.assertTrue(any(issue["reason"] == "unconsumed_anchor" for issue in report["issues"]))
        self.assertFalse(report["publishable"])

    def test_missing_rom_state_or_evidence_cannot_publish_a_solved_graph(self):
        s, rules = stair_scene()
        self.assertTrue(solve(s, rules)["publishable"])
        for key in ("rom_sha256", "state_key", "geometry_evidence"):
            incomplete = deepcopy(s)
            incomplete.pop(key)
            self.assertFalse(solve(incomplete, rules)["publishable"])

    def test_stair_evidence_overlays_mark_partial_rewrites_without_reassigning_writer(self):
        s, _ = stair_scene()
        cells = {}
        for cell in s["placements"]:
            if cell["layer"] == 1:
                writer = 1 if cell["tile_hash"] == "stair" else 0
                if (cell["x"], cell["y"]) == (2, 2):
                    writer = 2
                cells[(cell["x"], cell["y"])] = SimpleNamespace(
                    tile_hash=cell["tile_hash"], word=0, hflip=False, vflip=False, writer=writer)
        snapshot = SimpleNamespace(room=1, cell=lambda layer, x, y: cells[(x, y)])
        provenance = SimpleNamespace(owner=lambda layer, x, y: cells[(x, y)].writer,
            objects=[{"phase": 1, "ordinal": 3}, {"phase": 3, "ordinal": 2}])
        record = {**s["stairs"][0], "writer": 1}
        evidence = stair_evidence(snapshot, provenance, [record])
        self.assertEqual(evidence["stairs"][0]["source_writer_cells"], 15)
        self.assertEqual(evidence["stairs"][0]["overwritten_footprint_cells"], 1)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_stair_overlay(root / "map.png", evidence)
            write_stair_review(root / "review.svg", evidence, (root / "map.png").read_bytes())
            self.assertTrue((root / "map.png").read_bytes().startswith(b"\x89PNG"))
            self.assertIn("pink = overwritten stair", (root / "review.svg").read_text())

    def test_linked_room_background_has_the_same_tilemap_origin_as_stair_evidence(self):
        renderer_name = os.environ.get("ALTTP_ROOM_RENDERER")
        if not renderer_name:
            self.skipTest("set ALTTP_ROOM_RENDERER for the local ROM-backed alignment check")
        renderer = Path(renderer_name).resolve()
        from alttp_room_geometry import RoomProvenance, RoomSnapshot
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for room, stair_x, stair_y in ((0x55, 14, 52), (0x61, 10, 20)):
                raw = root / f"{room:03x}.raw"
                subprocess.run([str(renderer), "--linked", hex(room), str(raw), "-1", "0", "0", "0"],
                               cwd=renderer.parent, check=True, capture_output=True)
                snapshot = RoomSnapshot.read(raw)
                provenance = RoomProvenance.read(Path(str(raw) + ".prov"), snapshot)
                png = linked_room_image(renderer, room, root, raw)
                at = png.index(b"IDAT") + 4
                data = zlib.decompress(png[at:png.index(b"IEND") - 8])
                # The origin of the orange overlay is the stair's actual BG
                # placement. It must land on artwork, not on the blank region
                # introduced by the old 96-pixel gallery side buffer.
                self.assertGreater(provenance.owner(1, stair_x, stair_y), 0)
                px, py = stair_x * 8 + 16, stair_y * 8 + 16
                offset = py * (1 + 512 * 3) + 1 + px * 3
                rgb = tuple(data[offset:offset + 3])
                self.assertNotEqual(rgb, (0, 0, 0), (room, (stair_x, stair_y), rgb))

    def test_linked_horizontal_viewports_overlap_in_room_061(self):
        renderer_name = os.environ.get("ALTTP_ROOM_RENDERER")
        if not renderer_name:
            self.skipTest("set ALTTP_ROOM_RENDERER for the local ROM-backed seam check")
        renderer = Path(renderer_name).resolve()
        from alttp_room_geometry import RAW_ROW_BYTES
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            captures = {}
            for x in (0, 128, 256):
                raw = root / f"x{x}.raw"
                subprocess.run([str(renderer), "--linked", "0x61", str(raw), "-1", str(x), "0", "0"],
                               cwd=renderer.parent, check=True, capture_output=True)
                captures[x] = raw.read_bytes()

            def sample(data, x, y):
                return data[14 + y * RAW_ROW_BYTES + 30 + x * 6:
                            14 + y * RAW_ROW_BYTES + 30 + x * 6 + 2]

            for left, right in ((0, 128), (128, 256)):
                scores = {}
                for shift in range(-128, 129, 8):
                    locations = ((x, y) for y in range(32, 160, 4) for x in range(0, 128, 4)
                                 if 0 <= x + 128 + shift < 256)
                    scores[shift] = sum(sample(captures[left], x + 128 + shift, y) ==
                                        sample(captures[right], x, y) for x, y in locations)
                self.assertEqual(max(scores, key=scores.get), 0, (left, right, scores))
                self.assertGreater(scores[0], 900, (left, right, scores[0]))

    def test_floor_coverage_only_expands_complete_stair_landing_components(self):
        from alttp_room_geometry import Cell
        wall = "wall"
        flat = "flat"
        cells = [Cell(x, y, 1, 0, flat if x in (1, 2, 3, 6, 7) and y in (1, 2, 6, 7)
                      else wall, False, False) for y in range(64) for x in range(64)]
        snapshot = SimpleNamespace(room=42, cells=((), tuple(cells)))
        provenance = SimpleNamespace(owners=((), (0,) * 4096))
        profile = {"assets": [{"tile_hash": flat, "height": [0] * 64,
                               "normal_xyz": [128, 128, 255] * 64}],
                   "asset_groups": {"wall_tops": {"tile_hashes": [flat]}}}
        def landing(y):
            return [{"at": [x, y]} for x in (1, 2)]
        stair = {"status": "review", "id": "sample", "higher_landing": "north",
                 "lower_landing": "south", "north_landing": landing(2),
                 "south_landing": landing(6), "overwritten_footprint_cells": 0}
        report = floor_coverage(snapshot, provenance, {"stairs": [stair]}, profile)
        self.assertEqual({item["level"] for item in report["cells"]}, {"upper", "lower"})
        self.assertEqual(len(report["cells"]), 12)
        self.assertNotIn([6, 1], [item["at"] for item in report["cells"]])
        stair["south_landing"][1]["at"] = [4, 6]
        report = floor_coverage(snapshot, provenance, {"stairs": [stair]}, profile)
        self.assertEqual(report["cells"], [])
        self.assertEqual(report["stair_links"][0]["status"], "unresolved_landing")

    def test_real_room_floor_coverage_stays_on_landing_components(self):
        renderer_name = os.environ.get("ALTTP_ROOM_RENDERER")
        if not renderer_name:
            self.skipTest("set ALTTP_ROOM_RENDERER for the local ROM-backed coverage check")
        from alttp_room_geometry import RoomProvenance, RoomSnapshot
        from alttp_surface_audit import stair_records
        renderer = Path(renderer_name).resolve()
        atlas = Path("build/remaster-geometry/alttp-atlas.sqlite").resolve()
        profile = tomllib.loads(Path("build/remaster-geometry/alttp-profile-wall-review.toml").read_text())
        with tempfile.TemporaryDirectory() as directory:
            for room in (0x55, 0x60, 0x61):
                raw = Path(directory) / f"room-{room:03x}.raw"
                subprocess.run([str(renderer), "--linked", hex(room), str(raw), "-1", "0", "0", "0"],
                               cwd=renderer.parent, check=True, capture_output=True)
                snapshot = RoomSnapshot.read(raw)
                provenance = RoomProvenance.read(Path(str(raw) + ".prov"), snapshot)
                stairs = stair_evidence(snapshot, provenance, stair_records(atlas, room, 50, provenance))
                coverage = floor_coverage(snapshot, provenance, stairs, profile,
                                          Path(str(raw) + ".attr").read_bytes())
                colored = {(tuple(cell["at"]), cell["level"]) for cell in coverage["cells"]}
                if room == 0x60:
                    self.assertEqual(colored, set())
                    self.assertEqual(coverage["stair_links"][0]["status"], "unresolved_landing")
                    self.assertGreater(coverage["hidden_bg2_candidates"], 0)
                    self.assertNotIn(((46, 30), "lower"), colored)
                else:
                    for stair in stairs["stairs"]:
                        for side in ("north", "south"):
                            level = "upper" if side == stair["higher_landing"] else "lower"
                            for landing in stair[f"{side}_landing"]:
                                self.assertIn((tuple(landing["at"]), level), colored)
                    self.assertEqual({level for _, level in colored}, {"upper", "lower"})
                    self.assertEqual(coverage["stair_links"][0]["status"], "candidate")
                if room == 0x60:
                    self.assertIn((46, 30), {tuple(item["at"]) for item in coverage["covered_candidates"]})
                if room == 0x55:
                    unanchored = {tuple(item["at"]) for item in coverage["unanchored_candidates"]}
                    self.assertIn((18, 15), unanchored)  # disconnected north floor candidate
                    self.assertIn((45, 42), unanchored)  # chest-side floor candidate
                    self.assertNotIn(((45, 42), "lower"), colored)
                if room == 0x61:
                    self.assertNotIn(((16, 14), "upper"), colored)  # border needs assembly role
                self.assertLess(len(colored), 4096)

    def test_review_anchors_label_only_the_selected_disconnected_component(self):
        from alttp_room_geometry import Cell
        flat = "flat"
        cells = [Cell(x, y, 1, 0, flat if (x in (1, 2) and y in (1, 2)) or
                      (x in (10, 11) and y in (10, 11)) else "wall", False, False)
                 for y in range(64) for x in range(64)]
        snapshot = SimpleNamespace(room=1, cells=((), tuple(cells)))
        provenance = SimpleNamespace(owners=((), (0,) * 4096))
        profile = {"assets": [], "asset_groups": {"wall_tops": {"tile_hashes": [flat]}}}
        anchors = [{"at": [1, 1], "level": "lower", "evidence": "reviewed floor"}]
        result = floor_coverage(snapshot, provenance, {"stairs": []}, profile,
                                review_anchors=anchors)
        self.assertEqual({tuple(cell["at"]) for cell in result["cells"]},
                         {(1, 1), (1, 2), (2, 1), (2, 2)})
        self.assertEqual(len(result["unanchored_candidates"]), 4)
        with self.assertRaisesRegex(ValueError, "no candidate surface"):
            floor_coverage(snapshot, provenance, {"stairs": []}, profile,
                           review_anchors=[{"at": [9, 9], "level": "lower", "evidence": "review"}])

    def test_floor_coverage_rejects_wrong_collision_room(self):
        snapshot = SimpleNamespace(room=1, cells=((), ()))
        provenance = SimpleNamespace(owners=((), ()))
        with self.assertRaisesRegex(ValueError, "attributes do not match"):
            floor_coverage(snapshot, provenance, {"stairs": []}, {"assets": [],
                           "asset_groups": {"wall_tops": {"tile_hashes": []}}},
                           b"ALTPAT1\0" + bytes((2, 0)) + bytes(8192))


    def test_height_range_rejects_entire_connected_group(self):
        s = scene(["floor", "stair", "floor"], [{"layer": 1, "x": 0, "y": 0,
            "region": "surface", "height": 230, "evidence": "reviewed level"}])
        report = solve(s, templates())
        self.assertTrue(any(issue["reason"] == "height_out_of_range" for issue in report["issues"]))
        self.assertTrue(all(item["status"] == "unknown" for item in report["audit"]
                            if item.get("role") in ("floor", "stair")))

    def test_excluded_region_is_explicit_and_never_an_anchor(self):
        rules = {"format": "alttp-surface-rules-v1", "templates": [
            {"id": "decorative", "tile_hash": "empty", "regions": [region("ink", "excluded")]},
            {"id": "floor", "tile_hash": "floor", "regions": [region("surface", "floor")]},
        ]}
        s = scene(["floor"], [{"layer": 1, "x": 0, "y": 0,
            "region": "surface", "height": 0, "evidence": "verified"}])
        report = solve(s, rules)
        self.assertTrue(report["publishable"])
        self.assertEqual(report["region_counts"], {"explicitly_excluded": 1, "resolved": 1})


if __name__ == "__main__":
    unittest.main()
