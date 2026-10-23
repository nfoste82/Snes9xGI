import unittest
import tomllib
from migrate_artwork_emission import migrate


class ArtworkEmissionMigrationTests(unittest.TestCase):
    def test_preserves_intensity_and_every_other_field(self):
        rgba = [value for p in range(64) for value in (p, 255-p, 17, p*4)]
        source = ('schema_version = 30\n[game]\ntitle = "Test"\n'
                  '[[assets]]\ntile_hash = "test"\nheight = [1, 2]\n'
                  f'emission_rgba = {rgba}\nemission_depth = 3\n')
        result, count = migrate(source)
        self.assertEqual(count, 1)
        asset = tomllib.loads(result)['assets'][0]
        self.assertEqual(asset['emission_intensity'], list(range(0, 256, 4)))
        self.assertEqual(asset['height'], [1, 2])
        self.assertEqual(asset['emission_depth'], 3)
        self.assertNotIn('emission_rgba', asset)
        self.assertEqual(migrate(result), (result, 0))
