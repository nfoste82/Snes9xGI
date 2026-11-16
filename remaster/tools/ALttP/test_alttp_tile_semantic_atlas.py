#!/usr/bin/env python3
"""Portable checks for the ALTTP tile semantic atlas."""

from __future__ import annotations

import json
import hashlib
from pathlib import Path
import sqlite3
import shutil
import subprocess
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock
import tomllib
from http.server import ThreadingHTTPServer
import threading
import urllib.request
import urllib.error

from alttp_tile_semantic_atlas import (AnnotationStore, AtlasError, ROOM_INDEX_FORMAT,
                                       RoomOccurrenceIndex, TileCatalog,
                                       validate_annotation, make_handler, PAGE)
from alttp_tile_semantic_world import WorldCache, WORLD_FORMAT, CELL


ROM_SHA256 = "1" * 64


def make_database(path: Path) -> None:
    connection = sqlite3.connect(path)
    connection.executescript("""
        CREATE TABLE run_info (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE decoded_tile (content_id TEXT PRIMARY KEY, bit_depth INTEGER, indices BLOB);
        CREATE TABLE graphics_source (source_kind TEXT, pack_index INTEGER, tile_index INTEGER,
          conversion TEXT, snes_address INTEGER, content_id TEXT);
        CREATE TABLE overworld_base_tile_usage (quadrant_index INTEGER, x INTEGER, y INTEGER,
          area_id INTEGER, tile_word INTEGER, vram_slot INTEGER, pack_index INTEGER,
          pack_tile_index INTEGER, conversion TEXT, content_id TEXT, status TEXT);
    """)
    connection.executemany("INSERT INTO run_info VALUES (?, ?)",
                           [("atlas_schema_version", "1"), ("rom_sha256", ROM_SHA256)])
    connection.executemany("INSERT INTO decoded_tile VALUES (?, ?, ?)",
                           [("bg", 4, bytes(range(16)) * 4),
                            ("sprite", 4, bytes(64))])
    connection.executemany("INSERT INTO graphics_source VALUES (?, ?, ?, ?, ?, ?)", [
        ("background", 1, 2, "low", 0x1000, "bg"),
        ("background", 2, 3, "high", 0x2000, "bg"),
        ("sprite", 4, 5, "native", 0x3000, "bg"),
        ("sprite", 6, 7, "native", 0x4000, "sprite")])
    connection.execute("INSERT INTO overworld_base_tile_usage VALUES "
                       "(1, 2, 3, 4, 5, 6, 1, 2, 'low', 'bg', 'resolved')")
    connection.commit()
    connection.close()


