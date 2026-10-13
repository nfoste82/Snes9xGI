import unittest

from alttp_validate_room_heights import validate, directional_seams, audit_authored_obstacle_capability


class Cell:
    def __init__(self, x, y, tile_hash="floor", hflip=False, vflip=False):
        self.x, self.y, self.tile_hash = x, y, tile_hash
        self.hflip, self.vflip = hflip, vflip


class Snapshot:
    room = 0x60
    def __init__(self):
        self.cells = [[], [Cell(x, y) for y in range(64) for x in range(64)]]
    def cell(self, layer, x, y):
        return self.cells[layer][y * 64 + x]


def graph(solved=None):
    return {"format": "alttp-doorway-graph-review-v1", "room_id": 0x60,
            "planes": [{"reachable_body_cells": [[1, 2], [2, 2]]},
                       {"reachable_body_cells": [[1, 2]]}],
            "elevation": {"cells_plane_x_y_level": solved or [],
                          "conflicting_components": [], "unresolved_stairs": []}}


def attrs():
    result = [bytearray([3] * 4096), bytearray([3] * 4096)]
    for plane in result:
        plane[2 * 64 + 1] = plane[2 * 64 + 2] = 0
    return tuple(bytes(p) for p in result)


PROFILE = {"assets": [{"tile_hash": "floor", "height": [0] * 64}], "asset_groups": {}}
SOLVED = [[0, 1, 2, 1], [0, 2, 2, 1], [1, 1, 2, 0]]


