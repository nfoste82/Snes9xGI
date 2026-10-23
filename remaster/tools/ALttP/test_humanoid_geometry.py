"""Portable humanoid geometry/preservation regression tests; no ROM required."""
import json
from pathlib import Path
import tempfile
import tomllib
import unittest

from alttp_atlas import SUPPORTED_US_SHA256
from alttp_humanoid_geometry import generate, model_pixel, promote_link, segment


class HumanoidGeometryTests(unittest.TestCase):
    def part(self, h='v1:4bpp:1111111111111111', vflip=False):
        return dict(tile_hash=h, address=20480, x=0, y=0, slot=0,
                    hflip=False, vflip=vflip, indices=[6]*64, colors=[0x3d8f]*64)

    def actor(self, part, key='running', name='Running Man'):
        return dict(name=name, id=key, draw_model=dict(key=key), graphics_inherited=False,
                    poses=[dict(parts=[part], facing='south', action='modeled draw frames', step=0)])

    def text(self, part=None, fields=None):
        text = f'schema_version = 29\n[game]\nrom_sha256 = "{SUPPORTED_US_SHA256}"\n'
        if part:
            text += f'[[assets]]\ntile_hash = "{part["tile_hash"]}"\n'
            text += ''.join(f'{k} = {json.dumps(v)}\n' for k, v in (fields or {}).items())
        return text

    def manifest(self, actors, bg=()):
        return dict(actors=actors, background_shared_hashes=list(bg))

    def test_head_and_boot_are_not_same_cylinder(self):
        actor = self.actor(self.part())
        scene = segment(actor, actor['poses'][0])
        head = next(p for p in scene['occurrences'] if p['x']==3 and p['y']==0)
        boot = next(p for p in scene['occurrences'] if p['x']==3 and p['y']==7)
        self.assertEqual(head['zone'], 'head')
        self.assertEqual(boot['zone'], 'boot')
        self.assertGreater(model_pixel(scene, head)[0][2], 0.8)
        self.assertGreater(model_pixel(scene, head)[1], model_pixel(scene, boot)[1])

    def test_preserve_detailed_geometry_and_other_fields(self):
        p = self.part()
        fields = dict(height=[7]*64, normal_xyz=[128, 128, 255]*64,
                      normal_ignore_tile_transform=True)
        text = self.text(p, fields)
        result, report, _, assets = generate(self.manifest([self.actor(p)]), text)
        self.assertEqual(report['changed_tiles'], 0)
        self.assertEqual(assets[p['tile_hash']], dict(tile_hash=p['tile_hash'], **fields))
        self.assertEqual(result, text)

    def test_replace_only_confirmed_placeholder(self):
        p = self.part()
        text = self.text(p, dict(height=[3]*64, normal_xyz=[128, 255, 128]*64))
        _, report, _, assets = generate(self.manifest([self.actor(p)]), text)
        self.assertEqual(report['replaced_placeholders'], 1)
        self.assertEqual(assets[p['tile_hash']]['height'], [3]*64)
        self.assertNotEqual(assets[p['tile_hash']]['normal_xyz'], [128, 255, 128]*64)

    def test_background_and_foreign_shared_hashes_blocked(self):
        p = self.part()
        other = self.actor(p, 'link', 'Link')
        text = self.text()
        result, report, _, _ = generate(self.manifest([self.actor(p), other]), text)
        self.assertEqual(result, text)
        self.assertIn(p['tile_hash'], report['blocked_tiles'])
        result, report, _, _ = generate(self.manifest([self.actor(p)], [p['tile_hash']]), text)
        self.assertEqual(result, text)

    def test_variants_and_inherited_contexts_preserved(self):
        p = self.part()
        text = self.text(p, dict(geometry_by_flip=True, flip_vert_height=[5]*64))
        result, report, _, _ = generate(self.manifest([self.actor(p)]), text)
        self.assertEqual(result, text)
        self.assertIn(p['tile_hash'], report['blocked_tiles'])
        actor = self.actor(p)
        actor['graphics_inherited'] = True
        result, report, _, _ = generate(self.manifest([actor]), self.text())
        self.assertEqual(report['changed_tiles'], 0)
        self.assertEqual(report['inherited_contexts'], ['running'])

    def test_flip_vectors_and_idempotence(self):
        p = self.part(vflip=True)
        manifest = self.manifest([self.actor(p)])
        result, _, _, assets = generate(manifest, self.text())
        a = assets[p['tile_hash']]
        self.assertTrue(a['geometry_by_flip'])
        self.assertIn('flip_vert_normal_xyz', a)
        self.assertGreater(a['flip_vert_normal_xyz'][1], 128)
        again, report, _, _ = generate(manifest, result)
        self.assertEqual(again, result)
        self.assertEqual(report['changed_tiles'], 0)

    def test_promotion_detects_newer_authored_edit_before_writing(self):
        p = self.part()
        base = self.text(p, dict(height=[1]*64))
        approved = self.text(p, dict(height=[2]*64))
        current = self.text(p, dict(height=[3]*64))
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path/'source-profile.toml').write_text(base)
            (path/'link-profile.toml').write_text(approved)
            profile = path/'main.toml'
            profile.write_text(current)
            with self.assertRaisesRegex(ValueError, 'newer edits'):
                promote_link(profile, path, path/'unused-validator')
            self.assertEqual(profile.read_text(), current)


if __name__ == '__main__':
    unittest.main()
