import json
from pathlib import Path
import tempfile
import tomllib
import unittest

from alttp_tile_height_offsets import HeightOffsetStore, OffsetConflict
from alttp_tile_semantic_atlas import AnnotationStore
from test_alttp_tile_semantic_atlas import ROM_SHA256


class HeightOffsets(unittest.TestCase):
    def test_large_http_selection_save_values_remove_and_size_limit(self):
        from http.server import ThreadingHTTPServer
        import threading
        import urllib.request
        import urllib.error
        from alttp_tile_semantic_atlas import TileCatalog, make_handler, MAX_BODY, MAX_OFFSET_BODY
        from test_alttp_tile_semantic_atlas import make_database
        # Even the largest normal per-cell payload fits in one atomic request.
        full = json.dumps(dict(world='light', values=[dict(x=x,y=y,offset=-32767)
                          for y in range(512) for x in range(512)])).encode()
        self.assertLess(len(full), MAX_OFFSET_BODY)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            db = root/'atlas.sqlite'
            make_database(db)
            path = root/'profile.toml'
            path.write_text(f'schema_version = 39\n[game]\nrom_sha256 = "{ROM_SHA256}"\n')
            store = AnnotationStore(root/'annotations.json', ROM_SHA256, profile=path)
            server = ThreadingHTTPServer(('127.0.0.1',0), make_handler(TileCatalog(db),store,None,root))
            thread = threading.Thread(target=server.serve_forever,daemon=True)
            thread.start()
            url = f'http://127.0.0.1:{server.server_port}/api/height-offsets'
            locations = [dict(x=i%512,y=384+i//512) for i in range(5000)]
            try:
                revision = store.refresh()
                for payload, expected in (
                    (dict(world='light',locations=locations,offset=8),8),
                    (dict(world='light',values=[dict(at,offset=at['x']-256) for at in locations]),None),
                    (dict(world='light',locations=locations,offset=None),None)):
                    body = json.dumps(payload).encode()
                    self.assertGreater(len(body), MAX_BODY)
                    with urllib.request.urlopen(urllib.request.Request(url,method='PUT',data=body,
                            headers={'If-Match':revision,'Content-Type':'application/json'})) as response:
                        saved = json.load(response)
                    revision = saved['revision']
                    if 'values' in payload:
                        self.assertEqual(len(saved['offsets']),len(locations))
                        for at in locations:
                            self.assertEqual(saved['offsets'][f"light:{at['x']}:{at['y']}"],at['x']-256)
                    elif expected is not None:
                        self.assertEqual(len(saved['offsets']),len(locations))
                        self.assertEqual(set(saved['offsets'].values()),{expected})
                    else:
                        self.assertEqual(saved['offsets'],{})
                before = path.read_bytes()
                # Reject oversized declared bodies before reading or modifying data.
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(urllib.request.Request(url,method='PUT',data=b'{}',
                        headers={'Content-Length':str(MAX_OFFSET_BODY+1),'If-Match':revision}))
                self.assertEqual(error.exception.code,400)
                self.assertIn('16 MiB',error.exception.read().decode())
                error.exception.close()
                self.assertEqual(path.read_bytes(),before)
            finally:
                server.shutdown();server.server_close();thread.join()

    def test_distinct_baked_values_are_atomic(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            path=root/'profile.toml'
            path.write_text(f'schema_version = 39\n[game]\nrom_sha256 = "{ROM_SHA256}"\n')
            store=HeightOffsetStore(AnnotationStore(root/'annotations.json',ROM_SHA256,profile=path))
            config=store.config()
            saved=store.save_values('light',[dict(x=1,y=2,offset=8),dict(x=2,y=2,offset=16),dict(x=3,y=2,offset=0)],config['revision'])
            self.assertEqual(saved['heights'],{'light:1:2':8,'light:2:2':16,'light:3:2':0})
            before=path.read_bytes()
            for values in ([dict(x=1,y=2,offset=4),dict(x=2,y=2,offset=.5)],
                           [dict(x=1,y=2,offset=4),dict(x=1,y=2,offset=8)]):
                with self.assertRaises(ValueError):store.save_values('light',values,saved['revision'])
                self.assertEqual(path.read_bytes(),before)

    def test_bulk_offsets_atomic_validation_zero_remove_and_unselected_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'profile.toml'
            path.write_text(f'schema_version = 39\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                            '[[overworld_offsets]]\nworld = "dark"\nbackground = 1\nx = 1\ny = 2\noffset = 7\n'
                            '[[overworld_offsets]]\nworld = "light"\nbackground = 0\nx = 1\ny = 2\noffset = 9\n'
                            '[[overworld_offsets]]\nworld = "light"\nbackground = 1\nx = 8\ny = 9\noffset = 11\n')
            store = HeightOffsetStore(AnnotationStore(root/'annotations.json', ROM_SHA256, profile=path))
            config = store.config()
            targets = [dict(x=1,y=2), dict(x=3,y=4), dict(x=1,y=2)]
            before = path.read_bytes()
            for invalid in ([], targets+[dict(x=512,y=1)], [dict(x=True,y=1)], 'bad'):
                with self.assertRaises(ValueError):
                    store.save_group('light', invalid, 5, config['revision'])
                self.assertEqual(path.read_bytes(), before)
            for value in (-12, 0, None):
                config = store.save_group('light', targets, value, config['revision'])
                for key in ('light:1:2', 'light:3:4'):
                    if value is None:
                        self.assertNotIn(key, config['heights'])
                    else:
                        self.assertEqual(config['heights'][key], value)
                self.assertEqual(config['heights']['dark:1:2'], 7)
                self.assertEqual(config['heights']['light:8:9'], 11)
                rows = tomllib.loads(path.read_text())['overworld_offsets']
                self.assertEqual(next(r['offset'] for r in rows if r['background']==0), 9)
                self.assertEqual(len(rows), 3 if value is None else 5)
            before = path.read_bytes()
            with self.assertRaises(OffsetConflict):
                store.save_group('light', targets, 8, 'stale')
            self.assertEqual(path.read_bytes(), before)

    def test_native_roundtrip_exact_coordinate_zero_remove_and_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'profile.toml'
            path.write_text(f'schema_version = 39\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
                            '[[assets]]\ntile_hash = "a"\nheight = [7]\n'
                            '[[overworld_terrain]]\nworld = "dark"\nbackground = 1\nx = 3\ny = 4\n'
                            'normal_offset = [0, 0, 1]\nheight_offset = [' + ', '.join(['2']*64) + ']\n')
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=path)
            offsets = HeightOffsetStore(store)
            original = tomllib.loads(path.read_text())
            initial = offsets.config()
            saved = offsets.save('light', 3, 4, -8, initial['revision'])
            self.assertEqual(saved['heights']['light:3:4'], -8)
            self.assertEqual(saved['heights']['dark:3:4'], 2)
            self.assertTrue(saved['enabled'])
            before = path.read_bytes()
            with self.assertRaises(OffsetConflict):
                offsets.save('light', 3, 4, 9, initial['revision'])
            self.assertEqual(path.read_bytes(), before)
            saved = offsets.save('light', 3, 4, 0, saved['revision'])
            self.assertEqual(saved['heights']['light:3:4'], 0)
            for value in (True, .5, 32768, -32768, '2'):
                with self.assertRaises(ValueError):
                    offsets.save('light', 3, 4, value, saved['revision'])
            reloaded = HeightOffsetStore(AnnotationStore(store.path, ROM_SHA256, profile=path))
            self.assertEqual(reloaded.config()['heights']['light:3:4'], 0)
            removed = offsets.save('light', 3, 4, None, saved['revision'])
            self.assertNotIn('light:3:4', removed['heights'])
            parsed = tomllib.loads(path.read_text())
            for field in ('assets', 'overworld_terrain', 'game'):
                self.assertEqual(parsed[field], original[field])
            # Nonuniform terrain is never averaged into a single authored height.
            path.write_text(path.read_text().replace('height_offset = [2,', 'height_offset = [3,'))
            config = offsets.config()
            self.assertIn('dark:3:4', config['nonuniform'])
            self.assertNotIn('dark:3:4', config['heights'])

    def test_http_and_missing_profile(self):
        from http.server import ThreadingHTTPServer
        import threading
        import urllib.request
        import urllib.error
        from alttp_tile_semantic_atlas import TileCatalog, make_handler
        from test_alttp_tile_semantic_atlas import make_database
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            db = root / 'atlas.sqlite'
            make_database(db)
            path = root / 'profile.toml'
            path.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n')
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=path)
            server = ThreadingHTTPServer(('127.0.0.1', 0), make_handler(TileCatalog(db), store, None, root))
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            url = f'http://127.0.0.1:{server.server_port}/api/height-offsets'
            try:
                with urllib.request.urlopen(url) as response:
                    config = json.load(response)
                def put(revision, locations=None):
                    return urllib.request.urlopen(urllib.request.Request(url, method='PUT',
                        data=json.dumps(dict(world='dark', x=10, y=20, offset=40) if locations is None else
                                        dict(world='dark', locations=locations, offset=40)).encode(),
                        headers={'If-Match':revision}))
                with put(config['revision']) as response:
                    saved = json.load(response)
                    self.assertEqual(saved['heights']['dark:10:20'], 40)
                with put(saved['revision'], [dict(x=11,y=20),dict(x=12,y=20)]) as response:
                    saved = json.load(response)
                    self.assertEqual(saved['heights']['dark:11:20'], 40)
                    self.assertEqual(saved['heights']['dark:12:20'], 40)
                with self.assertRaises(urllib.error.HTTPError) as error:
                    put(config['revision'])
                self.assertEqual(error.exception.code, 409)
                error.exception.close()
                self.assertEqual(tomllib.loads(path.read_text())['schema_version'], 37)
            finally:
                server.shutdown(); server.server_close(); thread.join()
            missing = HeightOffsetStore(AnnotationStore(root / 'other.json', ROM_SHA256))
            with self.assertRaises(ValueError):
                missing.save('light', 1, 2, 0, missing.config()['revision'])