class TileSemanticAtlasTests(unittest.TestCase):
    def test_distinct_metadata_batch_saves_profile_and_sidecar_once(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root/'profile.toml'
            profile.write_text(f'schema_version = 39\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                               '[[assets]]\ntile_hash = "first"\nheight = [7]\n')
            store = AnnotationStore(root/'annotations.json', ROM_SHA256, profile=profile)
            revision = store.refresh()
            values = {key: dict(role='floor', normals=[], note=key, context_dependent=False)
                      for key in ('first', 'second', 'third')}
            before = profile.read_bytes()
            with self.assertRaises(AtlasError):
                store.save_batch(dict(values, third=dict(role='wall', normals=[])), revision)
            self.assertEqual(profile.read_bytes(), before)
            with mock.patch.object(store, '_save_profile_batch', wraps=store._save_profile_batch) as save_profile, \
                 mock.patch.object(store, '_write', wraps=store._write) as write:
                store.save_batch(values, revision)
                self.assertEqual(save_profile.call_count, 1)
                self.assertEqual(write.call_count, 1)
            rows = tomllib.loads(profile.read_text())['assets']
            self.assertEqual(rows[0]['height'], [7])
            self.assertEqual({r['tile_hash']: r['tile_semantic_note'] for r in rows},
                             {key: key for key in values})
            before = profile.read_bytes()
            with self.assertRaises(AtlasError):
                store.save_batch(values, revision)
            self.assertEqual(profile.read_bytes(), before)

    def test_world_cache_animated_selection_matches_rendered_phase(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest('world cache building requires Pillow')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            connection = sqlite3.connect(database)
            # The reported Light World cell and a flipped Dark World copy.
            connection.execute("INSERT INTO decoded_tile VALUES ('animated', 4, ?)", (bytes(64),))
            connection.execute("INSERT INTO graphics_source VALUES ('background', 90, 27, 'low', 8192, 'animated')")
            for qi in (56, 120):
                connection.execute("INSERT INTO overworld_base_tile_usage VALUES "
                                   "(?, 14, 58, 48, 5595, 7, NULL, NULL, NULL, NULL, 'animated_vram_slot')", (qi,))
            connection.execute("INSERT INTO overworld_base_tile_usage VALUES "
                               "(56, 15, 58, 48, 0, 7, NULL, NULL, NULL, NULL, 'unresolved')")
            connection.commit()
            connection.close()
            catalog = TileCatalog(database)
            def tile(x, y, flipped=False):
                return SimpleNamespace(x=x, y=y, map16_id=226, palette=0,
                                       flip_x=flipped, flip_y=flipped)
            quadrants = [SimpleNamespace(iter_tiles=lambda: []) for _ in range(128)]
            for qi in (1, 56, 120):
                tiles = [tile(2, 3)] if qi == 1 else [tile(14, 58, qi == 120), tile(15, 58)]
                quadrants[qi] = SimpleNamespace(iter_tiles=lambda tiles=tiles: iter(tiles))
            resolver = mock.Mock()
            resolver.resolve_tile.side_effect = lambda area, t: SimpleNamespace(
                status='animated_vram_slot' if t.x == 14 else 'resolved',
                content_id=None if t.x in (14, 15) else 'bg')
            resolver.resolve_animated_phase.return_value = SimpleNamespace(content_id='animated', status='resolved_animated_phase')
            with mock.patch('alttp_atlas.Rom', return_value=SimpleNamespace(sha256=ROM_SHA256, get_byte=lambda at: 0)), \
                 mock.patch('alttp_atlas_graphics.extract_graphics', return_value=[SimpleNamespace(content_id=key, indices=bytes(64)) for key in ('bg', 'animated')]), \
                 mock.patch('alttp_atlas_overworld.extract_overworld', return_value=quadrants), \
                 mock.patch('alttp_atlas_world.catalog_world', return_value={'overworld_areas': []}), \
                 mock.patch('alttp_atlas_overworld_gfx.BaseOverworldGraphics', return_value=resolver), \
                 mock.patch('alttp_visual_context.rom_palette', return_value=([0] * 16,)), \
                 mock.patch('alttp_visual_context.tile_stamp', return_value=Image.new('RGB', (8, 8), 'green')):
                cache = WorldCache(root / 'world', catalog, root / 'mock-rom')
            self.assertTrue(cache.valid())
            self.assertEqual(resolver.resolve_animated_phase.call_count, 2)
            for call in resolver.resolve_animated_phase.call_args_list:
                self.assertEqual(call.args[1], 0)
            for world, flipped in (('light', False), ('dark', True)):
                cell = cache.cell(world, 14, 506)
                self.assertEqual(cell['content_id'], 'animated')
                self.assertEqual(cell['catalog_content_id'], 'animated')
                self.assertEqual((cell['map16_id'], cell['area_id']), (226, 48))
                self.assertEqual((cell['flip_x'], cell['flip_y']), (flipped, flipped))
                with Image.open(cache.path / f'{world}.png') as image:
                    self.assertEqual(image.getpixel((14 * 8, 506 * 8)), (0, 128, 0))
            self.assertEqual(cache.manifest['counts']['light']['bg'], 1)
            self.assertEqual(cache.manifest['counts']['light']['animated'], 1)
            self.assertEqual(cache.manifest['counts']['dark']['animated'], 1)
            self.assertIsNone(cache.cell('light', 15, 506)['content_id'])
            # Old caches must rebuild even if all file hashes still match.
            cache.manifest['format'] = 'alttp-tile-semantic-world-v1'
            (cache.path / 'manifest.json').write_text(json.dumps(cache.manifest))
            self.assertFalse(cache.valid())

    def test_floor_water_validation_and_persistence(self):
        self.assertFalse(validate_annotation(dict(role='floor'))['water'])
        for value in (1, 'true', None):
            with self.assertRaises(AtlasError):
                validate_annotation(dict(role='floor', water=value))
        with self.assertRaises(AtlasError):
            validate_annotation(dict(role='wall', normals=['north'], water=True))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / 'profile.toml'
            profile.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                               '[[assets]]\ntile_hash = "a"\nheight = [7]\n')
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=profile)
            for water in (True, False):
                store.save_group(['a', 'b'], dict(role='floor', water=water), store.refresh())
                reloaded = AnnotationStore(store.path, ROM_SHA256, profile=profile)
                reloaded.refresh()
                for key in ('a', 'b'):
                    self.assertEqual(reloaded.tiles[key]['water'], water)
                assets = tomllib.loads(profile.read_text())['assets']
                self.assertEqual(assets[0]['height'], [7])
                self.assertTrue(all(a['tile_semantic_water'] == water for a in assets))
            store.save_group(['a'], dict(role='other'), store.refresh())
            self.assertNotIn('tile_semantic_water', tomllib.loads(profile.read_text())['assets'][0])

    def test_legacy_door_facings_retained_but_changes_require_cardinal(self):
        for role in ('door', 'doorway'):
            self.assertEqual(validate_annotation(dict(role=role), allow_incomplete=True)['normals'], [])
            for normals in ([], ['north_east'], ['north', 'south']):
                with self.assertRaises(AtlasError):
                    validate_annotation(dict(role=role, normals=normals))
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                profile = root / 'profile.toml'
                profile.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                                   f'[[assets]]\ntile_hash = "old"\ntile_role = "{role}"\nheight = [7]\n')
                store = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=profile)
                revision = store.refresh()
                self.assertEqual(store.tiles['old']['role'], role)
                self.assertEqual(store.tiles['old']['normals'], [])
                before = profile.read_bytes()
                with self.assertRaises(AtlasError):
                    store.save_group(['old'], dict(role=role, note='changed'), revision)
                self.assertEqual(profile.read_bytes(), before)
                store.save_group(['other'], dict(role='floor'), store.refresh())
                reloaded = AnnotationStore(store.path, ROM_SHA256, profile=profile)
                reloaded.refresh()
                self.assertEqual(reloaded.tiles['old']['role'], role)
                self.assertEqual(reloaded.tiles['old']['normals'], [])
                reloaded.save_group(['old'], dict(role=role, normals=['west']), reloaded.refresh())
                self.assertEqual(tomllib.loads(profile.read_text())['assets'][0]['height'], [7])

    def test_profile_save_parses_once_and_reuses_validated_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / 'profile.toml'
            profile.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                               '[[assets]]\ntile_hash = "bg"\nheight = [1, 2, 3]\n')
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=profile)
            store.refresh()
            with mock.patch('alttp_tile_semantic_atlas.tomllib.loads', wraps=tomllib.loads) as loads:
                revision = store.refresh()
                self.assertEqual(loads.call_count, 0)
                store.save_group(['bg'], dict(role='wall', normals=['north']), revision)
                self.assertEqual(loads.call_count, 1)
                store.refresh()
                store.refresh()
                self.assertEqual(loads.call_count, 1, 'local saves must not trigger a full reparse on refresh')
                self.assertEqual(store.tiles['bg']['normals'], ['north'])
                self.assertEqual(store._profile_document['assets'][0]['height'], [1, 2, 3])
                # A second save compares against the latest validated document.
                store.save_group(['bg'], dict(role='other'), store.revision())
                store.refresh()
                self.assertEqual(loads.call_count, 2)
                # External geometry edits invalidate the cache and are retained.
                profile.write_text(profile.read_text().replace('[1, 2, 3]', '[4, 5, 6]'))
                store.refresh()
                self.assertEqual(loads.call_count, 3)
                store.save_group(['bg'], dict(role='floor'), store.revision())
                self.assertEqual(loads.call_count, 4)
            self.assertEqual(tomllib.loads(profile.read_text())['assets'][0]['height'], [4, 5, 6])

    def test_profile_save_rechecks_external_edits_after_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / 'profile.toml'
            original = f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
            profile.write_text(original)
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=profile)
            revision = store.refresh()
            snapshot = store._profile_document
            parse = tomllib.loads
            external = original + '\n# concurrent external edit\n'
            def concurrent_edit(text):
                result = parse(text)
                profile.write_text(external)
                return result
            with mock.patch('alttp_tile_semantic_atlas.tomllib.loads', side_effect=concurrent_edit):
                with self.assertRaisesRegex(AtlasError, 'profile changed'):
                    store.save_group(['bg'], dict(role='floor'), revision)
            self.assertEqual(profile.read_text(), external)
            self.assertIs(store._profile_document, snapshot)
            self.assertEqual(store.profile_bytes, original.encode())
            self.assertFalse(profile.with_name(profile.name + '.semantics.tmp').exists())
            self.assertNotIn('bg', store.tiles)

    def test_world_cache_endpoints_aliases_and_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            connection = sqlite3.connect(database)
            connection.execute("UPDATE graphics_source SET conversion='low' WHERE source_kind='background'")
            connection.execute("INSERT INTO decoded_tile VALUES ('high', 4, ?)", (bytes(64),))
            connection.execute("INSERT INTO graphics_source VALUES ('background', 1, 2, 'high', 4096, 'high')")
            connection.commit()
            connection.close()
            catalog = TileCatalog(database)
            cache_path = root / 'world'
            cache_path.mkdir()
            raw = bytearray(CELL.size * 512 * 512)
            CELL.pack_into(raw, (497 * 512 + 9) * CELL.size, 1, 42, 56, 0)
            CELL.pack_into(raw, 0, 2, 43, 0, 3)
            files = {}
            for world in ('light', 'dark'):
                for suffix, data in (('bin', raw), ('png', b'fixture-image')):
                    name = f'{world}.{suffix}'
                    (cache_path / name).write_bytes(data)
                    files[name] = hashlib.sha256(data).hexdigest()
            manifest = dict(format=WORLD_FORMAT, rom_sha256=ROM_SHA256,
                            atlas_sha256=hashlib.sha256(database.read_bytes()).hexdigest(),
                            files=files, content_ids=[None, 'bg', 'not-in-catalog'],
                            counts={w: {'bg': 2, 'high': 3} for w in ('light', 'dark')})
            (cache_path / 'manifest.json').write_text(json.dumps(manifest))
            cache = WorldCache(cache_path, catalog)
            self.assertEqual(cache.cell('light', 9, 497)['catalog_content_id'], 'high')
            self.assertEqual(cache.cell('light', 9, 497)['placement_count'], 5)
            self.assertIsNone(cache.cell('dark', 0, 0)['catalog_content_id'])
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256)
            store.save('high', dict(role='wall', normals=['north']))
            server = ThreadingHTTPServer(('127.0.0.1', 0), make_handler(catalog, store, None, root, cache))
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            url = f'http://127.0.0.1:{server.server_port}'
            try:
                with urllib.request.urlopen(url + '/api/world/cell?world=light&x=9&y=497') as response:
                    cell = json.load(response)
                self.assertEqual((cell['pixel_x'], cell['pixel_y']), (72, 3976))
                self.assertEqual(cell['annotation']['normals'], ['north'])
                self.assertEqual((cell['map16_id'], cell['area_id'], cell['flip_x'], cell['flip_y']), (42, 56, False, False))
                with urllib.request.urlopen(url + '/api/world') as response:
                    self.assertEqual(json.load(response)['format'], WORLD_FORMAT)
                with urllib.request.urlopen(url + '/world/light.bin') as response:
                    self.assertEqual(response.read(), raw)
                for path in ('/api/world/cell?world=light&x=512&y=0', '/api/world/cell?world=bogus&x=0&y=0', '/api/world/cell?world=light&x=no&y=0'):
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        urllib.request.urlopen(url + path)
                    self.assertEqual(error.exception.code, 400)
                    error.exception.close()
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(url + '/world/manifest.json')
                self.assertEqual(error.exception.code, 404)
                error.exception.close()
            finally:
                server.shutdown()
                server.server_close()
                thread.join()
            manifest['rom_sha256'] = '2' * 64
            (cache_path / 'manifest.json').write_text(json.dumps(manifest))
            self.assertFalse(cache.valid())
            manifest['rom_sha256'] = ROM_SHA256
            (cache_path / 'manifest.json').write_text(json.dumps(manifest))
            (cache_path / 'dark.bin').write_bytes(b'truncated')
            self.assertFalse(cache.valid())
            with self.assertRaisesRegex(ValueError, 'cache missing or invalid'):
                WorldCache(cache_path, catalog)

    @unittest.skipUnless(shutil.which('node'), 'browser control regression requires node')
    def test_browser_door_cardinal_buttons_and_completion(self):
        # Execute the real embedded control functions without a browser/ROM.
        functions = '\n'.join(line for line in PAGE.splitlines()
                              if line.startswith(('function renderNormals(', 'function complete(',
                                                  'function changeRole(', 'function renderFloorWater(')))
        self.assertIn("['wall','wall_corner','door','doorway'].includes(a.role)", PAGE)
        script = r'''
const assert=require('node:assert/strict');
function element(){return {children:[],append(...items){this.children.push(...items)}}}
const document={createElement:element,createTextNode:text=>({textContent:text})}, detail=element();
let current={role:'door',normals:[]}, selected='tile', saves=0;
const byId=new Map([[selected,{annotation:current}]]);
function annotation(){return byId.get(selected).annotation}
function renderDetail(){}
function scheduleSave(){saves++}
function update(a){byId.get(selected).annotation=a;scheduleSave()}
'''+functions+r'''
renderNormals(current);
let buttons=detail.children[0].children[0].children;
assert.deepEqual(buttons.map(b=>b.textContent),['N','E','S','W']);
for(let [i,n] of ['north','east','south','west'].entries()){
  buttons[i].onclick();
  assert.deepEqual(annotation().normals,[n]);
  assert(complete(annotation()));
}
assert.equal(saves,4);
buttons[3].onclick();
assert.deepEqual(annotation().normals,[]);
assert(!complete(annotation()));
assert(!complete({role:'door',normals:['north_east']}));
assert(!complete({role:'door',normals:['north','east']}));
assert(complete({role:'wall',normals:['north_east']}));
changeRole('out-of-bounds');
assert.equal(annotation().role,'out-of-bounds');
assert.deepEqual(annotation().normals,[]);
assert(complete(annotation()));
assert.equal(saves,5);
changeRole('doorway');
assert(!complete(annotation()));
detail.children=[];renderNormals(annotation());
buttons=detail.children[0].children[0].children;
assert.deepEqual(buttons.map(b=>b.textContent),['N','E','S','W']);
buttons[0].onclick();buttons[1].onclick();
assert.deepEqual(annotation().normals,['east']);
assert(complete(annotation()));
assert(!complete({role:'doorway',normals:['north_east']}));
assert(!complete({role:'doorway',normals:['north','east']}));
changeRole('floor');detail.children=[];renderFloorWater(annotation());
const waterBox=detail.children[0].children[0];
assert.equal(waterBox.checked,false);
waterBox.onchange({target:{checked:true}});
assert.equal(annotation().role,'floor');assert.equal(annotation().water,true);
changeRole('other');assert(!('water' in annotation()));
'''
        subprocess.run([shutil.which('node'), '-e', script], check=True, capture_output=True, text=True)

    def test_explicit_profile_overrides_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            key = 'v1:4bpp:0123456789abcdef'
            def write(path):
                path.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                                f'[[assets]]\ntile_hash = "{key}"\nheight = [7]\n')
            marker_profile, chosen = root / 'marker.toml', root / 'chosen.toml'
            write(marker_profile)
            write(chosen)
            marker = root / 'marker.json'
            marker.write_text(json.dumps(dict(profile=str(marker_profile))))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker, chosen)
            revision = store.refresh()
            self.assertEqual(store.profile_path, chosen.resolve())
            self.assertIn('--profile', store.profile_status)
            store.save_group([key], dict(role='wall', normals=['south']), revision)
            self.assertEqual(tomllib.loads(chosen.read_text())['assets'][0]['tile_role'], 'wall')
            self.assertNotIn('tile_role', tomllib.loads(marker_profile.read_text())['assets'][0])
            marker_only = AnnotationStore(root / 'other.json', ROM_SHA256, marker)
            marker_only.refresh()
            self.assertEqual(marker_only.profile_path, marker_profile)
            self.assertIn('Snes9x marker', marker_only.profile_status)
            other = root / 'other-rom.toml'
            other.write_text('schema_version = 34\n[game]\nrom_sha256 = "' + '2' * 64 + '"\n')
            with self.assertRaises(AtlasError):
                AnnotationStore(root / 'third.json', ROM_SHA256, marker, other).refresh()
            unmarked = AnnotationStore(root / 'fourth.json', ROM_SHA256, root / 'missing.json')
            unmarked.refresh()
            self.assertIsNone(unmarked.profile_path)
            self.assertIn('sidecar only', unmarked.profile_status)

    def test_door_cardinal_facing_profile_and_sidecar_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, marker = root / 'profile.toml', root / 'marker.json'
            key = 'v1:4bpp:0123456789abcdef'
            profile.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                               f'[[assets]]\ntile_hash = "{key}"\ntile_role = "door"\n'
                               'height = [7]\nnormal_xyz = [128, 128, 255]\n')
            marker.write_text(json.dumps(dict(profile=str(profile))))
            for direction in ('north', 'east', 'south', 'west'):
                store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
                store.save_group([key], dict(role='door', normals=[direction]), store.refresh())
                asset = tomllib.loads(profile.read_text())['assets'][0]
                self.assertEqual(asset['tile_semantic_normals'], [direction])
                self.assertEqual(asset['height'], [7])
                self.assertEqual(asset['normal_xyz'], [128, 128, 255])
                reloaded = AnnotationStore(store.path, ROM_SHA256, marker)
                reloaded.refresh()
                self.assertEqual(reloaded.tiles[key]['normals'], [direction])
            # Loading old missing-facing doors is allowed, saving changes is not.
            before = profile.read_bytes()
            with self.assertRaises(AtlasError):
                store.save_group([key], dict(role='door'), store.refresh())
            self.assertEqual(profile.read_bytes(), before)
        for normals in (['north_east'], ['north', 'east'], ['north', 'north']):
            with self.assertRaises(AtlasError):
                validate_annotation(dict(role='door', normals=normals))

    def test_particle_semantic_save_and_reload(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'annotations.json'
            store = AnnotationStore(path, ROM_SHA256)
            annotation = dict(role='particle', note='Floating spark')
            store.save_group(['v1:4bpp:0123456789abcdef'], annotation, store.refresh())
            reloaded = AnnotationStore(path, ROM_SHA256)
            self.assertEqual(reloaded.tiles['v1:4bpp:0123456789abcdef'],
                             validate_annotation(annotation))
            with self.assertRaises(AtlasError):
                validate_annotation(dict(role='particle', normals=['north']))

    def test_out_of_bounds_profile_and_sidecar_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, marker = root / 'profile.toml', root / 'marker.json'
            key = 'v1:4bpp:0123456789abcdef'
            profile.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                               f'[[assets]]\ntile_hash = "{key}"\nheight = [0]\nnormal_xyz = [128,128,255]\n')
            marker.write_text(json.dumps({'profile': str(profile)}))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
            store.save_group([key], dict(role='out-of-bounds'), store.refresh())
            asset = tomllib.loads(profile.read_text())['assets'][0]
            self.assertEqual(asset['tile_role'], 'out-of-bounds')
            self.assertEqual(asset['height'], [0])
            self.assertEqual(asset['normal_xyz'], [128,128,255])
            reloaded = AnnotationStore(store.path, ROM_SHA256, marker)
            reloaded.refresh()
            self.assertEqual(reloaded.tiles[key]['role'], 'out-of-bounds')
        with self.assertRaises(AtlasError):
            validate_annotation(dict(role='out-of-bounds', normals=['north']))

    def test_door_semantic_save_preserves_independent_masks_unless_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, marker = root / 'profile.toml', root / 'marker.json'
            ids = ['v1:4bpp:0123456789abcdef', 'v1:4bpp:fedcba9876543210',
                   'v1:4bpp:1111111111111111']
            masks = ['3f3f3f3f1f1f1f1f', '0000000000000000', None]
            protected = dict(height=list(range(64)), height_sampling='linear',
                             occlusion=[0, 255] * 32, normal_xyz=[128, 128, 255] * 64,
                             emission_rgba=[0] * 256, geometry_by_flip=True,
                             flip_horz_height=[7] * 64)
            text = f'schema_version = 25\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
            for key, mask in zip(ids, masks):
                text += f'\n[[assets]]\ntile_hash = "{key}"\ntile_role = "door"\n'
                for field, value in protected.items():
                    text += f'{field} = {json.dumps(value)}\n'
                if mask is not None:
                    text += f'doorframe_mask = "{mask}"\n'
            profile.write_text(text)
            marker.write_text(json.dumps(dict(profile=str(profile))))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)

            def check(expected_masks, role):
                assets = tomllib.loads(profile.read_text())['assets']
                self.assertEqual([a.get('doorframe_mask') for a in assets], expected_masks)
                for asset in assets:
                    self.assertEqual(asset['tile_role'], role)
                    self.assertEqual({k: asset[k] for k in protected}, protected)
                self.assertFalse(profile.with_name(profile.name + '.semantics.tmp').exists())

            # Group semantics must not copy one member's mask onto another,
            # invent an unknown mask, or erase explicit empty authoring.
            store.save_group(ids, dict(role='doorway', normals=['north'], note='note only'), store.refresh())
            check(masks, 'doorway')
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
            store.save_group(ids, dict(role='doorway', normals=['north'], note='after reload'), store.refresh())
            check(masks, 'doorway')
            # An explicit empty mask is a replacement, not omission.
            store.save_group(ids, dict(role='doorway', normals=['north'], doorframe_mask='0000000000000000'), store.refresh())
            check(['0000000000000000'] * 3, 'doorway')
            store.save_group(ids, dict(role='doorway', normals=['north'], doorframe_mask='ffffffffffffffff'), store.refresh())
            check(['ffffffffffffffff'] * 3, 'doorway')
            store.save_group(ids, dict(role='other'), store.refresh())
            check([None] * 3, 'other')
            # Returning to Door must not resurrect a mask removed by role change.
            store.save_group(ids, dict(role='doorway', normals=['north']), store.refresh())
            check([None] * 3, 'doorway')

    def test_registered_schema22_semantic_save_preserves_group_records(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); profile=root/'profile.toml'; marker=root/'marker.json'
            h='v1:4bpp:0123456789abcdef'
            group=(f'\n[[obstacle_groups]]\nname = "example"\n[[obstacle_groups.members]]\ntile_hash = "{h}"\n'
                   'x = -2\ny = 3\nz = -1\nhflip = true\nvflip = false\nsupport_contact = true\n')
            text=f'schema_version = 22\n[game]\nrom_sha256 = "{ROM_SHA256}"\n[[assets]]\ntile_hash = "{h}"\nheight = [7]\n'+group
            profile.write_text(text);marker.write_text(json.dumps(dict(profile=str(profile))))
            store=AnnotationStore(root/'annotations.json',ROM_SHA256,marker);revision=store.refresh()
            store.save_group([h],dict(role='obstacle',note='external semantic editing'),revision)
            parsed=tomllib.loads(profile.read_text())
            self.assertEqual(parsed['schema_version'],22)
            self.assertEqual(parsed['obstacle_groups'],tomllib.loads(text)['obstacle_groups'])
            self.assertIn(group.strip(),profile.read_text())
            self.assertEqual(parsed['assets'][0]['height'],[7])
            # Invalid membership/contact evidence must be refused before any
            # sidecar/profile mutation, including profile registration refresh.
            invalid=profile.read_text().replace('support_contact = true','support_contact = false')
            profile.write_text(invalid)
            with self.assertRaisesRegex(AtlasError,'explicit support_contact'):store.refresh()
            self.assertEqual(profile.read_text(),invalid)

    def test_stale_browser_put_is_rejected_after_native_save(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256)
            server = ThreadingHTTPServer(('127.0.0.1', 0),
                                         make_handler(TileCatalog(database), store, None, root))
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            url = f'http://127.0.0.1:{server.server_port}'
            try:
                with urllib.request.urlopen(url + '/api/catalog') as response:
                    revision = json.load(response)['revision']
                external = AnnotationStore(store.path, ROM_SHA256)
                external.save('bg', {'role': 'obstacle'})
                request = urllib.request.Request(url + '/api/annotation/bg',
                                                 data=b'{"role":"floor"}', method='PUT',
                                                 headers={'If-Match': revision})
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(request)
                self.assertEqual(error.exception.code, 409)
                error.exception.close()
                self.assertEqual(AnnotationStore(store.path, ROM_SHA256).tiles['bg']['role'], 'obstacle')
            finally:
                server.shutdown()
                server.server_close()
                thread.join()

    def test_profile_refresh_and_bidirectional_save_preserve_other_layers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / 'profile.toml'
            profile.write_text('schema_version = 20\n[game]\nrom_sha256 = "' + ROM_SHA256 +
                               '"\n[[assets]]\ntile_hash = "bg"\nheight = [1, 2, 3]\n'
                               'tile_role = "wall"\ntile_semantics_authored = true\n'
                               'tile_semantic_normals = ["north"]\n')
            marker = root / 'marker.json'
            marker.write_text(json.dumps({'profile': str(profile)}))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
            old_revision = store.refresh()
            self.assertEqual(store.tiles['bg']['normals'], ['north'])
            store.save_group(['bg'], {'role': 'obstacle', 'note': 'changed\nsecond line'}, old_revision)
            parsed = tomllib.loads(profile.read_text())
            self.assertEqual(parsed['assets'][0]['height'], [1, 2, 3])
            self.assertEqual(parsed['assets'][0]['tile_role'], 'obstacle')
            self.assertEqual(parsed['assets'][0]['tile_semantic_note'], 'changed\nsecond line')
            with self.assertRaisesRegex(AtlasError, 'metadata changed'):
                store.save_group(['bg'], {'role': 'floor'}, old_revision)
            profile.write_text(profile.read_text().replace('"obstacle"', '"floor"'))
            with self.assertRaisesRegex(AtlasError, 'profile changed'):
                store.save_group(['bg'], {'role': 'other'})
            store.refresh()
            self.assertEqual(store.tiles['bg']['role'], 'floor')
            self.assertEqual(json.loads(store.path.read_text())['tiles']['bg']['role'], 'floor')
            store.save_group(['bg'], {'role': 'unknown'})
            store.refresh()
            self.assertEqual(store.tiles['bg']['role'], 'unknown')
            profile.write_text(profile.read_text() + '\n[[rules]]\ntile_hash = "bg"\nmaterial = "stone"\n')
            store.refresh()
            store.save_group(['new'], {'role': 'floor'})
            parsed = tomllib.loads(profile.read_text())
            self.assertEqual(parsed['assets'][-1]['tile_hash'], 'new')
            self.assertEqual(parsed['rules'][0]['material'], 'stone')
            self.assertLess(profile.read_text().index('tile_hash = "new"'), profile.read_text().index('[[rules]]'))

    def test_roles_require_their_exact_wall_normals(self) -> None:
        self.assertEqual(validate_annotation(
            {"role": "wall", "normals": ["north"]})["normals"], ["north"])
        self.assertEqual(validate_annotation(
            {"role": "wall_corner", "normals": ["north", "east"],
             "corner_type": "inside"})["normals"],
                         ["north", "east"])
        self.assertEqual(validate_annotation(
            {"role": "wall", "normals": ["north_east"]})["normals"],
                         ["north_east"])
        with self.assertRaisesRegex(AtlasError, "requires 2"):
            validate_annotation({"role": "wall_corner", "normals": ["north"],
                                 "corner_type": "outside"})
        with self.assertRaisesRegex(AtlasError, "distinct"):
            validate_annotation({"role": "wall_corner", "normals": ["north", "north"],
                                 "corner_type": "outside"})
        with self.assertRaisesRegex(AtlasError, "cardinal"):
            validate_annotation({"role": "wall_corner", "normals": ["north", "south_east"],
                                 "corner_type": "outside"})
        with self.assertRaisesRegex(AtlasError, "corner_type"):
            validate_annotation({"role": "wall_corner", "normals": ["north", "east"]})
        with self.assertRaisesRegex(AtlasError, "requires 0"):
            validate_annotation({"role": "floor", "normals": ["north"]})

    def test_legacy_corner_loads_as_incomplete_but_cannot_be_resaved(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.json"
            path.write_text(json.dumps({
                "format": "alttp-tile-semantics-v1", "rom_sha256": ROM_SHA256,
                "tiles": {"bg": {"role": "wall_corner",
                                    "normals": ["north", "east"]}}}))
            store = AnnotationStore(path, ROM_SHA256)
            self.assertEqual(store.tiles["bg"]["corner_type"], "unknown")
            with self.assertRaisesRegex(AtlasError, "corner_type"):
                store.save("bg", store.tiles["bg"])

    def test_catalog_lists_unique_background_art_and_reuse(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "atlas.sqlite"
            make_database(database)
            catalog = TileCatalog(database)
            tiles = catalog.tiles({})
            self.assertEqual([tile["content_id"] for tile in tiles], ["bg"])
            self.assertEqual(tiles[0]["source_count"], 2)
            self.assertEqual(tiles[0]["family_count"], 2)
            self.assertEqual(tiles[0]["packs"], [1, 2])
            self.assertEqual((tiles[0]["first_pack"], tiles[0]["first_tile"]), (1, 2))
            self.assertEqual(tiles[0]["placement_count"], 1)
            self.assertEqual(len(tiles[0]["indices"]), 64)
            detail = catalog.detail("bg")
            self.assertEqual(len(detail["sources"]), 3)
            self.assertEqual(detail["alternatives"], [])

    def test_conversion_aliases_share_annotations_and_hide_low_variant(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "atlas.sqlite"
            make_database(database)
            connection = sqlite3.connect(database)
            connection.executemany("INSERT INTO decoded_tile VALUES (?, ?, ?)",
                                   [("low", 4, bytes([1]) * 64),
                                    ("high", 4, bytes([9]) * 64)])
            connection.executemany("INSERT INTO graphics_source VALUES (?, ?, ?, ?, ?, ?)", [
                ("background", 7, 8, "low", 0x5000, "low"),
                ("background", 7, 8, "high", 0x5000, "high")])
            connection.commit()
            connection.close()
            catalog = TileCatalog(database)
            annotation = {"role": "floor", "normals": [],
                          "context_dependent": False, "note": ""}
            tiles = {tile["content_id"]: tile for tile in catalog.tiles({"high": annotation})}
            self.assertEqual(catalog.semantic_group("low"), ["high", "low"])
            self.assertTrue(tiles["low"]["low_variant"])
            self.assertFalse(tiles["high"]["low_variant"])
            self.assertEqual(tiles["low"]["annotation"], annotation)

            path = Path(directory) / "annotations.json"
            store = AnnotationStore(path, ROM_SHA256)
            store.save_group(catalog.semantic_group("high"), annotation)
            self.assertEqual(store.tiles, {"high": validate_annotation(annotation), "low": validate_annotation(annotation)})

    def test_store_round_trip_is_atomic_and_rom_bound(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.json"
            store = AnnotationStore(path, ROM_SHA256)
            store.save("bg", {"role": "obstacle", "normals": [],
                              "context_dependent": True, "note": "upper railing"})
            loaded = AnnotationStore(path, ROM_SHA256)
            self.assertEqual(loaded.tiles["bg"]["role"], "obstacle")
            self.assertFalse(path.with_name(path.name + ".tmp").exists())
            value = json.loads(path.read_text())
            value["rom_sha256"] = "2" * 64
            path.write_text(json.dumps(value))
            with self.assertRaisesRegex(AtlasError, "fingerprint"):
                AnnotationStore(path, ROM_SHA256)

    def test_empty_unknown_removes_saved_annotation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.json"
            store = AnnotationStore(path, ROM_SHA256)
            store.save("bg", {"role": "floor"})
            store.save("bg", {"role": "unknown"})
            self.assertNotIn("bg", store.tiles)

    def test_room_occurrences_group_rooms_and_preserve_layers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rooms.sqlite"
            connection = sqlite3.connect(path)
            connection.executescript("""
                CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE occurrence (content_id TEXT, room_id INTEGER, layer INTEGER,
                  x INTEGER, y INTEGER, hflip INTEGER, vflip INTEGER);
                CREATE TABLE room_status (room_id INTEGER PRIMARY KEY, status TEXT, detail TEXT);
                CREATE TABLE loaded_art (content_id TEXT, room_id INTEGER,
                  PRIMARY KEY (content_id, room_id));
            """)
            connection.executemany("INSERT INTO metadata VALUES (?, ?)",
                                   [("format", ROOM_INDEX_FORMAT),
                                    ("rom_sha256", ROM_SHA256)])
            connection.executemany("INSERT INTO occurrence VALUES (?, ?, ?, ?, ?, ?, ?)",
                                   [("bg", room, 0, 1, 2, 0, 0) for room in range(320)])
            connection.executemany("INSERT INTO room_status VALUES (?, 'indexed', '')",
                                   ((room,) for room in range(320)))
            connection.execute("INSERT INTO occurrence VALUES ('bg', 3, 1, 4, 5, 1, 0)")
            connection.executemany("INSERT INTO loaded_art VALUES ('bg', ?)", [(27,), (3,)])
            connection.commit()
            connection.close()
            index = RoomOccurrenceIndex(path, ROM_SHA256)
            rooms = index.rooms("bg")
            self.assertEqual(rooms[0], {"room_id": 3, "placement_count": 2,
                                        "bg1_count": 1, "bg2_count": 1})
            self.assertEqual(index.cells("bg", 3)[1],
                             {"layer": 1, "x": 4, "y": 5, "hflip": 1, "vflip": 0})
            self.assertEqual(index.loaded_rooms("bg"), [3, 27])

    def test_obscured_room_is_last_preview_choice(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rooms.sqlite"
            connection = sqlite3.connect(path)
            connection.executescript("""
                CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE occurrence (content_id TEXT, room_id INTEGER, layer INTEGER,
                  x INTEGER, y INTEGER, hflip INTEGER, vflip INTEGER);
                CREATE TABLE room_status (room_id INTEGER PRIMARY KEY, status TEXT, detail TEXT);
                CREATE TABLE loaded_art (content_id TEXT, room_id INTEGER,
                  PRIMARY KEY (content_id, room_id));
            """)
            connection.executemany("INSERT INTO metadata VALUES (?, ?)",
                                   [("format", ROOM_INDEX_FORMAT),
                                    ("rom_sha256", ROM_SHA256)])
            connection.executemany("INSERT INTO room_status VALUES (?, 'indexed', '')",
                                   ((room,) for room in range(320)))
            connection.executemany("INSERT INTO occurrence VALUES ('bg', ?, 0, 0, 0, 0, 0)",
                                   [(27,)] * 10 + [(42,)])
            connection.commit()
            connection.close()
            self.assertEqual([room["room_id"] for room in
                              RoomOccurrenceIndex(path, ROM_SHA256).rooms("bg")], [42, 27])


if __name__ == "__main__":
    unittest.main()
