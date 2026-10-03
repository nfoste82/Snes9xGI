#!/usr/bin/env python3
"""Focused checks for semantic ALTTP profile geometry."""

import unittest
import tomllib

from alttp_atlas_profile import _complete_semantic_groups, _semantic_wall_layers, _render_profile


class SemanticProfileTests(unittest.TestCase):
    def test_obstacle_replaces_legacy_group_and_preserves_relief(self) -> None:
        source = ('[asset_groups.dungeon_floor]\ntile_hashes = ["floor"]\n\n'
                  '[asset_groups.wall_faces]\ntile_hashes = ["rail", "wall"]\n\n'
                  '[[assets]]\ntile_hash = "rail"\nheight = [3, 7]\n')
        profile = tomllib.loads(source)
        rendered, _ = _complete_semantic_groups(source, profile, {
            "rail": {"role": "obstacle", "context_dependent": False}})
        result = tomllib.loads(rendered)
        self.assertEqual(result["asset_groups"]["obstacles"]["tile_hashes"], ["rail"])
        self.assertEqual(result["asset_groups"]["wall_faces"]["tile_hashes"], ["wall"])
        self.assertEqual(result["assets"][0]["height"], [3, 7])

    def test_explicit_wall_annotation_replaces_absolute_authored_geometry(self) -> None:
        content_id = "v1:4bpp:0123456789abcdef"
        source = f'[[assets]]\ntile_hash = "{content_id}"\nheight = [24]\n'
        authored = {content_id: {"tile_hash": content_id, "height": [24]}}
        local = _semantic_wall_layers({"role": "wall", "normals": ["east"]})
        rendered = _render_profile(source, authored, {content_id: local}, {content_id})
        self.assertEqual(tomllib.loads(rendered)["assets"][0]["height"], list(local["height"]))

    def test_cardinal_wall_descends_toward_its_normal(self) -> None:
        east = _semantic_wall_layers({"role": "wall", "normals": ["east"]})["height"]
        south = _semantic_wall_layers({"role": "wall", "normals": ["south"]})["height"]
        self.assertEqual(east[:8], tuple(range(7, -1, -1)))
        self.assertEqual(south[::8], tuple(range(7, -1, -1)))

    def test_authored_roles_replace_stale_labels_without_merging_corners(self) -> None:
        content_id = "v1:4bpp:0123456789abcdef"
        source = f'[[assets]]\ntile_hash = "{content_id}"\ntile_role = "wall"\nheight = [24]\n'
        authored = {content_id: {"tile_hash": content_id, "height": [24], "tile_role": "wall"}}
        rendered = _render_profile(source, authored, {}, tile_roles={content_id: "wall_corner"})
        self.assertEqual(tomllib.loads(rendered)["assets"][0]["tile_role"], "wall_corner")
        rendered = _render_profile(source, authored, {}, tile_roles={})
        self.assertNotIn("tile_role", tomllib.loads(rendered)["assets"][0])

    def test_corner_topology_combines_directional_ramps(self) -> None:
        outside = _semantic_wall_layers({"role": "wall_corner", "normals": ["east", "south"],
                                         "corner_type": "outside"})["height"]
        inside = _semantic_wall_layers({"role": "wall_corner", "normals": ["east", "south"],
                                        "corner_type": "inside"})["height"]
        self.assertEqual(outside[0], 7)
        self.assertEqual(outside[7], 0)
        self.assertEqual(outside[56], 0)
        self.assertEqual(inside[7], 7)
        self.assertEqual(inside[56], 7)
        self.assertEqual(inside[63], 0)

    def test_semantics_extend_existing_material_groups(self) -> None:
        source = ('[asset_groups.dungeon_floor]\ntile_hashes = ["old-floor"]\nmaterial = "floor"\n\n'
                  '[asset_groups.wall_faces]\ntile_hashes = ["old-wall"]\nmaterial = "wall"\n')
        profile = {"asset_groups": {"dungeon_floor": {"tile_hashes": ["old-floor"]},
                                    "wall_faces": {"tile_hashes": ["old-wall"]}}}
        semantics = {"new-floor": {"role": "floor", "context_dependent": False},
                     "new-wall": {"role": "wall", "context_dependent": False},
                     "unknown-corner": {"role": "wall_corner", "context_dependent": False,
                                        "corner_type": "unknown"}}
        rendered, additions = _complete_semantic_groups(source, profile, semantics)
        self.assertIn('tile_hashes = ["new-floor", "old-floor"]', rendered)
        self.assertIn('tile_hashes = ["new-wall", "old-wall"]', rendered)
        self.assertNotIn("unknown-corner", rendered)
        self.assertEqual(additions, {"dungeon_floor": 1, "wall_faces": 1})


if __name__ == "__main__":
    unittest.main()
