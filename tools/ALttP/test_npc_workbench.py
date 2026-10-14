import copy
import json
from pathlib import Path
import tempfile
import tomllib
import unittest
import subprocess
import re
import shutil
import socket
import sys
import time
import urllib.request
import urllib.error
import sqlite3

from alttp_atlas import SUPPORTED_US_SHA256
from alttp_atlas_graphics import tile_content_id
from alttp_npc_workbench import merge_profile, prepare_pose, silhouette_defaults, validate_layers, contexts
from alttp_npc_catalog import CATALOG
from alttp_npc_workbench_support import special_overworld_contexts


class NpcWorkbenchTests(unittest.TestCase):
    def test_special_screen_is_mapped_through_its_exit_not_screen_ordinal(self):
        with tempfile.TemporaryDirectory() as tmp:
            engine = Path(tmp)
            (engine / 'src').mkdir()
            (engine / 'src/overworld.c').write_text('''
              kSpExit_SprGfx[16] = {12,12,14,14,14,16,16,16,14,14,14,14,16,16,16,16};
              kSpExit_PalSpr[16] = {1,8,8,8,0,0,0,0,0,0,8,0,0,0,0,2};
              kSpecialSwitchAreaB_Screen[3] = {0x80,0x80,0x81};
              kSpecialSwitchArea_Exit[4] = {0x180,0x181,0x182,0x189};
            ''')
            self.assertEqual(special_overworld_contexts(engine)[0x81], (14, 8))
        link = next(entry for entry in CATALOG if entry['key'] == 'link')
        self.assertEqual((link['draw'], link['directions'], link['steps']), ('link', 4, 6))
        self.assertTrue(link['explicit_contexts'])
        ganon = next(entry for entry in CATALOG if entry['key'] == 'ganon')
        self.assertEqual((ganon['type_id'], ganon['draw'], ganon['steps']), (0xd6, 'ganon', 17))

    def test_character_catalog_and_context_filters(self):
        self.assertGreaterEqual(len({entry['name'] for entry in CATALOG}), 50)
        self.assertEqual(len({entry['key'] for entry in CATALOG}), len(CATALOG))
        with tempfile.TemporaryDirectory() as tmp:
            atlas = Path(tmp) / 'atlas.sqlite'
            with sqlite3.connect(atlas) as db:
                db.execute('CREATE TABLE world_record(kind TEXT, data_json TEXT)')
                records = [('dungeon_rooms', dict(room_id=0x103, enemy_blockset=15)),
                           ('dungeon_rooms', dict(room_id=0x119, enemy_blockset=15)),
                           ('dungeon_sprites', dict(type_id=0x29, room_id=0x103)),
                           ('dungeon_sprites', dict(type_id=0x29, room_id=0x119)),
                           ('overworld_sprites', dict(type_id=0x52, area_id=0x81, gfx_index=None, palette_index=None, stage='default'))]
                db.executemany('INSERT INTO world_record VALUES (?,?)', [(kind, json.dumps(record)) for kind,record in records])
            father = next(entry for entry in CATALOG if entry['key'] == 'father')
            result = contexts(atlas, father)
            self.assertEqual(len(result[0x29]), 1)
            self.assertIn('103', next(iter(result[0x29]))[3])
            zora = next(entry for entry in CATALOG if entry['key'] == 'zora')
            special = contexts(atlas, zora, {0x81: (12, 8)})
            self.assertEqual(next(iter(special[0x52]))[:3], (12, 8, 0))

    def test_dungeon_enemy_palettes_and_spawned_contexts(self):
        models = {entry['key']: entry for entry in CATALOG}
        for key in ('keese', 'rat', 'rope', 'armos-knight', 'lanmola',
                    'moldorm-boss', 'helmasaur-king', 'mothula', 'blind-boss',
                    'arrghus', 'kholdstare', 'vitreous'):
            self.assertTrue(models[key]['dungeon_palette'])
        with tempfile.TemporaryDirectory() as tmp:
            atlas = Path(tmp) / 'atlas.sqlite'
            with sqlite3.connect(atlas) as db:
                db.execute('CREATE TABLE world_record(kind TEXT, data_json TEXT)')
                records = [('dungeon_rooms', dict(room_id=2, enemy_blockset=6, palette=10)),
                           ('dungeon_rooms', dict(room_id=3, enemy_blockset=6, palette=11)),
                           ('dungeon_sprites', dict(type_id=0x6d, room_id=2)),
                           ('dungeon_sprites', dict(type_id=0x6d, room_id=3)),
                           ('dungeon_sprites', dict(type_id=0x109, record_kind='overlord', room_id=2))]
                db.executemany('INSERT INTO world_record VALUES (?,?)',
                               [(kind, json.dumps(record)) for kind, record in records])
            rats = contexts(atlas, models['rat'])[0x6d]
            self.assertEqual({entry[:3] for entry in rats}, {(0x46, 30, 0), (0x46, 31, 0)})
            hands = contexts(atlas, models['wallmaster'])[0x90]
            self.assertEqual(next(iter(hands))[:3], (0x46, 30, 0))

    def part(self, **kwargs):
        return dict(x=0, y=0, slot=0, hflip=False, vflip=False,
                    indices=[1]*64, colors=[32767]*64, **kwargs)

    def test_flip_transforms_normals_and_preserves_canonical_height(self):
        part = self.part()
        pose = prepare_pose(dict(parts=[part], direction=0))
        layers = silhouette_defaults(pose)[part['tile_hash']]['layers']
        mirrored = copy.deepcopy(pose)
        mirrored['parts'][0]['hflip'] = True
        other = silhouette_defaults(mirrored)[part['tile_hash']]['layers']
        self.assertEqual(layers, other)
        self.assertEqual(layers['height'][:8], [13]*8)
        self.assertEqual(layers['height'][-8:], [6]*8)
        self.assertLess(layers['normal_xyz'][0], 128)
        self.assertGreater(layers['normal_xyz'][21], 128)

    def test_transparency_and_shared_hash_conflict(self):
        left, right = self.part(), self.part()
        right['x'] = 8
        pose = prepare_pose(dict(parts=[left, right], direction=0))
        result = silhouette_defaults(pose)
        self.assertTrue(result[left['tile_hash']]['conflict'])
        left['indices'][0] = 0
        pose = prepare_pose(dict(parts=[left], direction=0))
        layers = silhouette_defaults(pose)[left['tile_hash']]['layers']
        self.assertEqual(layers['height'][0], 0)
        self.assertEqual(layers['occlusion'][0], 0)

    def test_profile_export_preserves_authored_layers_and_interleaved_tables(self):
        key = tile_content_id(bytes([1]*64), 4)
        source = (f'schema_version = 22\n[game]\nrom_sha256 = "{SUPPORTED_US_SHA256}"\n'
                  '[materials.body]\nsurface_class = "prop"\n'
                  f'[[assets]]\ntile_hash = "{key}"\nheight = {json.dumps([42]*64)}\n'
                  'height_sampling = "linear"\n[asset_groups.body]\ntile_hashes = []\n'
                  '[[rules]]\nsource = "OBJ"\nmaterial = "body"\n')
        normal = [128,128,255]*64
        edits = {key: dict(height=[13]*64, normal_xyz=normal)}
        exported, count = merge_profile(source, edits)
        self.assertEqual(count, 1)
        before, after = tomllib.loads(source), tomllib.loads(exported)
        self.assertEqual(after['assets'][0]['height'], [42]*64)
        self.assertEqual(after['assets'][0]['height_sampling'], 'linear')
        self.assertEqual(after['assets'][0]['normal_xyz'], normal)
        self.assertEqual(before['rules'], after['rules'][:len(before['rules'])])
        self.assertEqual(before['asset_groups']['body'], after['asset_groups']['body'])
        self.assertEqual(merge_profile(exported, edits), (exported, 0))

    def test_schema_upgrade_and_reject_wrong_rom_or_bad_data(self):
        source = f'schema_version = 1\n[game]\nrom_sha256 = "{SUPPORTED_US_SHA256}"\n'
        key = tile_content_id(bytes([1]*64), 4)
        exported, _ = merge_profile(source, {key: dict(normal_xyz=[128,128,255]*64)})
        self.assertEqual(tomllib.loads(exported)['schema_version'], 5)
        with self.assertRaises(ValueError):
            merge_profile(source.replace(SUPPORTED_US_SHA256, '0'*64), {})
        for data in ([1]*63, [256]*64, [True]*64):
            with self.assertRaises(ValueError):
                validate_layers(dict(height=data))
        with self.assertRaises(ValueError):
            validate_layers(dict(normal_xyz=[128]*192))

    @unittest.skipUnless(shutil.which('node'), 'Node is needed to check browser JavaScript')
    def test_browser_script_parses(self):
        page = Path(__file__).with_name('alttp_npc_workbench.html').read_text()
        script = re.search(r'<script>(.*?)</script>', page, re.S)[1]
        subprocess.run(['node', '--check'], input=script, text=True, check=True)

    def test_http_suggestions_save_and_stale_revision(self):
        pose = prepare_pose(dict(parts=[self.part()], direction=0))
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            (output/'poses.json').write_text(json.dumps(dict(actors=[dict(id='test', poses=[pose])],
                                                            background_shared_hashes=[])))
            with socket.socket() as sock:
                sock.bind(('127.0.0.1', 0))
                port = sock.getsockname()[1]
            process = subprocess.Popen([sys.executable, '-B', str(Path(__file__).with_name('alttp_npc_workbench.py')),
                                        '--serve', '--output', str(output), '--port', str(port)],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            base = f'http://127.0.0.1:{port}'
            try:
                for _ in range(100):
                    try:
                        with urllib.request.urlopen(base+'/api/state') as response:
                            state = json.load(response)
                        break
                    except urllib.error.URLError:
                        time.sleep(0.03)
                else:
                    self.fail('workbench server did not start')
                def post(path, body):
                    request = urllib.request.Request(base+path, data=json.dumps(body).encode(),
                                                     headers={'Content-Type': 'application/json'})
                    with urllib.request.urlopen(request) as response:
                        return json.load(response)
                suggested = post('/api/suggest', dict(revision=0, actor='test', pose=0))
                key = pose['parts'][0]['tile_hash']
                self.assertIn('normal_xyz', suggested['suggestions'][key])
                self.assertEqual(post('/api/save', dict(revision=0, edits=suggested['suggestions']))['revision'], 1)
                self.assertEqual(json.loads((output/'edits.json').read_text()), suggested['suggestions'])
                with self.assertRaises(urllib.error.HTTPError) as caught:
                    post('/api/save', dict(revision=0, edits={}))
                self.assertEqual(caught.exception.code, 400)
                caught.exception.close()
                self.assertEqual(state['revision'], 0)
            finally:
                process.terminate()
                process.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()
