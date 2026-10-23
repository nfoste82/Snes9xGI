import struct
import unittest
import json
from pathlib import Path
import tomllib
from types import SimpleNamespace

from alttp_diagonal_sets import (band_for, eligibility, inspect, proposal_patch,
                                 transfer, transformed, verify_low_cardinal_geometry)


class DiagonalSetsTest(unittest.TestCase):
    def test_low_authored_bands_and_full_wall_direction(self):
        profile = Path(__file__).resolve().parents[3]/'remaster/profiles/ALttP/alttp-profile.toml'
        assets = {a['tile_hash']:a for a in tomllib.loads(profile.read_text())['assets']}
        groups = [['50d27476d4f95ff0','042b1f6fe3b7a372'],
                  ['32a08f2b82a3bbe6','03e6150673f678eb'],
                  ['c6c8bc5f9a10e349','21a32992dd5a1432'],
                  ['4952263cea5a838f','41ad2b5f209bec6a','a07774b037c235c6','ecec2e21065071c6']]
        refs=['a4aa6ebd24fefeb7','503ab068dfa941d3','3bc2b767d36c52c3','31aba23efb31666b']
        for band,keys in enumerate(groups):
            for key in keys:
                target=assets['v1:4bpp:'+key]
                for field,value in transfer(assets['v1:4bpp:'+refs[band]]).items():
                    self.assertEqual(target[field],value)
                self.assertNotIn('occlusion',target)
                self.assertEqual(target['height'],[max(0,min(24,31-8*band-x-y)) for y in range(8) for x in range(8)])
        # Canonical SE becomes NE on the southwest V-flipped assembly.
        self.assertEqual(transformed(assets['v1:4bpp:c6c8bc5f9a10e349']['normal_xyz'],3,
                                     vflip=True,normal=True),[220,41,128]*64)
        self.assertEqual(assets['v1:4bpp:1a032ad7756c49db']['tile_role'],'floor')

    def test_low_requires_independent_cardinal_seams(self):
        templates=bytearray(12876)
        struct.pack_into('<3H',templates,0x330,1,2,3)
        struct.pack_into('<3H',templates,0x308,4,5,6)
        snapshot=SimpleNamespace(layers=[(0,0),(0,0)],_tile_hash=lambda addr:str(addr//16))
        assets={str(1+axis*3+band):{'height':[8*(2-band)+7-(x if axis else y)
                for y in range(8) for x in range(8)]} for axis in range(2) for band in range(3)}
        self.assertEqual(len(verify_low_cardinal_geometry(snapshot,templates,assets)),6)
        assets['2']['height']=[7]*64
        with self.assertRaises(ValueError):verify_low_cardinal_geometry(snapshot,templates,assets)

    def test_low_vflipped_dispatch_overwrite_rejection(self):
        templates=bytearray(12876);words=[0x8004,0x8003,0x8002,0x8001,0x8000]
        struct.pack_into('<5H',templates,0x4a2,*words)
        def cell(layer,x,y):
            row=y-(20+x-10)
            return SimpleNamespace(word=words[row],tile_hash=str(row),hflip=False,vflip=True)
        prov=SimpleNamespace(objects=[dict(phase=1,ordinal=0,is_door=0,raw=(20<<10)|(10<<2))],
            owner=lambda layer,x,y:2 if (x,y)==(10,21) else 1)
        records,claims=inspect(SimpleNamespace(cell=cell),prov,{(1,0):dict(subtype=0,object_id=22)},templates)
        self.assertEqual(records[0]['family'],'low')
        self.assertEqual(len(records[0]['placements']),58)
        self.assertEqual(claims[1,10,24],{('4',False,0)})
        self.assertNotIn((1,10,21),claims)

    def test_authored_profile_matches_exact_approved_arrays(self):
        profile = Path(__file__).resolve().parents[3]/'remaster/profiles/ALttP/alttp-profile.toml'
        assets = {a['tile_hash']:a for a in tomllib.loads(profile.read_text())['assets']}
        groups = {
            'a4aa6ebd24fefeb7':['0221286379a51537','14c17494b614916c','47b53287b9de4dad','763d8c266d053224','e45e17af66ecc627'],
            '503ab068dfa941d3':['114a7c432323276e','2981d7a1438cb7f9','3d3ebf8894607de1','4c00243dfbb980a5','5d3bc6907dece26c'],
            '3bc2b767d36c52c3':['21a81de1db2e6a51','39b78d16de3f7acb','43ecfb30d33274db','8b839534fd40a9d3','ed71fcb09d321d30'],
            '31aba23efb31666b':['4dc1cd16e8b5967e','e48afc9dc194123c','e75cf1bf1f0c6b12','ea5078ff7bc62f34','f7e9ae78507ae068']}
        for ref,keys in groups.items():
            reference = assets['v1:4bpp:'+ref]
            for key in keys:
                target = assets['v1:4bpp:'+key]
                for field,value in transfer(reference).items():
                    with self.subTest(key=key,field=field):
                        self.assertEqual(target[field],value,(key,field,len(target[field]),len(value)))

    def test_flip_normals_and_pixels(self):
        normals = sum(([220,215,128] for _ in range(64)),[])
        self.assertEqual(transformed(normals,3,True,True,True),[36,41,128]*64)
        ramp = list(range(64))
        self.assertEqual(transformed(ramp,hflip=True)[0:8],list(range(7,-1,-1)))
        self.assertEqual(transformed(ramp,vflip=True)[0:8],list(range(56,64)))
        self.assertEqual([band_for(r,0x8000) for r in range(5)],[4,3,2,1,0])

    def test_transfer_default_occlusion_is_not_artwork_hole(self):
        ref = {'height':list(range(64)), 'normal_xyz':[128,128,255]*64}
        result = transfer(ref)
        self.assertNotIn('occlusion',result)
        ref['occlusion'] = list(range(64))
        self.assertEqual(transfer(ref)['occlusion'],ref['occlusion'])

    def test_reject_conflicts_and_preserve_masks(self):
        sem = {'id':{'role':'wall','normals':['south_east']}}
        self.assertEqual(eligibility('id',{1},{},sem,(),set()),[])
        self.assertIn('conflicting_assembly_bands',eligibility('id',{1,2},{},sem,(),set()))
        self.assertIn('also_low_diagonal_topology',eligibility('id',{1},{},sem,(),{'id'}))
        for role in ('floor','door','obstacle','stair_top','stair_bottom','wall_corner'):
            self.assertIn('explicit_non_equivalent_role',eligibility('id',{1},{'id':{'tile_role':role}},sem,(),set()))
        self.assertIn('target_occlusion_requires_review',eligibility('id',{1},{'id':{'occlusion':[0]*64}},sem,(),set()))
        self.assertIn('context_dependent',eligibility('id',{1},{}, {'id':{**sem['id'],'context_dependent':True}},(),set()))

    def test_dispatch_overwrites_both_bgs_and_reverse_bands(self):
        templates = bytearray(12876)
        words = [0x8004,0x8003,0x8002,0x8001,0x8000]
        struct.pack_into('<5H',templates,0x420,*words)
        raw = (20<<10)|(10<<2)
        obj = dict(subtype=0,object_id=9)
        def cell(layer,x,y):
            row = y-(20-(x-10))
            word = words[row] if 10<=x<16 and 0<=row<5 else 0
            return SimpleNamespace(word=word,tile_hash=str(word),hflip=False,vflip=True)
        def owner(layer,x,y):
            return 2 if (x,y)==(10,21) else 1
        snapshot = SimpleNamespace(cell=cell)
        prov = SimpleNamespace(objects=[dict(phase=1,ordinal=0,is_door=0,raw=raw)],owner=owner)
        records, claims = inspect(snapshot,prov,{(1,0):obj},templates)
        self.assertEqual(len(records[0]['placements']),58)
        self.assertNotIn((1,10,21),claims)
        self.assertEqual(records[0]['placements'][0]['band'],'trim')
        self.assertEqual(records[0]['omitted']['not_final_writer_or_equal_write'],2)
        self.assertEqual({p['layer'] for p in records[0]['placements']},{0,1})

    def test_stair_priority_only_flank_retains_band_not_opening_art(self):
        templates=bytearray(12876)
        words=[100,101,102,103,104]
        struct.pack_into('<5H',templates,0x42a,*words)
        raw=(10<<10)|(10<<2)
        overwritten = {}
        def cell(layer,x,y):
            row=y-(10+x-10)
            word=words[row] if 10<=x<16 and 0<=row<5 else 999
            word=overwritten.get((layer,x,y),word)
            return SimpleNamespace(word=word,tile_hash=str(word&0x1fff),hflip=False,vflip=False)
        def owner(layer,x,y):
            return 2 if (layer,x,y) in overwritten else 1
        s=SimpleNamespace(cell=cell)
        prov=SimpleNamespace(objects=[dict(phase=1,ordinal=0,is_door=0,raw=raw),
            dict(phase=1,ordinal=1,is_door=0,raw=0)],owner=owner)
        objects={(1,0):dict(subtype=0,object_id=10),
                 (1,1):dict(subtype=2,object_id=0x39,x=6,y=10)}
        overwritten[1,10,10]=100|0x2000
        records,claims=inspect(s,prov,objects,templates)
        self.assertEqual(claims[1,10,10],{('100',True,0)})
        self.assertEqual(records[0]['placements'][-1]['ownership_operation'],'verified_stair_flank_priority_or')
        # Two plausible earlier dispatches cannot donate ownership, even if
        # they propose the same intrinsic band and artwork.
        ambiguous=SimpleNamespace(objects=[prov.objects[0],
            dict(phase=1,ordinal=2,is_door=0,raw=raw),prov.objects[1]],
            owner=lambda layer,x,y:3 if (layer,x,y) in overwritten else
                (1 if y==11 else 2 if y in (12,13) else 1))
        ambiguous_objects={**objects,(1,2):objects[1,0]}
        self.assertNotIn((1,10,10),inspect(s,ambiguous,ambiguous_objects,templates)[1])
        # Actual artwork replacement, interior placement, wrong handler and
        # an overwritten companion must not be accepted as priority-only.
        overwritten[1,10,10]=101|0x2000
        self.assertNotIn((1,10,10),inspect(s,prov,objects,templates)[1])
        overwritten[1,10,10]=100|0x2000
        objects[1,1]['x']=7
        self.assertNotIn((1,10,10),inspect(s,prov,objects,templates)[1])
        objects[1,1]['x']=6;objects[1,1]['object_id']=0x37
        self.assertNotIn((1,10,10),inspect(s,prov,objects,templates)[1])
        objects[1,1]['object_id']=0x39;overwritten[1,10,11]=101
        self.assertNotIn((1,10,10),inspect(s,prov,objects,templates)[1])

    def test_patch_changes_only_authorized_lines_and_new_assets(self):
        text = 'schema_version = 21\n[[assets]]\ntile_hash = "old"\nheight = [0]\nnote = "keep"\n\n[[rules]]\nid = "keep"\n'
        sections = proposal_patch(text,{'old':{'height':[1]},'new':{'height':[2]}})
        self.assertEqual(len(sections),2)
        self.assertIn('+note = "keep"',sections[0])
        self.assertIn('+tile_hash = "new"',sections[1])
        self.assertIn(' [[rules]]',sections[1])


if __name__ == '__main__':
    unittest.main()
