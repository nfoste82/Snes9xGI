"""Regression checks for the opt-in Link geometry proposal (no ROM required)."""
import copy
import json
import unittest

import numpy as np

from alttp_atlas import SUPPORTED_US_SHA256
from alttp_link_geometry import analytic, encode, groups, placeholder, propose, unit


class LinkGeometryTests(unittest.TestCase):
    def part(self, tile_hash='v1:4bpp:1111111111111111', address=16384, vflip=False):
        return dict(tile_hash=tile_hash, address=address, slot=1, x=0, y=0,
                    hflip=False, vflip=vflip, indices=[11]*64, colors=[0]*64)

    def pose(self, parts):
        return dict(parts=parts, facing='north', action='walking', step=0)

    def test_placeholder_is_entire_tile_only(self):
        for n in ([128, 255, 128], [255, 128, 128]):
            self.assertTrue(placeholder({'normal_xyz': n*64}))
        n = [128, 255, 128]*64
        n[0:3] = [128, 128, 255]
        self.assertFalse(placeholder({'normal_xyz': n}))
        self.assertFalse(placeholder({'normal_xyz': [128, 128, 255]*64}))

    def test_normal_encoding(self):
        self.assertEqual(encode([0, 0, 1]), [128, 128, 255])
        self.assertEqual(encode([0, 1, 0]), [128, 255, 128])

    def test_head_crown_points_up(self):
        g = groups(self.pose([self.part()]), {})[0]
        n, h = analytic(g, 3, 0, g['pixels'][3, 0])
        self.assertGreater(n[2], 0.9)
        self.assertGreater(h, 10)
        n, _ = analytic(g, 3, 7, g['pixels'][3, 7])
        self.assertGreater(n[1], 0.7)

    def test_flips_move_pixels_and_vectors_independently(self):
        p = self.part(vflip=True)
        asset = {'normal_xyz': [128, 255, 128]*64}
        # Use a slightly nonuniform map so it is not a placeholder.
        asset['normal_xyz'][0:3] = [128, 220, 215]
        g = groups(self.pose([p]), {p['tile_hash']: asset})[0]
        self.assertEqual(g['pixels'][0, 7]['pixel'], 0)
        self.assertLess(g['pixels'][0, 7]['normal'][1], 0)

    def test_proposal_preserves_authored_and_is_idempotent(self):
        p, q = self.part(), self.part('v1:4bpp:2222222222222222', 16464, True)
        q['slot'] = 2
        manifest = dict(actors=[dict(name='Link', id='fixture', poses=[self.pose([p, q])])],
                        background_shared_hashes=[])
        text = f'''schema_version = 29
[game]
rom_sha256 = "{SUPPORTED_US_SHA256}"
[[assets]]
tile_hash = "{p['tile_hash']}"
height = {json.dumps([7]*64)}
normal_xyz = {json.dumps([128, 128, 255]*64)}
'''
        original = copy.deepcopy(manifest)
        candidate, report, _, assets = propose(manifest, text)
        self.assertEqual(assets[p['tile_hash']]['height'], [7]*64)
        self.assertEqual(assets[p['tile_hash']]['normal_xyz'], [128, 128, 255]*64)
        self.assertEqual(report['added_heights'], 1)
        self.assertTrue(assets[q['tile_hash']]['geometry_by_flip'])
        n = assets[q['tile_hash']]['flip_vert_normal_xyz']
        self.assertGreater(n[2], 128)  # up remains up for vertically flipped blade
        self.assertEqual(manifest, original)
        second, report2, _, _ = propose(manifest, candidate)
        self.assertEqual(second, candidate)
        self.assertEqual(report2['added_heights'], 0)
        self.assertEqual(report2['added_normals'], 0)


if __name__ == '__main__':
    unittest.main()
