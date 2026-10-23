#!/usr/bin/env python3
"""Manifest-only rejection tests; producer integration lives in replay_test.cpp."""
import copy
import hashlib
import unittest

from alttp_overworld_replay import standing_stream


class StandingManifestTest(unittest.TestCase):
    def setUp(self):
        self.data = b'original fixture'
        self.manifest = dict(format='ALTTP-STANDING-FIXTURE1', actor='Link', grounded=True,
                             grounded_source='user', reference='user explicitly says standing',
                             capture_sha256=hashlib.sha256(self.data).hexdigest(),
                             footprint_convention='link-anchor-sole-x8pm3-y23',
                             expected_original_offset=0, origin=[1783, 1587],
                             link_anchor_map=[1905, 1680], link_anchor_screen=[122, 93],
                             bg=1, support=11, local_height=0, base=32, actor_slots=[102, 103],
                             samples=[dict(screen=[x, 116], map=[x+1783, 1703]) for x in range(127, 134)])

    def test_accept(self):
        self.assertTrue(standing_stream(self.manifest, self.data).startswith('OWSTAND1 '))

    def test_airborne_unattested_and_nonzero(self):
        for key, value in [('grounded', False), ('grounded_source', 'bbox'),
                           ('reference', ''), ('expected_original_offset', 32)]:
            m = copy.deepcopy(self.manifest); m[key] = value
            with self.assertRaises(ValueError): standing_stream(m, self.data)

    def test_sha_and_reapply(self):
        for data in (b'other capture', self.data+b'new actor base'):
            with self.assertRaisesRegex(ValueError, 'SHA256'): standing_stream(self.manifest, data)

    def test_coordinate_alignment(self):
        m = copy.deepcopy(self.manifest); m['samples'][0]['map'][1] += 1
        with self.assertRaisesRegex(ValueError, 'alignment'): standing_stream(m, self.data)


if __name__ == '__main__':
    unittest.main()
