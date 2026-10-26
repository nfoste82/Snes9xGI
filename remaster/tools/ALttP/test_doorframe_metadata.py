"""Doorframe authoring compatibility: stairs survive and unknown is not empty."""
import json
from pathlib import Path
import sqlite3
import tempfile
import tomllib
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from alttp_tile_semantic_atlas import AnnotationStore, AtlasError, validate_annotation
from alttp_atlas_profile import generate_profile

ROM = 'a' * 64
KEY = 'v1:4bpp:000000000000007b'


class DoorframeTests(unittest.TestCase):
    def test_strict_optional_mask(self):
        self.assertNotIn('doorframe_mask', validate_annotation({'role': 'doorway'}))
        self.assertEqual(validate_annotation({'role': 'doorway', 'doorframe_mask': '0' * 16})['doorframe_mask'], '0' * 16)
        for bad in (0, False, [], [False] * 64, '0', 'f' * 17, 'F' * 16, 'g' * 16):
            with self.assertRaises(AtlasError):
                validate_annotation({'role': 'doorway', 'doorframe_mask': bad})
        with self.assertRaises(AtlasError):
            validate_annotation({'role': 'floor', 'doorframe_mask': '0' * 16})

    def test_profile_refresh_html_save_and_role_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            profile = root / 'profile.toml'
            marker = root / 'marker.json'
            annotations = root / 'semantics.json'
            stairs = [y for y in range(8) for x in range(8)]
            profile.write_text(f'schema_version = 21\n[game]\nrom_sha256 = "{ROM}"\n'
                               f'[[assets]]\ntile_hash = "{KEY}"\ntile_role = "door"\n'
                               'doorframe_mask = "8000000000020001"\nheight = ' + json.dumps(stairs) + '\n')
            marker.write_text(json.dumps({'profile': str(profile)}))
            store = AnnotationStore(annotations, ROM, marker)
            store.refresh()
            annotation = store.tiles[KEY]
            self.assertEqual(annotation['doorframe_mask'], '8000000000020001')
            store.save(KEY, {**annotation, 'note': 'HTML note edit'})
            parsed = tomllib.loads(profile.read_text())
            self.assertEqual(parsed['assets'][0]['height'], stairs)
            self.assertEqual(parsed['assets'][0]['doorframe_mask'], annotation['doorframe_mask'])
            store.refresh()
            self.assertEqual(store.tiles[KEY]['note'], 'HTML note edit')
            store.save(KEY, {'role': 'doorway', 'doorframe_mask': '0' * 16})
            self.assertEqual(tomllib.loads(profile.read_text())['assets'][0]['doorframe_mask'], '0' * 16)
            store.save(KEY, {'role': 'doorway'})
            self.assertEqual(tomllib.loads(profile.read_text())['assets'][0]['doorframe_mask'], '0' * 16)
            store.save(KEY, {'role': 'other'})
            self.assertNotIn('doorframe_mask', tomllib.loads(profile.read_text())['assets'][0])
            self.assertEqual(tomllib.loads(profile.read_text())['assets'][0]['height'], stairs)

    def test_compiler_preserves_native_door_geometry_and_sidecar_mask(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); db = root / 'atlas.sqlite'; source = root / 'profile.toml'; out = root / 'compiled.toml'
            con = sqlite3.connect(db)
            con.executescript('CREATE TABLE run_info(key TEXT,value TEXT); CREATE TABLE decoded_tile(content_id TEXT,indices BLOB); CREATE TABLE graphics_source(content_id TEXT,source_kind TEXT,pack_index INTEGER);')
            con.execute('INSERT INTO run_info VALUES(?,?)', ('rom_sha256', ROM))
            con.execute('INSERT INTO decoded_tile VALUES(?,?)', (KEY, bytes(64)))
            con.execute('INSERT INTO graphics_source VALUES(?,?,?)', (KEY, 'background', 1))
            con.commit(); con.close()
            stairs = [y for y in range(8) for x in range(8)]
            base = (f'schema_version = 20\n[game]\nrom_sha256 = "{ROM}"\n'
                    '[lighting_space]\nupper_floor_height = 50\n'
                    f'[asset_groups.dungeon_floor]\ntile_hashes = ["{KEY}"]\n'
                    '[asset_groups.wall_faces]\ntile_hashes = []\n'
                    f'[[assets]]\ntile_hash = "{KEY}"\nheight = {json.dumps(stairs)}\n'
                    'normal_xyz = ' + json.dumps([128, 128, 255] * 64) + '\n')
            annotation = {'role': 'doorway', 'normals': [], 'context_dependent': False, 'doorframe_mask': '8000000000020001'}
            (root / 'semantics.json').write_text(json.dumps({'rom_sha256': ROM, 'tiles': {KEY: annotation}}))
            with patch('alttp_atlas_profile.AnnotationStore', return_value=SimpleNamespace(tiles={KEY: annotation})), patch(
                    'alttp_atlas_profile.TileCatalog', return_value=SimpleNamespace(semantic_group=lambda key: [key])):
                for native in (False, True):
                    text = base.replace('schema_version = 20', 'schema_version = 21') if native else base
                    if native:
                        text += 'tile_role = "door"\ndoorframe_mask = "0000000000000000"\n'
                    source.write_text(text)
                    generate_profile(db, source, out, root / 'semantics.json')
                    result = tomllib.loads(out.read_text())
                    self.assertEqual(result['schema_version'], 34)
                    self.assertEqual(result['assets'][0]['tile_role'], 'doorway')
                    self.assertEqual(result['assets'][0]['height'], stairs)
                    self.assertEqual(result['assets'][0]['doorframe_mask'], '0' * 16 if native else annotation['doorframe_mask'])
                    self.assertEqual(source.read_text(), text)


if __name__ == '__main__':
    unittest.main()
