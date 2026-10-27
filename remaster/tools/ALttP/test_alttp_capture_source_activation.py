import unittest
from alttp_capture_source_activation import audit,classify

class SourceActivationTest(unittest.TestCase):
 def test_authored_not_activated(self):
  self.assertEqual(classify(dict(height=[3]*64,normal_xyz=[128]*192),dict(has_height=0,has_normals=0)),['authored_height_not_selected','authored_normal_xyz_not_selected'])
 def test_selected_alternate_not_false_missing(self):
  self.assertEqual(classify({},dict(has_height=1,has_normals=1)),[])
 def test_absent_authoring_separate(self):
  self.assertEqual(classify({},dict(has_height=0,has_normals=1)),['missing_asset_height_and_selected_source'])
 def test_partial_authoring_not_complete(self):
  self.assertEqual(classify(dict(height=[0]*63),dict(has_height=0,has_normals=1)),['missing_asset_height_and_selected_source'])
 def test_buffers_retained_pixels_deduplicated_transparent_excluded(self):
  s=dict(id=1,source=2,bg=1,ink=2,hash='123',k=63,has_height=0,has_normals=1)
  p=dict(sx=0,sy=0,main=s,sub=s,scene=dict(s,ink=0),field_known=0,participation=1,use_scene=1)
  r=audit(dict(assets=[dict(tile_hash='v1:4bpp:123',height=[0]*64)]),[p],[511,511])
  self.assertEqual(r['observed_placements'],1);g=r['groups'][0]
  self.assertEqual((g['x'],g['y']),(63,63));self.assertEqual(g['distinct_canonical_pixels'],1)
  self.assertEqual(len(g['samples']),2);self.assertEqual(r['issue_placement_counts']['authored_height_not_selected'],1)
 def test_obj_not_bg(self):
  self.assertEqual(audit({},[dict(sx=0,sy=0,use_scene=0,main=dict(id=1,source=3,bg=1,ink=2))],[0,0])['observed_placements'],0)
 def test_scene_admission_role_and_normals_separate(self):
  s=dict(id=1,source=2,bg=1,ink=2,hash='123',k=0,has_height=1,has_normals=1,role=0)
  p=dict(sx=0,sy=0,main={},scene={},sub=s,field_known=0,participation=255,use_scene=1)
  r=audit({},[p],[0,0]);self.assertEqual(r['scene_counts'],{'scene_restore_role_not_admitted':1})
  p['sub']=dict(s,has_normals=0)
  self.assertEqual(audit({},[p],[0,0])['scene_counts'],{'scene_restore_requires_normals':1})
