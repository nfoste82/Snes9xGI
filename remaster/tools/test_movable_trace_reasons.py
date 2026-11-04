"""Additive reason bits preserve the historical schema2 indices/names."""
import unittest
from summarize_live_trace import ROOM_REASONS


class MovableReasonsTest(unittest.TestCase):
    def test_additive_names(self):
        self.assertEqual(ROOM_REASONS[3], 'producer')
        self.assertEqual(ROOM_REASONS[10], 'graphics')
        self.assertEqual(ROOM_REASONS[18], 'unpublishable')
        self.assertEqual(ROOM_REASONS[19], 'replacement_evidence')
        self.assertEqual(ROOM_REASONS[20], 'graphics_dependencies')
        self.assertEqual(ROOM_REASONS[21], 'movement_seed')


if __name__ == '__main__':
    unittest.main()
