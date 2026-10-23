import unittest
from alttp_author_cardinal_missing import proposal

class Missing(unittest.TestCase):
 def test_missing_only(self):
  old={'tile_hash':'old','height':[83]*64,'normal_xyz':[12]*192,'occlusion':[77]*64}
  profile={'assets':[old]};sem={'tiles':{'east':{'role':'wall','normals':['east']},'south':{'role':'wall','normals':['south']},'old':{'role':'wall','normals':['east']},'corner':{'role':'wall_corner','normals':['east','south']},'diagonal':{'role':'wall','normals':['south_east']},'obstacle':{'role':'obstacle','normals':['east']},'context':{'role':'wall','normals':['east'],'context_dependent':True}}}
  rows,assets=proposal(profile,sem);a={v['tile_hash']:v for v in assets}
  self.assertEqual(set(a),{'east','south'});self.assertEqual(a['east']['height'],[7-x for y in range(8) for x in range(8)])
  self.assertEqual(a['south']['height'],[7-y for y in range(8) for x in range(8)])
  self.assertEqual(old['height'],[83]*64);self.assertEqual(len(rows),5)
  self.assertEqual(sum(r['status']=='unresolved_multiple_facings_or_context' for r in rows),2)
if __name__=='__main__':unittest.main()
