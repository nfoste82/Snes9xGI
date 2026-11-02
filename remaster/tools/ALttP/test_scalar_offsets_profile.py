import unittest
import tomllib
from alttp_atlas_profile import _render_profile


class ScalarProfilePreservation(unittest.TestCase):
    def test_compiler_preserves_interleaved_scalar_sections(self):
        source = '''schema_version = 37
[game]
title = "test"
[[assets]]
tile_hash = "v1:4bpp:0000000000000001"
height = [0]
[overworld_placement]
mode = "authored_overworld_offsets"
[[overworld_offsets]]
world = "light"
background = 1
x = 192
y = 192
offset = -40
[[overworld_offsets]]
world = "dark"
background = 1
x = 192
y = 192
offset = 80
'''
        parsed = tomllib.loads(source)
        authored = {a['tile_hash']: a for a in parsed['assets']}
        result = tomllib.loads(_render_profile(source, authored, {}))
        self.assertEqual(parsed, result)


if __name__ == '__main__':
    unittest.main()
