import struct
import unittest
from pathlib import Path
from unittest.mock import patch

from alttp_compile_room_height_map import UNKNOWN, compile_map
from alttp_validate_room_heights import load_height_map


class CompileRoomHeightMapTest(unittest.TestCase):
    def test_signed_map_preserves_negative_and_large_bases(self):
        report = {"format": "alttp-room-height-validation-v2", "success": True, "room_id": 7,
                  "placements": [{"x": 1, "y": 2, "plane": 0, "floor_base": -24},
                                 {"x": 2, "y": 2, "plane": 0, "floor_base": 512}]}
        data = compile_map(report)
        self.assertEqual(data[:8], b"ALTPHM3\0")
        self.assertEqual(struct.unpack_from("<i", data, 10+129*4)[0], -24)
        self.assertEqual(struct.unpack_from("<i", data, 10+130*4)[0], 512)
        with patch.object(Path, "read_bytes", return_value=data):
            decoded = load_height_map(Path("signed-review.bin"), 7)
        self.assertEqual(decoded["visible"][129], -24)
        self.assertEqual(decoded["planes"][0][130], 512)
        self.assertEqual(decoded["planes"][1][129], UNKNOWN)

    def test_overlapping_planes_remain_distinct_and_visible_uses_highest(self):
        report = {"format": "alttp-room-height-validation-v2", "success": True, "room_id": 0x60,
                  "placements": [
                      {"x": 3, "y": 4, "plane": 0, "floor_base": 50},
                      {"x": 3, "y": 4, "plane": 1, "floor_base": 0},
                      {"x": 5, "y": 6, "planes": [0], "floor_base": 50}]}
        data = compile_map(report)
        self.assertEqual(data[:8], b"ALTPHM2\0")
        self.assertEqual(struct.unpack_from("<H", data, 8)[0], 0x60)
        index = 4 * 64 + 3
        self.assertEqual(data[10 + index], 50)
        self.assertEqual(data[10 + 4096 + index], 50)
        self.assertEqual(data[10 + 8192 + index], 0)
        self.assertEqual(data[10 + 8192 + 6 * 64 + 5], UNKNOWN)

    def test_rejects_unsuccessful_or_conflicting_report(self):
        with self.assertRaises(ValueError):
            compile_map({"format": "alttp-room-height-validation-v2", "success": False})
        report = {"format": "alttp-room-height-validation-v2", "success": True, "room_id": 1,
                  "placements": [{"x": 1, "y": 1, "plane": 0, "floor_base": 0},
                                 {"x": 1, "y": 1, "plane": 0, "floor_base": 50}]}
        with self.assertRaises(ValueError):
            compile_map(report)


if __name__ == "__main__":
    unittest.main()
