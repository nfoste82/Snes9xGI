import copy
from types import SimpleNamespace
import unittest
from alttp_authored_obstacles import parallel_section_datums


class MemberDatums(unittest.TestCase):
    def test_transformed_signed_intrinsic_sections_not_group_orientation(self):
        for axis in (0, 1):
            for flip in range(4):
                for sign in (-1, 1):
                    cells = [SimpleNamespace(hflip=bool(flip & 1), vflip=bool(flip & 2)) for _ in range(4096)]
                    i = 10*64+10; j = i+(64 if axis == 0 else 1)
                    records = []
                    for cell, datum in ((i, 12), (j, 0)):
                        local = [sign*(datum+(x if axis == 0 else y)) for y in range(8) for x in range(8)]
                        records.append(dict(layer=1, x=cell%64, y=cell//64, assembly=1, group=2,
                                            tile_hash=str(cell), floor_base=-24, local_height=local, local_source='authored_asset'))
                    before = copy.deepcopy(records)
                    got = parallel_section_datums(cells, records)
                    self.assertEqual(len(got), 1)
                    self.assertEqual(got[0]['effective_first_minus_second'], sign*12)
                    self.assertEqual(records, before)
                    records[1]['floor_base'] += sign*12
                    self.assertEqual(parallel_section_datums(cells, records), [])

    def test_no_seam_claim_for_ambiguous_or_incompatible_sources(self):
        cells = [SimpleNamespace(hflip=False, vflip=False) for _ in range(4096)]
        base = [dict(layer=0, x=10, y=y, assembly=1, group=2, tile_hash=str(y), floor_base=24,
                     local_height=[(12 if y == 10 else 0)+x for _ in range(8) for x in range(8)],
                     local_source='authored_asset') for y in (10, 11)]
        for mode in range(6):
            rows = copy.deepcopy(base)
            if mode == 0: rows[1]['assembly'] = 2
            if mode == 1: rows[1]['group'] = 3
            if mode == 2: rows[1]['local_source'] = 'placement_obstacle_vertical_fallback'
            if mode == 3: rows[1]['local_height'][27] += 1
            if mode == 4: rows[1]['local_height'] = [0]*64
            if mode == 5: rows[1]['local_height'] = [7-x for _ in range(8) for x in range(8)]
            self.assertEqual(parallel_section_datums(cells, rows), [])


if __name__ == '__main__':
    unittest.main()
