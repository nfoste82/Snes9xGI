import unittest
from alttp_wall_torch_evidence import identity, match_footprint


class WallTorchEvidenceTests(unittest.TestCase):
    def test_canonical_core_hash(self):
        vram = bytearray(65536)
        for y in range(8):
            for x in range(8):
                for b in range(4):
                    vram[32+b//2*16+y*2+b%2] |= (((y*8+x)&15)>>b & 1) << (7-x)
        self.assertEqual(identity(vram, 0, 1), 'v1:4bpp:f99d5746200467e1')

    def test_transforms_uniform_palette_and_priority(self):
        words = [0x4800+k for k in range(8)]
        for flip in range(4):
            maps = [0]*4096
            for x in range(2):
                for y in range(4):
                    sx = 1-x if flip&1 else x
                    sy = 3-y if flip&2 else y
                    maps[(20+y)*64+10+x] = (words[sx*4+sy] ^ (0x4000 if flip&1 else 0) ^
                                            (0x8000 if flip&2 else 0)) | 0x2400
            self.assertIsNotNone(match_footprint(maps, words, 10, 20, 2, 4, flip))
            maps[20*64+10] ^= 0x2000
            self.assertIsNotNone(match_footprint(maps, words, 10, 20, 2, 4, flip))
            maps[20*64+10] ^= 1
            self.assertIsNone(match_footprint(maps, words, 10, 20, 2, 4, flip))

    def test_partial_overwrite_palette_and_boundaries_reject(self):
        words = [0x4800+k for k in range(8)]
        maps = [0]*4096
        for x in range(2):
            for y in range(4):
                maps[(20+y)*64+10+x] = words[x*4+y]
        maps[20*64+10] ^= 0x400
        self.assertIsNone(match_footprint(maps, words, 10, 20, 2, 4, 0))
        self.assertIsNone(match_footprint(maps, words, 63, 20, 2, 4, 0))
        self.assertIsNone(match_footprint(maps, words, 10, 61, 2, 4, 0))

    def test_north_handler_is_row_major(self):
        words = [0x4800+k for k in range(8)]
        maps = [0]*4096
        for y in range(2):
            for x in range(4):
                maps[(20+y)*64+10+x] = words[y*4+x]
        self.assertIsNotNone(match_footprint(maps, words, 10, 20, 4, 2, 0, row_major=True))
        self.assertIsNone(match_footprint(maps, words, 10, 20, 4, 2, 0))

    def test_symmetric_claims_do_not_supply_attachment_datum(self):
        words = [0x4800,0x4801,0xc801,0xc800]*2
        maps = [0]*4096
        for x in range(2):
            for y in range(4):
                maps[(20+y)*64+10+x] = words[x*4+y]
        self.assertIsNotNone(match_footprint(maps, words, 10, 20, 2, 4, 0))
        self.assertIsNotNone(match_footprint(maps, words, 10, 20, 2, 4, 2))
        # Matching two transforms yields ownership candidates, no base field.
        self.assertEqual(len(match_footprint(maps, words, 10, 20, 2, 4, 0)), 2)


if __name__ == '__main__':
    unittest.main()
