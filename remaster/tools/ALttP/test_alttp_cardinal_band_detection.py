import unittest
from alttp_cardinal_band_detection import join
class BandTest(unittest.TestCase):
 def face(self,base,axis=0,rev=0):return dict(base=base,has_effective_height=1,actual=[base+(x if axis else y)*(1 if rev else -1)+7 for y in range(8) for x in range(8)])
 def test_missing_band_detected(self):self.assertTrue(all(p['residual']==8 for p in join(self.face(0),self.face(0),0,0)))
 def test_continuous_negative(self):self.assertTrue(all(p['residual']==0 for p in join(self.face(8),self.face(0),0,0)))
 def test_horizontal_reverse(self):self.assertTrue(all(p['residual']==0 for p in join(self.face(0,1,1),self.face(8,1,1),1,1)))
 def test_missing_source(self):
  b=self.face(0);b['has_effective_height']=0;self.assertIsNone(join(self.face(8),b,0,0))
 def test_cutout_unsupported(self):
  b=self.face(0);b['actual'][30]=0;self.assertIsNone(join(self.face(8),b,0,0))
