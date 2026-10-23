import unittest

from alttp_height_samples import valid_heights, height_bytes
from alttp_validate_room_heights import _profile
from alttp_atlas_profile import _format_layer, _render_profile, ProfileGenerationError
from alttp_npc_workbench import validate_layers


class SignedHeightSamples(unittest.TestCase):
    def test_schema_boundary_and_strict_integers(self):
        for value in (-32768, -1, 256, 32767):
            samples = [value] * 64
            self.assertTrue(valid_heights(samples, 27))
            self.assertFalse(valid_heights(samples, 26))
            self.assertEqual(_profile(dict(schema_version=27, assets=[dict(tile_hash='a', height=samples)]))[0]['a'], samples)
            with self.assertRaises(ValueError):
                _profile(dict(schema_version=26, assets=[dict(tile_hash='a', height=samples)]))
        for value in (-32769, 32768, True, 1.0):
            self.assertFalse(valid_heights([value]*64))
        self.assertTrue(valid_heights([255]*64, 1))

    def test_digest_does_not_alias_low_byte(self):
        self.assertEqual(height_bytes([-1]*64)[:2], b'\xff\xff')
        self.assertEqual(len({height_bytes([v]*64) for v in (0, 256, -256)}), 3)

    def test_authoring_keeps_other_layers_unsigned(self):
        for value in (-32768, 32767):
            _format_layer('height', (value,)*64)
            validate_layers(dict(height=[value]*64))
        for name, size in (('normal_xyz',192), ('occlusion',64), ('emission_rgba',256)):
            with self.assertRaises(ProfileGenerationError):
                _format_layer(name, (-1,)*size)
        with self.assertRaises(ValueError):
            validate_layers(dict(occlusion=[256]*64))
        result = _render_profile('schema_version = 26\n', {}, {'a': {'height':(-1,)*64}})
        self.assertIn('schema_version = 27', result)


if __name__ == '__main__':
    unittest.main()
