import struct
import unittest

from alttp_compile_room_height_map import UNKNOWN, compile_map


class CompileRoomHeightMapTest(unittest.TestCase):
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
