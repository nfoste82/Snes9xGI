import copy
from pathlib import Path
import tomllib
import unittest
from alttp_torch_animation_metadata import families, reviewed_changes, invariants, SEEDS

ROOT=Path(__file__).resolve().parents[3]
ATLAS=ROOT/'build/remaster-geometry/alttp-atlas.sqlite'
PROFILE=ROOT/'remaster/profiles/ALttP/alttp-profile.toml'

class TorchAnimationTests(unittest.TestCase):
    def test_preserves_emission_normals_and_unrelated_metadata(self):
        a={'assets':[{'tile_hash':'x','height':[0]*64,'emission_rgba':[1]*256,
                     'emission_depth':2,'emission_omnidirectional':True,'combine_emitters':['y'],
                     'normal_xyz':[128]*192,'occlusion':[255]*64}]}
        b=copy.deepcopy(a);b['assets'][0]['height']=[7]*64;invariants(a,b)
        for field in ('emission_rgba','emission_depth','emission_omnidirectional','combine_emitters','normal_xyz','occlusion'):
            c=copy.deepcopy(b);c['assets'][0][field]=None
            with self.assertRaises(AssertionError):invariants(a,c)

    def test_new_members_are_geometry_only(self):
        a={'assets':[]};b={'assets':[{'tile_hash':'x','height':[7]*64,'tile_role':'wall'}]};invariants(a,b)
        b['assets'][0]['emission_rgba']=[0]*256
        with self.assertRaises(AssertionError):invariants(a,b)

    @unittest.skipUnless(ATLAS.exists(),'local ROM-bound atlas required')
    def test_complete_three_phase_positional_metadata(self):
        p=tomllib.loads(PROFILE.read_text());assets={a['tile_hash']:a for a in p['assets']}
        self.assertEqual(reviewed_changes(p,ATLAS),{})
        groups=families(ATLAS);self.assertEqual(len(groups),6)
        self.assertEqual(len({i for ids in groups.values() for i in ids}),18)
        for slot,ids in groups.items():
            seed=assets[SEEDS[slot]]
            for tile in ids:
                self.assertEqual(assets[tile]['height'],seed['height'])
                self.assertEqual(assets[tile]['height_sampling'],seed['height_sampling'])
                self.assertEqual(assets[tile]['tile_role'],'wall')

if __name__=='__main__':unittest.main()
