import json
from pathlib import Path
import tempfile
import tomllib
import unittest

from alttp_atlas_profile import _render_profile, _semantic_wall_layers, _enable_upper_floor_height
from alttp_reference_wall_templates import ReferenceTemplates, transform, calibrated_diagonal
from alttp_reference_source_merge import merge, AUTHORIZED


class ReferenceWallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'templates.json'
        self.normal = tuple((p * 7 + c * 23) % 256 for p in range(64) for c in range(3))

    def templates(self, entries, assets):
        self.path.write_text(json.dumps({'format': 'alttp-reference-wall-templates-v1',
                                        'rom_sha256': 'rom', 'templates': entries}))
        return ReferenceTemplates(self.path, assets, 'rom')

    def entry(self, **kw):
        return {'id': 'east', 'reference': 'ref', 'role': 'wall', 'facings': ['east'],
                'evidence': 'explicit test reference', **kw}

    def test_exact_cardinal_profile_not_normalized(self):
        height = tuple(40 + (7-x)*2 + y%2 for y in range(8) for x in range(8))
        t = self.templates([self.entry()], {'ref': {'height': height, 'normal_xyz': self.normal}})
        east, r = t.resolve('east', {'role': 'wall', 'normals': ['east']})
        self.assertEqual(r['classification'], 'matched')
        self.assertEqual(east['height'], height)
        west, r = t.resolve('west', {'role': 'wall', 'normals': ['west']})
        self.assertEqual(r['classification'], 'matched')
        self.assertEqual(west['height'],transform(height,hflip=True))

    def test_disagreeing_references_are_ambiguous(self):
        t = self.templates([self.entry(),self.entry(id='second',reference='other')],
                           {'ref':{'height':[0]*64,'normal_xyz':self.normal},
                            'other':{'height':[1]*64,'normal_xyz':self.normal}})
        local,r = t.resolve('target',{'role':'wall','normals':['east']})
        self.assertIsNone(local)
        self.assertEqual(r['classification'],'ambiguous')

    def test_symmetric_cardinal_flip_and_normal_sign(self):
        height = tuple(7-x for y in range(8) for x in range(8))
        normal = (255,128,128)*64
        t = self.templates([self.entry()], {'ref': {'height': height, 'normal_xyz': normal}})
        west,r = t.resolve('west', {'role': 'wall', 'normals': ['west']})
        self.assertEqual(r['classification'], 'matched')
        self.assertEqual(west['height'][:8], tuple(range(8)))
        self.assertEqual(west['normal_xyz'][:3], (1,128,128))

    def test_corner_uses_reference_samples_both_facings(self):
        height = tuple((x*x+y*3)%19 for y in range(8) for x in range(8))
        t = self.templates([self.entry(role='wall_corner', corner_type='inside',
                                      facings=['east','south'])],
                           {'ref': {'height': height, 'normal_xyz': self.normal}})
        local,r = t.resolve('corner', {'role':'wall_corner','corner_type':'inside',
                                      'normals':['west','north']})
        self.assertEqual(r['classification'],'matched')
        self.assertEqual(local['height'],transform(height,hflip=True,vflip=True))
        self.assertEqual(local['normal_xyz'],transform(self.normal,3,True,True,True))

    def test_mixed_diagonal_keeps_explicit_clipped_floor(self):
        height = [max(0, 7-x-y) for y in range(8) for x in range(8)]
        mask = sum(1<<p for p,h in enumerate(height) if h == 0)
        t = self.templates([self.entry(tile_hashes=['bottom'], height=height,
                                      floor_mask=hex(mask))], {'ref': {'normal_xyz':self.normal}})
        local,r = t.resolve('bottom',{})
        self.assertEqual(local['height'],tuple(height))
        self.assertEqual(local['height'][56:],(0,)*8)
        self.assertEqual(r['classification'],'matched')
        with self.assertRaisesRegex(ValueError,'floor pixels'):
            self.templates([self.entry(height=[7]*64,floor_mask=hex(mask))],{})

    def test_unreviewed_diagonal_is_missing_not_ramp(self):
        t = self.templates([self.entry(tile_hashes=['bottom'],status='unresolved')],
                           {'ref': {'height':[0]*64,'normal_xyz':self.normal}})
        local,r = t.resolve('bottom',{})
        self.assertIsNone(local)
        self.assertEqual(r['classification'],'missing')

    def test_authorized_diagonal_all_facings_and_mixed_parts_excluded(self):
        heights = tuple(max(0,7-x-y) for y in range(8) for x in range(8))
        entries = [self.entry(id='middle',facings=['south_east']),
                   self.entry(id='bottom',reference='bottom',tile_hashes=['bottom'],status='unresolved'),
                   self.entry(id='upper',reference='upper',tile_hashes=['upper'],status='unresolved')]
        t = self.templates(entries,{'ref':{'height':heights,'normal_xyz':self.normal},
                                   'bottom':{'height':heights,'normal_xyz':self.normal}})
        for facing,h,v in [('south_east',False,False),('south_west',True,False),
                           ('north_east',False,True),('north_west',True,True)]:
            annotation = {'role':'wall','normals':[facing],'context_dependent':False}
            local,r = t.resolve('full-wall',annotation)
            self.assertEqual(r['classification'],'matched')
            self.assertEqual(local['height'],transform(heights,hflip=h,vflip=v))
            self.assertEqual(local['normal_xyz'],transform(self.normal,3,h,v,True))
            for part in ('bottom','upper'):
                local,r = t.resolve(part,annotation)
                self.assertIsNone(local)
                self.assertEqual(r['classification'],'missing')
        local,r = t.resolve('context-dependent',{'role':'wall','normals':['south_east'],
                                                'context_dependent':True})
        self.assertIsNone(local)

    def test_geometry_override_preserves_normal_text_bit_exact_multiline(self):
        literal = 'normal_xyz = [\n  '+', '.join(map(str,self.normal))+'\n]\n'
        source = '[[assets]]\ntile_hash = "tile"\nheight = [\n  42\n]\n'+literal
        authored = {'tile':tomllib.loads(source)['assets'][0]}
        local = _semantic_wall_layers({'role':'wall','normals':['east']})
        result = _render_profile(source,authored,{'tile':local},{'tile'})
        self.assertIn(literal,result)
        self.assertEqual(tomllib.loads(result)['assets'][0]['normal_xyz'],list(self.normal))
        self.assertEqual(tomllib.loads(result)['assets'][0]['height'],list(local['height']))

    def test_only_absent_normal_is_transferred(self):
        t = self.templates([self.entry()], {'ref':{'height':[9]*64,'normal_xyz':self.normal}})
        local,_ = t.resolve('ref',{})
        source = '[[assets]]\ntile_hash = "tile"\n'
        result = _render_profile(source,{'tile':{'tile_hash':'tile'}},{'tile':local},{'tile'})
        self.assertEqual(tomllib.loads(result)['assets'][0]['normal_xyz'],list(self.normal))

    def calibration(self):
        cross = [5]*9 + [4,3,2,1,1,0]
        return {'height': [cross[x+y] for y in range(8) for x in range(8)],
                'normal_xyz': self.normal}

    def test_calibrated_face_all_flips_and_local_zero(self):
        t = self.templates([self.entry(facings=['south_east'], generator='calibrated_diagonal',
                                      calibration_reference='cap')],
                           {'ref': {'normal_xyz': self.normal}, 'cap': self.calibration()})
        for facing,h,v in [('south_east',False,False),('south_west',True,False),
                           ('north_east',False,True),('north_west',True,True)]:
            local, record = t.resolve('wall', {'role':'wall','normals':[facing]})
            self.assertEqual(local['height'], transform(tuple(14-x-y for y in range(8) for x in range(8)),hflip=h,vflip=v))
            self.assertEqual(min(local['height']),0)
            self.assertEqual(record['calibrations'][0]['unit_slope'],1)

    def test_upper_cap_and_authored_cap_not_generic(self):
        normal = tuple(c for y in range(8) for x in range(8)
                       for c in ((128,128,255) if x+y<=1 else (220,215,128)))
        t = self.templates([self.entry(reference='upper', tile_hashes=['upper'],
                                      generator='calibrated_diagonal',calibration_reference='cap',authored_cap=True),
                            self.entry(id='cap',reference='cap',tile_hashes=['cap']),
                            self.entry(id='generic', reference='ref',facings=['south_east'],
                                       generator='calibrated_diagonal',calibration_reference='cap')],
                           {'upper':{'normal_xyz':normal},'cap':self.calibration(),
                            'ref':{'normal_xyz':self.normal}})
        local,_ = t.resolve('upper',{})
        self.assertEqual(local['height'][0:2],(13,13))
        self.assertEqual(local['height'][-1],0)
        local,_ = t.resolve('cap',{'role':'wall','normals':['south_east']})
        self.assertEqual(local['height'],tuple(self.calibration()['height']))
        with self.assertRaisesRegex(ValueError,'no exact up'):
            calibrated_diagonal(self.calibration(),(220,215,128)*64)

    def test_invalid_calibration_not_normalized(self):
        source = self.calibration()
        source['height'] = [v+50 for v in source['height']]
        with self.assertRaisesRegex(ValueError,'local zero'):
            calibrated_diagonal(source)

    def test_saved_floor_region_clips_measured_slope(self):
        normal = tuple(c for y in range(8) for x in range(8)
                       for c in ((128,128,255) if x+y>=10 else (220,215,128)))
        t = self.templates([self.entry(reference='bottom',tile_hashes=['alias'],
                                      generator='calibrated_diagonal',calibration_reference='cap',
                                      authored_floor=True)],
                           {'bottom':{'normal_xyz':normal},'cap':self.calibration()})
        for key in ('bottom','alias'):
            local,r = t.resolve(key,{'role':'wall','normals':['south_east']})
            self.assertEqual(local['height'],tuple(max(0,10-x-y) for y in range(8) for x in range(8)))
            self.assertEqual(local['normal_xyz'],normal)
            c = r['calibrations'][0]
            self.assertEqual(c['floor_pixels'],15)
            self.assertEqual(c['wall_pixels'],49)
            self.assertEqual(c['floor_mask_rows'],['WWWWWWWW']*3+[
                'WWWWWWWF','WWWWWWFF','WWWWWFFF','WWWWFFFF','WWWFFFFF'])
            mask = int(c['floor_mask'],0)
            for h,v in ((False,False),(True,False),(False,True),(True,True)):
                displayed = transform(local['height'],hflip=h,vflip=v)
                region = transform(tuple((mask>>p)&1 for p in range(64)),hflip=h,vflip=v)
                self.assertTrue(all(z==0 for z,f in zip(displayed,region) if f))

    def test_floor_mask_requires_complete_authored_region(self):
        normal = list((220,215,128)*64)
        with self.assertRaisesRegex(ValueError,'both floor and face'):
            calibrated_diagonal(self.calibration(),floor_normals=normal)
        normal[-3:] = [128,128,255]
        normal[:3] = [128,128,255]
        with self.assertRaisesRegex(ValueError,'half-plane'):
            calibrated_diagonal(self.calibration(),floor_normals=normal)
        with self.assertRaisesRegex(ValueError,'both floor and face'):
            calibrated_diagonal(self.calibration(),floor_normals=(128,128,255)*64)

    def test_inconsistent_measured_slope_rejected(self):
        source = self.calibration()
        source['height'][0] = 7
        with self.assertRaisesRegex(ValueError,'inconsistent descent'):
            calibrated_diagonal(source)

    def test_floor_is_not_a_full_diagonal_wall(self):
        t = self.templates([self.entry(facings=['south_east'],generator='calibrated_diagonal',
                                      calibration_reference='cap')],
                           {'ref':{'normal_xyz':self.normal},'cap':self.calibration()})
        local,record = t.resolve('explicit-floor',{'role':'floor','normals':[]})
        self.assertIsNone(local)
        self.assertEqual(record['classification'],'missing')

    def test_configured_floor_rise_is_not_reset(self):
        source = 'schema_version = 18\n[lighting_space]\nupper_floor_height = 37\n'
        result,rise = _enable_upper_floor_height(source,tomllib.loads(source))
        self.assertEqual(result,source)
        self.assertEqual(rise,37)

    def test_authorized_merge_keeps_other_normals_and_settings_exact(self):
        current = 'schema_version = 18\n[lighting_space]\nupper_floor_height = 37\n'
        backup = 'schema_version = 13\n'
        for key in (*AUTHORIZED, 'other'):
            block = f'\n[[assets]]\ntile_hash = "{key}"\nheight = [0]\nnormal_xyz = [1, 2, 3]\n'
            current += block
            if key != 'v1:4bpp:1a032ad7756c49db':
                backup += block.replace('height = [0]','height = [9]').replace('normal_xyz = [1, 2, 3]','normal_xyz = [4, 5, 6]')
        result, report = merge(current,backup)
        parsed = tomllib.loads(result)
        assets = {a['tile_hash']:a for a in parsed['assets']}
        self.assertEqual(parsed['lighting_space']['upper_floor_height'],37)
        self.assertEqual(assets['other']['normal_xyz'],[1,2,3])
        self.assertIn('normal_xyz = [1, 2, 3]',result)
        self.assertEqual(assets['v1:4bpp:a4aa6ebd24fefeb7']['height'],[9])
        self.assertEqual(assets['v1:4bpp:31aba23efb31666b']['height'],[0])
        self.assertEqual(assets['v1:4bpp:1a032ad7756c49db']['height'],[0]*64)
        self.assertEqual(len(report['authorized_fields']),5)


if __name__ == '__main__':
    unittest.main()
