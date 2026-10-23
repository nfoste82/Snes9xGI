"""Normals-only biped authoring and assembled-surface regressions."""
import json
import tomllib
import unittest

import numpy as np

from alttp_biped_normals import generate, limb_normal, lighting_cue, model_pixel, rounded, segment, unauthored
from test_humanoid_geometry import HumanoidGeometryTests


class BipedNormalsTests(unittest.TestCase):
    def fixture(self,fields=None,key='running'):
        helper=HumanoidGeometryTests()
        part=helper.part()
        actor=helper.actor(part,key)
        return helper.manifest([actor]),helper.text(part,fields),actor,part

    def test_only_y_fill_is_unauthored(self):
        self.assertTrue(unauthored(dict(normal_xyz=[128,255,128]*64)))
        for n in ([255,128,128]*64,[128,128,255]*64,[128,254,128]*64):
            self.assertFalse(unauthored(dict(normal_xyz=n)))
            m,t,_,p=self.fixture(dict(normal_xyz=n,height=[7]*64))
            candidate,report,assets,_=generate(m,t)
            self.assertEqual(candidate,t)
            self.assertEqual(report['changed_tiles'],0)

    def test_y_replaced_preserving_all_other_fields(self):
        fields=dict(normal_xyz=[128,255,128]*64,height=[4]*64,height_sampling='nearest',normal_ignore_tile_transform=True)
        m,t,_,p=self.fixture(fields)
        candidate,report,assets,_=generate(m,t)
        self.assertEqual(report['replaced_y'],1)
        for k,v in fields.items():
            if k!='normal_xyz':self.assertEqual(assets[p['tile_hash']][k],v)
        again,report,_,_=generate(m,candidate)
        self.assertEqual(again,candidate)
        self.assertEqual(report['changed_tiles'],0)

    def test_no_heights_added_with_missing_normal(self):
        m,t,_,p=self.fixture()
        _,report,assets,_=generate(m,t)
        self.assertEqual(report['added_normals'],1)
        self.assertNotIn('height',assets[p['tile_hash']])

    def test_background_excluded_actor_context_and_variant_are_blocked(self):
        m,t,a,p=self.fixture(dict(normal_xyz=[128,255,128]*64))
        m['background_shared_hashes']=[p['tile_hash']]
        self.assertEqual(generate(m,t)[0],t)
        m['background_shared_hashes']=[]
        other=dict(a,name='Ganon',draw_model=dict(key='ganon'))
        m['actors'].append(other)
        self.assertEqual(generate(m,t)[0],t)
        m['actors'].pop()
        for field in (dict(geometry_by_flip=True),dict(tile_contexts=['custom'])):
            variant=t+''.join(f'{k} = {json.dumps(v)}\n'for k,v in field.items())
            self.assertEqual(generate(m,variant)[0],variant)

    def test_inherited_and_detached_blind_not_modeled(self):
        m,t,a,p=self.fixture()
        a['graphics_inherited']=True
        self.assertEqual(generate(m,t)[0],t)
        a['graphics_inherited']=False;a['draw_model']['key']='blind-boss';a['poses'][0]['step']=1
        self.assertEqual(generate(m,t)[0],t)

    def test_crown_is_world_up_and_torso_center_not_a_new_cap(self):
        m,t,a,p=self.fixture()
        lm,points=segment(a,a['poses'][0])
        head=next(p for p in points if p['zone']=='head'and p['y']==0 and p['x']==3)
        n=model_pixel(lm,head)
        self.assertGreater(n[2],0.95)
        torso=dict(head,zone='torso',x=lm['cx'],y=lm['head_end'],material='cloth')
        self.assertGreater(model_pixel(lm,torso)[1],model_pixel(lm,torso)[2])

    def test_bent_limb_axis_and_boot_top(self):
        p=dict(x=3,y=2,zone='arm',surface_bounds=(0,0,7,8),path={y:1+y*0.5 for y in range(9)})
        n=limb_normal(p)
        self.assertTrue(np.isfinite(n).all())
        boot=dict(p,y=0,zone='boot')
        self.assertGreater(limb_normal(boot,True)[2],0.9)
        leg=dict(p,y=0,zone='leg')
        self.assertGreater(limb_normal(leg)[1],limb_normal(leg)[2])

    def test_single_head_center_has_outward_not_inward_normals(self):
        left=rounded(7,10,7.5,10,8,8)
        right=rounded(8,10,7.5,10,8,8)
        self.assertLess(left[0],0)
        self.assertGreater(right[0],0)
        self.assertGreater(left[1],0.95)
        self.assertLess(np.linalg.norm(left-right),0.1)

    def test_cloth_normal_varies_both_axes_without_tile_resets(self):
        from alttp_reference_npc_geometry import cloth_normal
        a=cloth_normal(7.999,10,7.5,3,8,20,0.68)
        b=cloth_normal(8.001,10,7.5,3,8,20,0.68)
        self.assertLess(np.linalg.norm(a-b),0.01)
        self.assertGreater(np.linalg.norm(a-cloth_normal(6,14,7.5,3,8,20,0.68)),0.05)

    def test_brightness_evidence_bounded_and_dark_outline_ignored(self):
        from collections import defaultdict
        grid=defaultdict(list)
        for x in range(5):
            q=dict(x=x,y=0,color=0x4210 if x<4 else 0x7fff,zone='head')
            grid[x,0].append(q)
        bright=grid[4,0][0]
        self.assertGreater(lighting_cue(bright,grid),0)
        self.assertLessEqual(lighting_cue(bright,grid),0.12)
        self.assertEqual(lighting_cue(dict(bright,color=0),grid),0)


if __name__=='__main__':unittest.main()
