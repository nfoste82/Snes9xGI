import unittest
from alttp_door_extension_detection import expected_band
class ExtensionTest(unittest.TestCase):
 def test_south(self):self.assertEqual(expected_band([y for y in range(8) for x in range(8)],3,0,0,False),[24+y for y in range(8) for x in range(8)])
 def test_north(self):self.assertEqual(expected_band([7-y for y in range(8) for x in range(8)],4,12,0,True)[0],51)
 def test_horizontal_flip(self):self.assertEqual(expected_band([7-x for y in range(8) for x in range(8)],3,0,1,True)[0],31)
 def test_cutout_and_multiband_unsupported(self):
  self.assertIsNone(expected_band([0]*64,3,0,0,False));self.assertIsNone(expected_band(list(range(64)),3,0,0,False))
