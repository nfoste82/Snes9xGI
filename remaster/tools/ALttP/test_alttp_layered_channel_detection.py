import unittest
from alttp_layered_channel_detection import classify,detect

def row():
 return dict(stage='final',x=8,y=16,bg1_ink=2,bg2_ink=0,sub=dict(bg=1,id=1,k=0,hash='a',has_height=1,has_normals=1,has_occlusion=1,local=0,actual=24,base=24),scene=dict(id=0))

class Layered(unittest.TestCase):
 def test_empty_transparent_retains_all_available_channels(self):
  self.assertEqual(classify(row())['lost'],['height','normals','occlusion'])
 def test_authored_zero_is_present(self):
  r=row();r['sub']['actual']=0;r['sub']['base']=0
  self.assertIn('height',classify(r)['lost'])
 def test_missing_normals_does_not_hide_relief(self):
  r=row();r['sub']['has_normals']=0
  self.assertEqual(classify(r)['lost'],['height','occlusion']);self.assertEqual(classify(r)['donor_absent'],['normals'])
 def test_opaque_bg2_missing_channels_not_transparency(self):
  r=row();r['bg2_ink']=4;r['scene']=dict(bg=2,id=2,has_height=0)
  self.assertEqual(classify(r)['lost'],[]);self.assertEqual(classify(r)['rule'],'opaque_bg2_owner_do_not_borrow_bg1')
 def test_transparent_bg2_selected_is_clobber(self):
  r=row();r['scene']=dict(bg=2,id=2);self.assertEqual(classify(r)['rule'],'transparent_bg2_selected_over_opaque_bg1')
 def test_retained_same_pixel_not_missing(self):
  r=row();r['scene']=r['sub'].copy();self.assertEqual(classify(r)['lost'],[])
 def test_partial_and_flip_exact_pixel(self):
  r=row();r['scene']=r['sub'].copy();r['scene']['k']=63
  self.assertNotEqual(classify(r)['rule'],'bg1_donor_retained')
 def test_other_support_owner_no_borrow(self):
  r=row();r['scene']=dict(bg=1,id=3,base=72);self.assertEqual(classify(r)['lost'],[])
 def test_transparent_donor_not_counted(self):
  r=row();r['bg1_ink']=0;self.assertIsNone(classify(r))
 def test_dedup_only_final_physical_pixels(self):
  r=row();s=row();s['stage']='post_profile';o=detect([r,s]);self.assertEqual(o['counts']['screen_pixels'],1);self.assertEqual(o['groups'][0]['pixels'],1)
if __name__=='__main__':unittest.main()
