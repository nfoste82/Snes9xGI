import json
from pathlib import Path
import unittest
import tomllib
from alttp_diagonal_gaps import diagonal_universe, intrinsic_status, summarize, conversion_equivalent


class DiagonalGapTest(unittest.TestCase):
    def test_universe_not_filtered_by_dispatch_or_context(self):
        semantics={
            'unknown':dict(role='wall',normals=['south_east'],context_dependent=True),
            'alias':dict(role='wall',normals=['southeast']),
            'multi':dict(role='wall',normals=['north','south_east']),
            'notwall':dict(role='floor',normals=['south_east']),
            'cardinal':dict(role='wall',normals=['south'])}
        self.assertEqual(diagonal_universe(semantics),{'unknown','alias','multi'})
        summary=summarize([intrinsic_status({}) for _ in diagonal_universe(semantics)])
        self.assertEqual(summary['semantic_denominator'],3)
        self.assertEqual(summary['intrinsic_gaps'],3)

    def test_default_occlusion_and_role_are_separate(self):
        asset=dict(height=[0]*64,normal_xyz=[128,128,255]*64)
        status=intrinsic_status(asset)
        self.assertFalse(status['gap'])
        self.assertEqual(status['occlusion_behavior'],'default_full255')
        self.assertIsNone(status['profile_role'])
        self.assertEqual(intrinsic_status(dict(asset,occlusion=[255]*64))['occlusion_behavior'],'explicit_full255')
        self.assertEqual(intrinsic_status(dict(asset,occlusion=[0]*64))['occlusion_behavior'],'explicit_painted')
        self.assertTrue(intrinsic_status(dict(asset,normal_xyz=[0]*191))['gap'])

    def test_conversion_requires_rom_slot_and_exact_zero_mask(self):
        source=dict(sources=[dict(source_kind='background',pack_index=18,tile_index=26,snes_address=123,conversion='high')],
                    decoded_index_rows=[[0,9,10,11,12,13,14,15]]*8)
        target=dict(source,sources=[dict(source['sources'][0],conversion='low')],decoded_index_rows=[[0,1,2,3,4,5,6,7]]*8)
        self.assertTrue(conversion_equivalent(target,source))
        self.assertFalse(conversion_equivalent(dict(target,sources=[dict(source['sources'][0],tile_index=27)]),source))
        self.assertFalse(conversion_equivalent(dict(target,decoded_index_rows=[[0,0,2,3,4,5,6,7]]*8),source))
        self.assertFalse(conversion_equivalent(dict(target,sources=target['sources']+[dict(source['sources'][0],tile_index=27)]),source))
        self.assertFalse(conversion_equivalent(dict(target,decoded_index_rows=[[0,1,2,3,4,5,6,7]]*7),source))

    def test_real_semantic_denominator_and_explicit_remaining_gaps(self):
        root=Path(__file__).resolve().parents[3]
        semantics=json.loads((root/'remaster/tools/ALttP/alttp-tile-semantics-v1.json').read_text())['tiles']
        assets={a['tile_hash']:a for a in tomllib.loads((root/'remaster/profiles/ALttP/alttp-profile.toml').read_text())['assets']}
        universe=diagonal_universe(semantics)
        self.assertEqual(len(universe),70)
        gaps={key for key in universe if intrinsic_status(assets.get(key,{}))['gap']}
        expected={'4527c0415be5feae','65f25342cf31afce','abe52639a643975e',
                  'd5cfe79bacf8bfd5','d7283c7d2135e845','f3b8b9d8b8ba394e'}
        self.assertEqual(gaps,{'v1:4bpp:'+key for key in expected})
        self.assertEqual(summarize([intrinsic_status(assets.get(key,{})) for key in universe])['intrinsic_complete'],64)


if __name__=='__main__':unittest.main()
