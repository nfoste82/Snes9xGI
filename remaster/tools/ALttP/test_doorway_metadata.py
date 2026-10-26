import unittest
from alttp_doorway_metadata import migrate_profile, validate_asset, transformed_direction


class DoorwayMetadataTests(unittest.TestCase):
    def test_version_boundary(self):
        old = dict(schema_version=33, assets=[dict(tile_role='door', doorframe_mask='0000000000000000', height=[-8]*64)])
        self.assertEqual(migrate_profile(old)['assets'][0]['tile_role'], 'doorway')
        self.assertEqual(old['assets'][0]['tile_role'], 'door')
        self.assertEqual(migrate_profile(dict(schema_version=34, assets=[dict(tile_role='door')]))['assets'][0]['tile_role'], 'door')

    def test_strict_support(self):
        for direction in ('north', 'south', 'east', 'west'):
            validate_asset(dict(tile_role='doorway', doorway_support=True, doorway_support_direction=direction))
        for asset in (dict(tile_role='doorway', doorway_support=True), dict(tile_role='door', doorway_support=True, doorway_support_direction='north'),
                      dict(tile_role='doorway', doorway_support=False, doorway_support_direction='north'), dict(tile_role='doorway', doorway_support=1)):
            with self.assertRaises(ValueError):
                validate_asset(asset)

    def test_flips(self):
        self.assertEqual(transformed_direction('north', False, True), 'south')
        self.assertEqual(transformed_direction('east', True, False), 'west')


if __name__ == '__main__':
    unittest.main()
