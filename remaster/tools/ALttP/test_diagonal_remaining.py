import unittest
from alttp_diagonal_remaining_rules import remaining_proposals
from alttp_diagonal_sets import REFERENCES,ROLES
from alttp_validate_room_heights import grounded_floor_hashes

class RemainingRulesTest(unittest.TestCase):
 def test_grounded_datum_explicit_floor_role_and_stale_group_veto(self):
  profile=dict(asset_groups={'dungeon_floor':dict(tile_hashes=['group','veto'])},assets=[
      dict(tile_hash='explicit',tile_role='floor'),dict(tile_hash='veto',tile_role='wall'),
      dict(tile_hash='unknown',height=[0]*64)])
  self.assertEqual(grounded_floor_hashes(profile),{'group','explicit'})
 def test_trim_all_uses_owned_and_unmodified(self):
  entry=dict(tile_hash='trim',gap=True,templates=[dict(band='trim')],
    placements=[dict(room=1,entrance=-1,layer=1,x=2,y=3,word=5)],alternatives=[])
  placement=dict(tile_hash='trim',band='trim',layer=1,x=2,y=3,word=5)
  discovery=dict(rooms=[dict(room=1,requested_entrance=-1,status='scanned',renderer='entrance=4')],
    assemblies=[dict(room=1,entrance=4,placements=[placement])],loaded_templates=[])
  semantics={'trim':dict(role='wall',normals=['south_east'])}
  updates,_=remaining_proposals([entry],discovery,{}, semantics)
  self.assertEqual(updates['trim'],dict(height=[0]*64,normal_xyz=[128,128,255]*64))
  placement['word']=6
  self.assertEqual(remaining_proposals([entry],discovery,{}, {})[0],{})
  placement['word']=5;placement['band']='foot'
  self.assertEqual(remaining_proposals([entry],discovery,{}, {})[0],{})
  placement['band']='trim'
  self.assertEqual(remaining_proposals([entry],discovery,{'trim':dict(height=[1]*64)}, {})[0],{})

 def test_loaded_foot_intrinsic_not_placement_and_conflict_veto(self):
  assets={key:dict(height=[max(0,min(24,31-8*b-x-y)) for y in range(8) for x in range(8)],normal_xyz=[220,215,128]*64)
          for b,key in enumerate(REFERENCES)}
  entry=dict(tile_hash='foot',gap=True,templates=[dict(band='foot',family='low')],placements=[],alternatives=[])
  bands=[dict(tile_hash=REFERENCES[i] if i<3 else 'foot',band=role) for i,role in enumerate(ROLES[:4])]
  discovery=dict(rooms=[],assemblies=[],low_cardinal_evidence={'accepted':True},loaded_templates=[dict(room=2,handler=21,bands=bands)])
  semantics={'foot':dict(role='wall',normals=['south_east'])}
  updates,proofs=remaining_proposals([entry],discovery,assets,semantics)
  self.assertEqual(updates['foot']['height'],assets[REFERENCES[3]]['height'])
  self.assertEqual(proofs['foot']['observed_placements'],0)
  entry['templates'].append(dict(band='middle',family='low'))
  self.assertEqual(remaining_proposals([entry],discovery,assets,semantics)[0],{})
  entry['templates'].pop();assets[REFERENCES[1]]['height']=[9]*64
  # Reference + companion equality is independently pinned by discovery's main
  # entrypoint; reject a different companion here.
  bands[1]['tile_hash']='unapproved'
  self.assertEqual(remaining_proposals([entry],discovery,assets,semantics)[0],{})

if __name__=='__main__':unittest.main()
