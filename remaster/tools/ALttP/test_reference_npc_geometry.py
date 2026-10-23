"""Regression checks for reference-specific surfaces and preservation."""
import unittest
import numpy as np

from alttp_reference_npc_geometry import cloth_normal, dome, flat_bevel, generate, guard_opening, ganon_surface, ganon_armor_detail, ganon_roles, top_facing
from test_humanoid_geometry import HumanoidGeometryTests


class ReferenceNpcTests(unittest.TestCase):
    def test_shield_center_is_plane_not_sphere(self):
        a=flat_bevel(5,5,(0,0,12,12))
        b=flat_bevel(8,8,(0,0,12,12))
        np.testing.assert_allclose(a,b)
        self.assertGreater(flat_bevel(5,0,(0,0,12,12))[2],0.8)
        self.assertLess(flat_bevel(0,5,(0,0,12,12))[0],-0.5)

    def test_dome_crown_and_flanks(self):
        self.assertGreater(dome(0,-5,0,0,6,6)[2],0.8)
        self.assertLess(dome(-5,0,0,0,6,6)[0],0)
        self.assertGreater(dome(5,0,0,0,6,6)[0],0)

    def test_dome_has_no_branch_stripe_at_old_crown_cutoff(self):
        # Old code jumped from -Y to +Y at v=-.70, often across tile edges.
        a=dome(0,-4.201,0,0,6,6)
        b=dome(0,-4.199,0,0,6,6)
        self.assertLess(np.linalg.norm(a-b),0.002)
        for x in (-3.601,-3.599,3.599,3.601):
            self.assertLess(np.linalg.norm(dome(x,-5,0,0,6,6)-dome(x+0.002,-5,0,0,6,6)),0.005)

    def test_cloth_has_fold_slopes_and_vertical_undulation(self):
        normals=[cloth_normal(x,19,0,0,12,26,1.5) for x in range(-10,11)]
        self.assertGreater(max(n[0] for n in normals),0.3)
        self.assertLess(min(n[0] for n in normals),-0.3)
        self.assertGreater(np.linalg.norm(cloth_normal(4,12)-cloth_normal(4,22)),0.10)
        # Continuous across integer rows and 8px tile borders, without a normal
        # reset or phase restart at the bottom of each canonical tile.
        for y in (7.999,15.999,23.999):
            self.assertLess(np.linalg.norm(cloth_normal(4,y)-cloth_normal(4,y+0.002)),0.003)

    def test_ganon_front_surface_does_not_roll_away_at_top_row(self):
        normals=[ganon_surface(7.5,y,7.5,10,15,9,0.34) for y in range(3,15)]
        self.assertTrue(all(n[1]>0.65 for n in normals))
        angles=[np.degrees(np.arccos(np.clip(a@b,-1,1))) for a,b in zip(normals,normals[1:])]
        self.assertLess(max(angles),6)

    def test_world_up_caps_are_z_dominant_not_camera_facing(self):
        front=np.array([0,1,0],dtype=float)
        cap=top_facing(front,0,0,0,0,8,6)
        self.assertGreater(cap[2],0.99)
        self.assertLess(abs(cap[1]),0.1)
        np.testing.assert_allclose(top_facing(front,0,6,0,0,8,6),front)
        samples=[top_facing(front,0,y,0,0,8,6) for y in np.linspace(0,6,100)]
        self.assertLess(max(np.linalg.norm(a-b)for a,b in zip(samples,samples[1:])),0.05)

    def test_ganon_frame16_head_dma_is_not_split_into_arm_roles(self):
        def p(h,address,slot):return dict(tile_hash=h,address=address,slot=slot)
        actor=dict(poses=[dict(step=15,parts=[p('head-left',0x5c10,14),
            p('head-mid',0x5c20,15),p('head-right',0x5c30,15),
            p('head-bottom',0x5d20,15),p('torso-right',0x5e80,9),
            p('shoulder-right',0x5c80,7)])])
        roles=ganon_roles(actor)
        for h in ('head-left','head-mid','head-right','head-bottom'):
            self.assertEqual(roles[h],'head')
        self.assertEqual(roles['torso-right'],'belly')
        self.assertEqual(roles['shoulder-right'],'shoulder')

    def test_ganon_frame17_mirrored_head_and_torso_are_shared_surfaces(self):
        parts=[dict(tile_hash=f'part-{s}',address=0x5000+s*16,slot=s)
               for s in range(5,17)]
        roles=ganon_roles(dict(poses=[dict(step=16,parts=parts)]))
        for s in (5,6):self.assertEqual(roles[f'part-{s}'],'head')
        for s in (7,8):self.assertEqual(roles[f'part-{s}'],'shoulder')
        for s in (9,10,11,12):self.assertEqual(roles[f'part-{s}'],'arm')
        for s in (13,14):self.assertEqual(roles[f'part-{s}'],'belly')
        for s in (15,16):self.assertEqual(roles[f'part-{s}'],'boot')
        # One shared center produces nearly +Y, with outward (not inward)
        # lateral signs on either side of the mirrored join.
        left=ganon_surface(7,10,7.5,10,15,9,0.34)
        right=ganon_surface(8,10,7.5,10,15,9,0.34)
        self.assertLess(left[0],0)
        self.assertGreater(right[0],0)
        self.assertGreater(left[1],0.9)
        self.assertGreater(right[1],0.9)

    def test_ganon_pendant_spans_mirrored_body_halves(self):
        base=ganon_surface(7.5,6,7.5,10,15,9,0.34)
        left=ganon_armor_detail(7,5,'belly',1,1,base,(-8,3,7,18))
        right=ganon_armor_detail(8,5,'belly',1,1,base,(8,3,23,18))
        self.assertEqual(left[0],'skull')
        self.assertEqual(right[0],'skull')
        np.testing.assert_allclose(left[1]*[-1,1,1],right[1])
        socket=ganon_armor_detail(5,7,'belly',1,5,base,(-8,3,7,18))
        self.assertEqual(socket[0],'socket')
        self.assertLess(socket[1][2],0)

    def test_ganon_armor_highlights_do_not_become_separate_bumps(self):
        base=ganon_surface(0,0,0,0,8,8,0.4)
        for role in ('belly','shoulder'):
            a=ganon_armor_detail(1,12,role,3,3,base,(-8,-13,7,2))
            b=ganon_armor_detail(1,12,role,4,4,base,(-8,-13,7,2))
            np.testing.assert_allclose(a[1],b[1])

    def test_ganon_gold_plate_has_no_scanline_switch_at_y11(self):
        base=ganon_surface(1,11,7.5,10,15,9,0.34)
        a=ganon_armor_detail(1,10.999,'belly',6,6,base,(-8,3,7,18))[1]
        b=ganon_armor_detail(1,11.001,'belly',6,6,base,(-8,3,7,18))[1]
        self.assertLess(np.linalg.norm(a-b),0.002)

    def test_visor_has_inward_rim_and_no_back_head_opening(self):
        self.assertIsNone(guard_opening(7,10,'north'))
        self.assertLess(guard_opening(7,8.3,'south')[1][2],-0.5)
        self.assertGreater(guard_opening(7,11.9,'south')[1][2],0.5)
        self.assertGreater(guard_opening(3.5,10,'south')[1][0],0.5)
        self.assertLess(guard_opening(11.5,10,'south')[1][0],-0.5)
        east=guard_opening(6,10,'east')[1]
        west=guard_opening(9,10,'west')[1]
        np.testing.assert_allclose(east*[-1,1,1],west)

    def test_all_existing_heights_preserved_including_ramps(self):
        fixture=HumanoidGeometryTests()
        p=fixture.part()
        actor=fixture.actor(p,key='agahnim',name='Agahnim')
        actor['poses'][0]['facing']='fixed'
        heights=[20-i//8 for i in range(64)]
        source=fixture.text(p,dict(height=heights,normal_xyz=[128,255,128]*64))
        candidate,report,assets,_=generate(fixture.manifest([actor]),source)
        self.assertEqual(assets[p['tile_hash']]['height'],heights)
        self.assertEqual(report['authored_height_relief_tiles'],0)
        self.assertEqual(report['replaced_placeholders'],1)
        second,report2,_,_=generate(fixture.manifest([actor]),candidate)
        self.assertEqual(second,candidate)
        self.assertEqual(report2['changed_tiles'],0)


if __name__=='__main__':
    unittest.main()