class RoomHeightValidationTests(unittest.TestCase):
    def test_wall_mount_requires_complete_footprint_and_both_backing_sections(self):
        snapshot=Snapshot()
        for c in snapshot.cells[1]:c.word=0xffff
        data=bytearray([255]*12876)
        ramp=[7-x for y in range(8) for x in range(8)]
        profile={'assets':PROFILE['assets']+[{'tile_hash':'mount','height':[7-y for y in range(8) for x in range(8)],'tile_role':'wall'},
                 {'tile_hash':'back','height':ramp,'tile_role':'wall'}],'asset_groups':{}}
        records=[]
        for x in range(2):
            for y in range(4):
                word=0x800+x*4+y;data[0xa44+(x*4+y)*2:0xa46+(x*4+y)*2]=word.to_bytes(2,'little')
                c=snapshot.cell(1,20+x,20+y);c.word=word;c.tile_hash='mount'
                records.append(dict(x=c.x,y=c.y,floor_base=40+8*x,kind='authored_wall',planes=[0],
                                    local_source='placement_wall_mount_backing',local_height=ramp))
            for y in (19,24):
                c=snapshot.cell(1,20+x,y);c.word=1;c.tile_hash='back'
                records.append(dict(x=c.x,y=c.y,floor_base=40+8*x,kind='authored_wall',planes=[0]))
        fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
        fixture['elevation']['structural_placements']=records
        report=validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        self.assertEqual(sum(p.get('local_source')=='placement_wall_mount_backing' for p in report['placements']),8)
        snapshot.cell(1,20,20).word^=1
        with self.assertRaisesRegex(ValueError,'mounted fixture'):validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        snapshot.cell(1,20,20).word^=1
        records[5]['floor_base']+=1
        with self.assertRaisesRegex(ValueError,'mounted fixture'):validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_authored_group_runtime_counts_are_not_independent_approval(self):
        for solution in (None, {'authored_obstacle_cells': 8, 'solved_authored_obstacle_cells': 8,
                                'unmatched_authored_obstacle_cells': 0}):
            report = {'success': True}
            audit_authored_obstacle_capability(report, {'obstacle_groups': [{'name': 'example'}]}, solution)
            self.assertFalse(report['success'])
            self.assertEqual(report['authored_obstacle_groups']['status'],
                             'missing_complete_inputs')
        report = {'success': True}
        audit_authored_obstacle_capability(report, {})
        self.assertEqual(report, {'success': True})

    def test_terminal_corner_two_transformed_face_seams(self):
        for flip in range(4):
            snapshot=Snapshot();snapshot.cells[0]=[Cell(x,y) for y in range(64) for x in range(64)]
            for layer in snapshot.cells:
                for c in layer:c.word=0xffff
            data=bytearray([255]*12876);kind=(2 if flip&1 else 0)+(1 if flip&2 else 0);off=0xbe6+kind*0x20
            profile={'assets':PROFILE['assets']+[{'tile_hash':'corner','height':[max(7-x,7-y) for y in range(8) for x in range(8)]},
                {'tile_hash':'xface','height':[23-y for y in range(8) for x in range(8)]},
                {'tile_hash':'yface','height':[23-x for y in range(8) for x in range(8)]}],
                'asset_groups':{'wall_faces':{'tile_hashes':['xface','yface']}}}
            for yy in range(3):
                for xx in range(4-yy):
                    tx=3-xx if kind>=2 else xx;ty=3-yy if kind%2 else yy;word=100+xx*3+yy
                    data[off+(tx*4+ty)*2:off+(tx*4+ty)*2+2]=word.to_bytes(2,'little')
                    for layer in (0,1):
                        c=snapshot.cell(layer,20+tx,20+ty);c.word=word;c.hflip=bool(flip&1);c.vflip=bool(flip&2)
                        c.tile_hash='corner' if not xx and not yy else 'xface' if xx else 'yface'
            x=20+(3 if flip&1 else 0);y=20+(3 if flip&2 else 0);dx=-1 if flip&1 else 1;dy=-1 if flip&2 else 1
            fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
            fixture['elevation']['structural_placements']=[dict(x=x,y=y,floor_base=16,kind='authored_wall',planes=[0],terminal_corner=True),
                dict(x=x+dx,y=y,floor_base=0,kind='authored_wall',planes=[0]),dict(x=x,y=y+dy,floor_base=0,kind='authored_wall',planes=[0])]
            validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
            fixture['elevation']['structural_placements'][2]['floor_base']=1
            with self.assertRaisesRegex(ValueError,'seam disagreement'):validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_complete_torch_cap_requires_same_plane_support(self):
        snapshot=Snapshot()
        for c in snapshot.cells[1]:c.word=0xffff
        data=bytearray([255]*12876)
        profile={'assets':PROFILE['assets']+[{'tile_hash':'cap','height':[5]*64},{'tile_hash':'foot','height':[3]*64}],
                 'asset_groups':{'obstacles':{'tile_hashes':['foot']}}}
        for n in range(4):
            data[0xeca+n*2:0xecc+n*2]=(100+n).to_bytes(2,'little')
            c=snapshot.cell(1,1+n//2,n%2);c.word=100+n;c.tile_hash='cap' if n%2==0 else 'foot'
        fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
        fixture['elevation']['structural_placements']=[{'x':1,'y':1,'floor_base':50,'kind':'authored_obstacle','planes':[0]}]
        report=validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        self.assertTrue(any(p.get('assembly_source')=='verified_complete_floor_torch' for p in report['placements']))
        fixture['elevation']['structural_placements'][0]['floor_base']=49
        with self.assertRaisesRegex(ValueError,'same-plane support'):validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_complete_column_endpoints_require_nonplaceholder_local_and_full_ownership(self):
        snapshot=Snapshot()
        for c in snapshot.cells[1]:c.word=0xffff
        data=bytearray([255]*12876)
        profile={'assets':PROFILE['assets']+[{'tile_hash':f'w{n}','height':[7-x for y in range(8) for x in range(8)]} for n in range(12)],
                 'asset_groups':{'wall_faces':{'tile_hashes':['w0','w8']}}}
        for n in range(12):
            data[0x89a+n*2:0x89c+n*2]=(100+n).to_bytes(2,'little')
            c=snapshot.cell(1,20+n//4,20+n%4);c.word=100+n;c.tile_hash=f'w{n}'
        fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
        fixture['elevation']['structural_placements']=[{'x':21,'y':20,'floor_base':32,'kind':'authored_wall','planes':[0]}]
        report=validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        self.assertEqual(next(p for p in report['placements'] if p['kind']=='authored_wall')['effective_max'],39)
        snapshot.cell(1,22,23).word^=1
        with self.assertRaisesRegex(ValueError,'not classified'):validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        snapshot.cell(1,22,23).word^=1
        profile['assets'][5]['height']=[0]*64
        with self.assertRaisesRegex(ValueError,'not classified'):validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_split_lower_door_priority_preserves_high_face_not_foot(self):
        snapshot=Snapshot();snapshot.cells[0]=[Cell(x,y) for y in range(64) for x in range(64)]
        for layer in snapshot.cells:
            for c in layer:c.word=0xffff
        data=bytearray([255]*12876)
        profile={'assets':PROFILE['assets']+[{'tile_hash':'face','height':[24+x for y in range(8) for x in range(8)]}], 'asset_groups':{}}
        for n in range(8):data[0x2d8+n*2:0x2da+n*2]=(100+n).to_bytes(2,'little')
        for n in range(12):
            word=200+n
            data[0x2e82+n*2:0x2e84+n*2]=word.to_bytes(2,'little')
            snapshot.cell(1 if n//4==2 else 0,10+n//4,20+n%4).word=word
        for row in range(-1,5):
            for col in range(1,4):
                c=snapshot.cell(1,12+col,20+row);c.word=(100+((row+2)%2)*4+col)|0x2000;c.tile_hash='face'
        fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
        fixture['elevation']['structural_placements']=[{'x':14,'y':21,'floor_base':0,'kind':'authored_wall','planes':[0]}]
        report=validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        self.assertEqual(next(p for p in report['placements'] if p['kind']=='authored_wall')['effective_min'],24)
        snapshot.cell(0,10,21).word^=1
        with self.assertRaisesRegex(ValueError,'not classified'):
            validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_perimeter_flank_ownership_priority_and_transformed_seam(self):
        for flip in (False,True):
            snapshot=Snapshot()
            for c in snapshot.cells[1]: c.word=0xffff
            data=bytearray([255]*12876)
            profile={'assets':PROFILE['assets']+[{'tile_hash':'foot','height':[16+(7-x if flip else x) for y in range(8) for x in range(8)]}],
                     'asset_groups':{'wall_faces':{'tile_hashes':['foot']}}}
            for yy in range(2):
                foot=snapshot.cell(1,10,10+yy);foot.tile_hash='foot';foot.hflip=flip
                for xx in range(1,4):
                    word=100+yy*4+xx
                    data[0x2d8+(yy*4+xx)*2:0x2da+(yy*4+xx)*2]=word.to_bytes(2,'little')
                    c=snapshot.cell(1,10+xx,10+yy);c.word=word^0x2000;c.tile_hash=f'face{xx}';c.hflip=flip
            for xx in range(1,4):
                profile['assets'].append({'tile_hash':f'face{xx}','height':[24+(xx-1)*8+(7-x if flip else x) for y in range(8) for x in range(8)]})
            fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
            fixture['elevation']['structural_placements']=[{'x':11,'y':10,'floor_base':0,'kind':'authored_wall','planes':[0]}]
            report=validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
            self.assertEqual(next(p for p in report['placements'] if p['kind']=='authored_wall')['effective_max'],31)
            snapshot.cell(1,12,11).word^=0x4000
            with self.assertRaisesRegex(ValueError,'not classified'):
                validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_other_plane_occupied_support_is_not_identityless_fallback(self):
        fixture = graph(SOLVED)
        collision = [bytearray(p) for p in attrs()]
        collision[1][2*64+2] = 2  # upper support's artwork covers a lower collider
        report = validate(fixture, 50, snapshot=Snapshot(),
                          attributes=tuple(bytes(p) for p in collision), profile=PROFILE)
        self.assertEqual(report['required_walkable_placements'], 3)
        self.assertEqual(report['required_fallback_placements'], 0)
        self.assertEqual(report['fallback_conflicts'], [])

    def test_exact_corner_boundary_continuation_not_clipped_derivative(self):
        for flip in (False, True):
            s=Snapshot();s.cell(1,10,10).tile_hash='corner';s.cell(1,11,10).tile_hash='wall'
            s.cell(1,10,10).vflip=flip
            local=[max(x,7-y if flip else y) for y in range(8) for x in range(8)]
            profile={'assets':[{'tile_hash':'corner','height':local}, {'tile_hash':'wall','height':[x for y in range(8) for x in range(8)]}]}
            records=[dict(layer=1,plane=0,x=x,y=10,role=2,base=base,fallback_directions=0,has_local=True,floor_datum=255) for x,base in ((10,42),(11,50))]
            report=directional_seams(records,s,attrs(),profile)
            self.assertEqual(report['conflicts'],0)
            self.assertEqual(next(e for e in report['edges'] if e['direction']=='east')['expected_steps'],[1]*8)
            records[1]['base']=49
            self.assertEqual(directional_seams(records,s,attrs(),profile)['conflicts'],2)

    def test_obstacle_relief_is_not_wall_support_seam(self):
        s=Snapshot();s.cell(1,10,10).tile_hash='wall';s.cell(1,10,11).tile_hash='trim'
        profile={'assets':[{'tile_hash':'wall','height':[y for y in range(8) for x in range(8)]}, {'tile_hash':'trim','height':[3]*64}]}
        records=[dict(layer=1,plane=0,x=10,y=y,role=role,base=base,fallback_directions=0,has_local=True,floor_datum=255) for y,role,base in ((10,2,42),(11,3,50))]
        report=directional_seams(records,s,attrs(),profile)
        self.assertEqual(report['conflicts'],0)
        edge=next(e for e in report['edges'] if e['direction']=='south')
        self.assertEqual(edge['neighbor_authored_relief_edge'],[3]*8)
        self.assertEqual(edge['neighbor_contact_source'],'obstacle_support_plane')
        records[1]['base']=49
        self.assertEqual(directional_seams(records,s,attrs(),profile)['conflicts'],1)

    def test_visible_packet_chain_collision_zero_cannot_hide_break(self):
        # Visible placed-surface ownership is separate from actor support. All
        # collision00 must still detect actual repeated7..0 in the packet.
        s=Snapshot();collision=(bytes(4096),bytes(4096));records=[]
        for x in (41,42,43,44):
            s.cell(1,x,10).tile_hash='wall' if x<44 else 'floor'
            records.append(dict(layer=1,plane=0,x=x,y=10,role=2 if x<44 else 1,
                base=0,fallback_directions=0,has_local=True,floor_datum=255,
                surface_scope='observed_visible_placed_surface',
                captured_local_source='captured_authored_asset',
                captured_local_height=[7-xx if x<44 else 0 for yy in range(8) for xx in range(8)]))
        profile={'assets':PROFILE['assets']+[{'tile_hash':'wall','height':[7-x for y in range(8) for x in range(8)]}]}
        before=directional_seams(records,s,collision,profile)
        self.assertEqual(before['conflicts'],4)
        self.assertEqual(before['status_counts']['continuous'],1) # foot/floor
        self.assertNotIn('independent_support_or_occluder',before['status_counts'])
        records[0]['base']=16;records[1]['base']=8
        after=directional_seams(records,s,collision,profile)
        self.assertEqual(after['conflicts'],0)
        self.assertEqual(after['status_counts']['continuous'],5)
        # Unobserved actor-plane scope keeps the occluder distinction.
        for r in records:r.pop('surface_scope')
        self.assertEqual(directional_seams(records,s,collision,profile)['status_counts']['independent_support_or_occluder'],5)

    def test_directional_effective_chain_and_flips(self):
        for flip in (False,True):
            s=Snapshot(); collision=attrs(); records=[]
            for x,base in ((10,16),(11,8),(12,0)):
                s.cell(1,x,10).tile_hash='wall';s.cell(1,x,10).hflip=flip
                records.append(dict(layer=1,plane=0,x=x,y=10,role=2,base=base,
                                    fallback_directions=0,has_local=True,floor_datum=255))
            # Canonical flip transforms to the same displayed east descent.
            local=[x if flip else 7-x for y in range(8) for x in range(8)]
            profile={'assets':[{'tile_hash':'wall','height':local}], 'asset_groups':{}}
            report=directional_seams(records,s,collision,profile)
            self.assertEqual(report['conflicts'],0)
            self.assertEqual(report['status_counts']['continuous'],4)
            records[1]['base']=0
            broken=directional_seams(records,s,collision,profile)
            self.assertEqual(broken['conflicts'],4)
            edge=next(e for e in broken['edges'] if e['direction']=='east' and e['x']==10)
            self.assertEqual(edge['expected_steps'],[-1]*8)
            self.assertEqual(edge['actual_steps'],[-9]*8)
            # Another plane never supplies a missing contact.
            records[1]['plane']=1
            self.assertEqual(directional_seams(records,s,collision,profile)['status_counts']['room_boundary'],12)

    def test_directional_unknown_local_and_base_are_explicit(self):
        s=Snapshot();s.cell(1,10,10).tile_hash=s.cell(1,11,10).tile_hash='wall'
        records=[dict(layer=1,plane=0,x=x,y=10,role=2,base=base,fallback_directions=0,has_local=True,floor_datum=255)
                 for x,base in ((10,8),(11,255))]
        profile={'assets':[{'tile_hash':'wall','height':[7-x for y in range(8) for x in range(8)]}]}
        self.assertEqual(directional_seams(records,s,attrs(),profile)['missing'],2)
    def test_template_middle_requires_exact_placed_words_not_hash(self):
        snapshot=Snapshot()
        for c in snapshot.cells[1]: c.word=0xffff
        data=bytearray(12876)
        for n in range(8): data[0x2e8+n*2:0x2ea+n*2]=(n+100).to_bytes(2,'little')
        for xx in range(2):
            for yy in range(3):
                c=snapshot.cell(1,10+xx,10+yy);c.word=100+xx*4+yy
                c.tile_hash='middle' if yy==1 else 'wall'
        profile={'assets':PROFILE['assets']+[{'tile_hash':'wall','height':[7]*64},
                    {'tile_hash':'middle','height':[39-y for y in range(8) for x in range(8)]}],
                 'asset_groups':{'wall_faces':{'tile_hashes':['wall']}}}
        fixture=graph(SOLVED);fixture['_verified_rom_template_data']=data
        fixture['elevation']['structural_placements']=[{'x':10,'y':11,'floor_base':26,'kind':'authored_wall','planes':[0]}]
        report=validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)
        middle=next(p for p in report['placements'] if p['kind']=='authored_wall')
        self.assertEqual((middle['effective_min'],middle['effective_max']),(58,65))
        # Both alternate recognition paths are broken at the same endpoint.
        snapshot.cell(1,10,12).word=999
        with self.assertRaisesRegex(ValueError,'not classified'):
            validate(fixture,50,snapshot=snapshot,attributes=attrs(),profile=profile)

    def test_negative_wall_base_is_not_clamped_to_zero(self):
        fixture = graph(SOLVED)
        fixture["elevation"]["structural_placements"] = [
            {"x": 3, "y": 2, "floor_base": -8, "kind": "authored_wall", "planes": [0]}]
        snapshot = Snapshot(); snapshot.cell(1,3,2).tile_hash = "wall"
        local = [7-x for y in range(8) for x in range(8)]
        profile = {"assets": PROFILE["assets"] + [{"tile_hash": "wall", "height": local}],
                   "asset_groups": {"wall_faces": {"tile_hashes": ["wall"]}}}
        visible = [255]*4096; planes = [[255]*4096 for _ in range(2)]
        visible[129] = visible[130] = 50; visible[131] = -8
        planes[0][129] = planes[0][130] = 50; planes[0][131] = -8; planes[1][129] = 0
        report = validate(fixture,50,{"visible":visible,"planes":planes},
                          snapshot=snapshot,attributes=attrs(),profile=profile)
        self.assertTrue(report["success"])
        wall = next(p for p in report["placements"] if p["kind"] == "authored_wall")
        self.assertEqual((wall["effective_min"],wall["effective_max"]),(-8,-1))

    def test_effective_mixed_floor_array_is_independently_checked_all_flips(self):
        for flip in range(4):
            snapshot = Snapshot()
            x, y = 3, 2
            cell = snapshot.cell(1, x, y)
            cell.tile_hash = "mixed"
            cell.hflip, cell.vflip = bool(flip & 1), bool(flip & 2)
            # Canonical zero foot edge faces west after placement transforms.
            local = [max(0, (7-xx if cell.hflip else xx)-3) for yy in range(8) for xx in range(8)]
            normals = [v for h in local for v in ([128,128,255] if h == 0 else [220,215,128])]
            profile = {"assets": PROFILE["assets"] + [{"tile_hash": "mixed", "height": local, "normal_xyz": normals}],
                       "asset_groups": {"wall_faces": {"tile_hashes": ["mixed"]},
                                        "dungeon_floor": {"tile_hashes": ["floor"]}}}
            fixture = graph()
            fixture["planes"][1]["reachable_body_cells"] = []
            fixture["elevation"]["cells_plane_x_y_base"] = [[0,1,2,24], [0,2,2,24]]
            record = {"x":x, "y":y, "floor_base":20, "kind":"authored_wall", "planes":[0],
                      "attached_floor_base":24, "local_source":"placement_grounded_floor_clamp",
                      "local_height":[max(h,4) for h in local]}
            fixture["elevation"]["structural_placements"] = [record]
            collision = [bytearray(p) for p in attrs()]; collision[0][y*64+x] = 1
            report = validate(fixture,50,snapshot=snapshot,attributes=collision,profile=profile)
            wall = next(p for p in report["placements"] if p["kind"] == "authored_wall")
            self.assertEqual(wall["effective_min"],24)
            self.assertEqual(wall["local_height"],record["local_height"])
            record["local_height"][0] = 0
            with self.assertRaisesRegex(ValueError,"reproduce clamp"):
                validate(fixture,50,snapshot=snapshot,attributes=collision,profile=profile)
            record["local_height"] = [max(h,4) for h in local]
            record["attached_floor_base"] = 25
            with self.assertRaisesRegex(ValueError,"grounded connectivity"):
                validate(fixture,50,snapshot=snapshot,attributes=collision,profile=profile)

    def test_unresolved_mixed_foot_fails_gate(self):
        fixture = graph(SOLVED)
        fixture["elevation"]["mixed_floor_unknown"] = 1
        self.assertFalse(validate(fixture,50,snapshot=Snapshot(),attributes=attrs(),profile=PROFILE)["success"])

    def test_variable_bases_do_not_multiply_by_configured_stair_rise(self):
        fixture = graph(SOLVED)
        fixture["elevation"]["cells_plane_x_y_base"] = [[0, 1, 2, 32], [0, 2, 2, 32], [1, 1, 2, 0]]
        report = validate(fixture, 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertTrue(report["success"])
        self.assertEqual(report["placements"][0]["floor_base"], 32)

    def test_requires_every_walkable_placement_on_each_plane(self):
        report = validate(graph(SOLVED[:-1]), 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertEqual(report["missing_placements"][0]["plane"], 1)

    def test_false_pass_every_hash_has_placeholder_but_no_placement_solution(self):
        # Regression for the old gate: existence of a zero array on the hash is
        # not effective geometry when no placement level/base was assigned.
        report = validate(graph([]), 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertEqual(len(report["missing_placements"]), 3)
        self.assertEqual(report["valid_walkable_placements"], 0)

    def test_plane_overlap_and_complete_effective_geometry(self):
        report = validate(graph(SOLVED), 50, snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertTrue(report["success"])
        self.assertEqual(report["per_plane_walkable_denominator"], [2, 1])
        self.assertEqual(report["plane_overlap_cells"], 1)
        self.assertEqual(report["valid_walkable_placements"], 3)

    def test_fallback_requires_placement_ramp_and_direction(self):
        collision = [bytearray(p) for p in attrs()]
        collision[0][2 * 64 + 3] = 1
        report = validate(graph(SOLVED), 50, snapshot=Snapshot(),
                          attributes=tuple(bytes(p) for p in collision), profile=PROFILE)
        ramps = [p for p in report["placements"] if p["kind"] == "collidable_fallback"]
        self.assertTrue(ramps)
        self.assertTrue(all(p["local_source"] == "placement_ramp_0_7" for p in ramps))
        self.assertTrue(all(min(p["local_height"]) == 0 and max(p["local_height"]) == 7 for p in ramps))

    def test_same_level_corner_composes_minimum_outward_ramps(self):
        collision = [bytearray(p) for p in attrs()]
        collision[0][2 * 64] = 1  # West of support (1,2).
        collision[0][1 * 64 + 1] = 1  # North of the same support.
        # Make the northwest corner itself border both solved cells (1,2) and
        # (2,1) in a tiny explicit graph.
        fixture = graph([[0, 1, 2, 1], [0, 2, 1, 1], [0, 2, 2, 1]])
        fixture["planes"][0]["reachable_body_cells"] = [[1, 2], [2, 1], [2, 2]]
        fixture["planes"][1]["reachable_body_cells"] = []
        collision[0][1 * 64 + 2] = 0
        report = validate(fixture, 50, snapshot=Snapshot(),
                          attributes=tuple(bytes(p) for p in collision), profile=PROFILE)
        corner = next(p for p in report["placements"]
                      if p["kind"] == "collidable_fallback" and p["x"] == 1 and p["y"] == 1)
        self.assertEqual(corner["displayed_directions"], ["west", "north"])
        self.assertEqual(corner["composition"], "minimum_outward_ramps")
        self.assertEqual(corner["local_height"][0], 7)
        self.assertEqual(corner["local_height"][7], 0)
        self.assertEqual(corner["local_height"][56], 0)
        self.assertEqual(corner["local_height"][63], 0)

    def test_compiled_map_must_contain_effective_base(self):
        height_map = [255] * 4096
        report = validate(graph(SOLVED), 50, height_map, snapshot=Snapshot(),
                          attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertTrue(report["compiled_map_missing"])

    def test_plane_aware_map_cannot_hide_wrong_underpass_base(self):
        visible = [255] * 4096
        visible[129] = visible[130] = 50
        planes = [[255] * 4096 for _ in range(2)]
        planes[0][129] = planes[0][130] = 50
        planes[1][129] = 50 # wrong: lower underpass should be zero
        report = validate(graph(SOLVED), 50, {"visible": visible, "planes": planes},
                          snapshot=Snapshot(), attributes=attrs(), profile=PROFILE)
        self.assertFalse(report["success"])
        self.assertEqual(report["compiled_map_mismatches"], [[1, 1, 2, 0, 50]])


if __name__ == "__main__":
    unittest.main()
