import unittest
import tomllib
from alttp_activate_diagonal_semantics import activate


class ActivationTest(unittest.TestCase):
    def test_classification_only_and_trim_exception(self):
        source='schema_version = 21\n[game]\nrom_sha256="rom"\n'
        for key,h in [('face',[24]*64),('trim',[0]*64)]:
            source+='\n[[assets]]\ntile_hash="'+key+'"\nheight = '+str(h)+'\n'
        semantics=dict(rom_sha256='rom',tiles={key:dict(role='wall',normals=['south_east']) for key in ('face','trim')})
        result,count=activate(source,semantics);old=tomllib.loads(source);new=tomllib.loads(result)
        self.assertEqual(count,1)
        for a,b in zip(old['assets'],new['assets']):self.assertEqual(a['height'],b['height'])
        self.assertEqual(new['assets'][0]['tile_semantic_normals'],['south_east'])
        self.assertNotIn('tile_role',new['assets'][1])

    def test_conflicting_role_rejects(self):
        source='[game]\nrom_sha256="rom"\n[[assets]]\ntile_hash="face"\ntile_role="door"\nheight=[24]\n'
        with self.assertRaisesRegex(ValueError,'conflicting explicit role'):
            activate(source,dict(rom_sha256='rom',tiles={'face':dict(role='wall',normals=['south_east'])}))
