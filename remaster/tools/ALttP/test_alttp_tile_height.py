"""Temporary-file persistence and real worker algorithm regressions (no browser required)."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer
import urllib.request
import urllib.error

from alttp_tile_semantic_atlas import AnnotationStore, TileCatalog, make_handler
from alttp_tile_height_weights import HeightWeightStore, WeightConflict, validate_weight
from test_alttp_tile_semantic_atlas import make_database, ROM_SHA256


class HeightTests(unittest.TestCase):
    def test_legacy_weights_scaled_once_and_saved_as_integers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            store = HeightWeightStore(AnnotationStore(root / 'annotations.json', ROM_SHA256), TileCatalog(database))
            initial = store.config()
            self.assertEqual(initial['weights']['bg'], 8)
            path = Path(initial['path'])
            for old, new in ((0, 0), (.25, 2), (.5, 4), (1, 8), (2, 16), (4, 32)):
                path.write_text(json.dumps(dict(format='alttp-experimental-height-weights-v1',
                    binding=initial['binding'], weights={'bg':old})))
                legacy = path.read_bytes()
                config = store.config()
                self.assertEqual(config['weights']['bg'], new)
                self.assertEqual(path.read_bytes(), legacy)  # reads do not rewrite authored files
                saved = store.save('bg', float(new), config['revision'])
                doc = json.loads(path.read_text())
                self.assertEqual(doc['format'], 'alttp-experimental-height-weights-v2')
                self.assertIs(type(doc['weights']['bg']), int)
                self.assertEqual(store.config()['weights']['bg'], new)  # no second scaling
                self.assertEqual(saved['weights']['bg'], new)
            path.write_text(json.dumps(dict(format='alttp-experimental-height-weights-v2',
                binding=initial['binding'], weights={'bg':.5})))
            with self.assertRaises(ValueError):
                store.config()

    def test_validation(self):
        for value in (-1, .25, .5, 4.01, 32.01, 33, float('nan'), float('inf'), True, '1', None):
            with self.assertRaises(ValueError):
                validate_weight(value)
        self.assertEqual(validate_weight(4), 4)
        self.assertIs(type(validate_weight(8.0)), int)
        self.assertEqual(validate_weight(32), 32)
        self.assertEqual(validate_weight(0), 0)

    def test_persistence_binding_aliases_revision_atomicity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            catalog = TileCatalog(database)
            # Exercise canonical alias defaults without requiring a real ROM database.
            half = 'v1:4bpp:437727f0595cb424'
            catalog.aliases['bg'] = ['bg', half]
            catalog.aliases[half] = ['bg', half]
            annotations = AnnotationStore(root / 'annotations.json', ROM_SHA256)
            weights = HeightWeightStore(annotations, catalog)
            initial = weights.config()
            path = Path(initial['path'])
            self.assertFalse(path.exists())  # GET/launch does not persist defaults
            self.assertEqual(initial['weights']['bg'], 4)
            self.assertEqual(initial['weights'][half], 4)
            saved = weights.save('bg', 2, initial['revision'])
            self.assertEqual(saved['weights'][half], 2)
            self.assertFalse(list(root.glob('*.tmp')))
            raw = path.read_bytes()
            with self.assertRaises(WeightConflict):
                weights.save('bg', 2, initial['revision'])
            self.assertEqual(path.read_bytes(), raw)
            for bad in (-1, float('nan'), 33, .25):
                with self.assertRaises(ValueError):
                    weights.save('bg', bad, saved['revision'])
            self.assertEqual(path.read_bytes(), raw)
            annotations.save('bg', dict(role='wall', normals=['south']))
            with self.assertRaises(WeightConflict):
                weights.save('bg', 1, saved['revision'])
            self.assertEqual(path.read_bytes(), raw)
            doc = json.loads(raw)
            doc['binding']['rom_sha256'] = '2' * 64
            path.write_text(json.dumps(doc))
            with self.assertRaises(WeightConflict):
                weights.config()

    def test_profile_marker_changes_reject_previous_tab_and_explicit_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            catalog = TileCatalog(database)
            marker = root / 'marker.json'
            profiles = [root / 'a.toml', root / 'b.toml']
            for p in profiles:
                p.write_text(f'schema_version = 34\n[game]\nrom_sha256 = "{ROM_SHA256}"\n')
            marker.write_text(json.dumps(dict(profile=str(profiles[0]))))
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256, marker)
            weights = HeightWeightStore(store, catalog)
            first = weights.config()
            saved = weights.save('bg', 2, first['revision'])
            old = Path(saved['path']).read_bytes()
            marker.write_text(json.dumps(dict(profile=str(profiles[1]))))
            with self.assertRaises(WeightConflict):
                weights.save('bg', 4, saved['revision'])
            self.assertEqual(Path(saved['path']).read_bytes(), old)
            self.assertFalse(Path(str(profiles[1]) + '.height-weights.json').exists())
            explicit = HeightWeightStore(store, catalog, Path(saved['path']))
            with self.assertRaises(WeightConflict):
                explicit.config()
            self.assertEqual(profiles[0].read_bytes(), profiles[1].read_bytes())

    def test_http_weights_worker_and_invalid_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            database = root / 'atlas.sqlite'
            make_database(database)
            store = AnnotationStore(root / 'annotations.json', ROM_SHA256)
            server = ThreadingHTTPServer(('127.0.0.1', 0), make_handler(TileCatalog(database), store, None, root))
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            url = f'http://127.0.0.1:{server.server_port}'
            def put(path, value, revision):
                request = urllib.request.Request(url + path, data=json.dumps(value).encode(), method='PUT',
                    headers={'Content-Type': 'application/json', 'If-Match': revision})
                with urllib.request.urlopen(request) as response:
                    return json.load(response)
            try:
                with urllib.request.urlopen(url + '/api/weights') as response:
                    config = json.load(response)
                for asset in ('/height-worker.js', '/height-ui.js', '/world.js'):
                    with urllib.request.urlopen(url + asset) as response:
                        self.assertIn('javascript', response.headers['Content-Type'])
                        self.assertEqual(response.headers['Cache-Control'], 'no-store')
                        self.assertTrue(response.read())
                updated = put('/api/weights/bg', {'weight': 4}, config['revision'])
                self.assertEqual(updated['weights']['bg'], 4)
                for route, value, revision, code in (
                    ('/api/weights/bg', 2, config['revision'], 409),
                    ('/api/weights/bg', .25, updated['revision'], 400),
                    ('/api/weights/bg', -1, updated['revision'], 400),
                    ('/api/weights/../../evil', 1, updated['revision'], 400)):
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        put(route, {'weight': value}, revision)
                    self.assertEqual(error.exception.code, code)
                    error.exception.close()
            finally:
                server.shutdown(); server.server_close(); thread.join()

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_worker_matches_independent_reference(self):
        # Independent scalar implementation, including repeated anchor resets and
        # nonzero wall contributions south of the bottommost original anchor.
        import random
        rng = random.Random(1717)
        width, height = 13, 11
        north = [rng.choice([-2, -.5, 0, .5, 1]) for _ in range(width * height)]
        east = [rng.choice([-1, -.5, 0, .5, 1]) for _ in north]
        anchors = [0] * len(north)
        for x, y in [(2, 8), (2, 6), (6, 7), (10, 9)]:
            anchors[y * width + x] = 1
        expected = [0.] * len(north)
        seeds = []
        for x in range(width):
            ys = [y for y in range(height) if anchors[y * width + x]]
            if not ys:
                continue
            seeds.append(x)
            start = max(ys)
            for y in range(start - 1, -1, -1):
                i = y * width + x
                expected[i] = 0 if anchors[i] else expected[i + width] + north[i]
            for y in range(start + 1, height):
                i = y * width + x
                expected[i] = expected[i - width] - north[i - width]
        for x in range(width):
            if x in seeds:
                continue
            source = min(seeds, key=lambda s: (abs(s - x), s))
            for y in range(height):
                value = expected[y * width + source]
                if x > source:
                    for step in range(source + 1, x + 1):
                        value += east[y * width + step]
                else:
                    for step in range(source, x, -1):
                        value -= east[y * width + step]
                expected[y * width + x] = value
        module = Path(__file__).with_name('alttp_tile_height_worker.js').resolve()
        script = "const m=require(process.argv[1]), fs=require('fs'),i=JSON.parse(fs.readFileSync(0,'utf8'));process.stdout.write(JSON.stringify([...m.anchoredWalk(i.w,i.h,i.n,i.e,i.a).heights]));"
        process = subprocess.run(['node', '-e', script, str(module)], text=True, capture_output=True,
            input=json.dumps(dict(w=width, h=height, n=north, e=east, a=anchors)), check=True)
        self.assertEqual(json.loads(process.stdout), expected)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_worker_synthetic_walk_flips_fraction_scope_and_errors(self):
        module = Path(__file__).with_name('alttp_tile_height_worker.js').resolve()
        script = r'''
const assert = require('node:assert/strict');
const {heightDeltas:d, anchoredWalk:walk, calculateHeights:calculate} = require(MODULE);
assert.deepEqual(d({role:'wall',normals:['south_east']},0,.5),[.5,-.5]);
assert.deepEqual(d({role:'wall',normals:['south_east']},3,.5),[-.5,.5]);
assert.deepEqual(d({role:'wall_corner',normals:['north','south']},0,1),[0,0]);
assert.deepEqual(d({role:'floor',normals:['south']},0,1),[0,0]);
// North entering cell; south subtracts delta of the cell LEFT, not entered.
assert.deepEqual([...walk(1,5,[1,.5,-2,1,1],[0,0,0,0,0],[0,0,1,0,0]).heights], [1.5,.5,0,2,1]);
// Other original anchors reset the walk rather than reconcile discontinuities.
assert.deepEqual([...walk(1,4,[5,5,5,5],[0,0,0,0],[1,0,0,1]).heights], [0,10,5,0]);
// Columns 1 and 3 seeded: nearest tie at 2 must use western seed.
assert.deepEqual([...walk(5,1,[0,0,0,0,0],[9,2,.5,4,1],[0,1,0,1,0]).heights], [-2,0,.5,0,1]);
assert.throws(()=>walk(0,0,[],[],[]), /No original sand anchors/);
assert.throws(()=>walk(2,2,[0,0,0,0],[0,0,0,0],[0,0,0,0]), /No original/);
const grid = new ArrayBuffer(256*256*8), view = new DataView(grid);
view.setUint32(((500-256)*256+100)*8,1,true);
view.setUint32(((499-256)*256+100)*8,2,true);
const tiles = [{annotation:{role:'floor'},weight:1}, {annotation:{role:'floor'},sand:true,weight:1},
 {annotation:{role:'wall',normals:['south_east']},weight:.5}];
const legacy={world:'light',mode:'legacy-sand',bounds:{x0:0,y0:256,width:256,height:256},grid,tiles};
const out=calculate(legacy);
assert.equal(out.stats.anchors,1); assert.equal(out.stats.seededColumns,1);
assert.equal(out.stats.y0,256); assert.equal(out.stats.width,256);
assert.equal(out.heights[(499-256)*256+100],.5);
assert.equal(out.heights[(500-256)*256+100],0);
assert.equal(out.stats.computed + out.stats.missing,65536);
assert.throws(()=>calculate({...legacy,world:'dark'}),/Light World only/);
assert.throws(()=>calculate({...legacy,grid:new ArrayBuffer(0)}),/compact/);
assert.throws(()=>calculate({...legacy,grid:new ArrayBuffer(grid.byteLength)}),/No original sand anchors/);
// Arbitrary global origin and nonzero manual reference: no reads beyond compact scope.
const bounds={x0:301,y0:71,width:3,height:3};
const small=new ArrayBuffer(9*8), sv=new DataView(small);
for(let i=0;i<9;i++) {sv.setUint32(i*8,0,true);sv.setUint8(i*8+7,0);}
const manual={world:'light',mode:'manual',bounds,grid:small,tiles,
 reference:{world:'light',x:302,y:72,height:10.25}};
const inputJSON=JSON.stringify(manual), inputBytes=Buffer.from(small).toString('hex');
const light=calculate(manual);
assert.equal(JSON.stringify(manual),inputJSON);
assert.equal(Buffer.from(small).toString('hex'),inputBytes);
assert.deepEqual([...light.heights],Array(9).fill(10.25));
assert.equal(light.stats.version,'diagonal-propagation-v37-1');
assert.deepEqual(light.bounds,bounds); assert.equal(light.stats.computed,9);
assert.equal(light.stats.anchors,1); assert.equal(light.reference.height,10.25);
const dark=calculate({...manual,world:'dark',reference:{...manual.reference,world:'dark'}});
assert.deepEqual([...dark.heights],[...light.heights]);assert.equal(dark.world,'dark');
assert.throws(()=>calculate({...manual,world:'dark'}),/for this world/);
sv.setUint32(1*8,2,true);sv.setUint8(1*8+7,3); // Both placement flips, fractional diagonal.
const flipped=calculate(manual);
assert.equal(flipped.heights[1],10.25); // Descending entry defers subtraction until leaving the wall.
assert.equal(flipped.stats.computed + flipped.stats.missing,9);
for(const reference of [null,{...manual.reference,height:Infinity},{...manual.reference,x:300},{...manual.reference,y:74}]) {
 assert.throws(()=>calculate({...manual,reference}),/reference|Reference/);
}
for(const b of [null,{...bounds,x0:-1},{...bounds,width:0},{...bounds,x0:511},
 {...bounds,y0:511},{...bounds,width:1.5}]) assert.throws(()=>calculate({...manual,bounds:b}),/bounds/);
const missing=calculate({...manual,tiles:[]});
assert.equal(missing.stats.missingSources,9);assert.equal(missing.stats.unknownRoles,9);
assert.equal(missing.stats.computed,1); // Only the explicitly pinned reference.
assert.equal(missing.stats.missing,8);
assert.equal(missing.assigned[4],1);
assert(Number.isNaN(missing.heights[0]));
'''.replace('MODULE', json.dumps(str(module)))
        subprocess.run([shutil.which('node'), '-e', script], check=True, capture_output=True, text=True)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_explicit_count_skip_examples_and_inverse(self):
        # Historical filter remains available to reproduce prior experiments;
        # calculateHeights now uses diagonalWalk instead.
        script = Path(__file__).with_name('test_alttp_tile_height_filter.js')
        process = subprocess.run([shutil.which('node'), str(script)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_diagonal_crossing_paths_and_missing_heights(self):
        script = Path(__file__).with_name('test_alttp_tile_height_diagonal.js')
        process = subprocess.run([shutil.which('node'), str(script)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_world_drag_selection(self):
        script = Path(__file__).with_name('test_alttp_tile_semantic_world.js')
        process = subprocess.run([shutil.which('node'), str(script)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_editor_mock_snapshot_references_and_stale_results(self):
        script = Path(__file__).with_name('test_alttp_tile_height_ui.js')
        process = subprocess.run([shutil.which('node'), str(script)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_explicit_boundary_gradient(self):
        script = Path(__file__).with_name('test_alttp_tile_gradient.js')
        process = subprocess.run([shutil.which('node'), str(script)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_tile_layer_previews(self):
        script = Path(__file__).with_name('test_alttp_tile_layers.js')
        process = subprocess.run([shutil.which('node'), str(script)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)

    @unittest.skipUnless(shutil.which('node'), 'requires Node')
    def test_viewport_snapshot_bounds_and_stale_comparison(self):
        module = Path(__file__).with_name('alttp_tile_height_ui.js').resolve()
        script = r'''
const assert=require('node:assert/strict');
const {heightVisibleBounds:b,heightSameBounds:same}=require(MODULE);
assert.deepEqual(b(-2401,-563,1,23,19),{x0:300,y0:70,width:3,height:3});
assert.deepEqual(b(100,50,1,4200,4200),{x0:0,y0:0,width:512,height:512});
assert.deepEqual(b(800,800,1,100,100),{x0:0,y0:0,width:0,height:0});
assert.deepEqual(b(-5000,-5000,1,100,100),{x0:512,y0:512,width:0,height:0});
const snapshot=b(-2401,-563,1,23,19);
assert(same(snapshot,b(-2401,-563,1,23,19))); // redraw / resize callback: no invalidation
assert(same(snapshot,b(-2400.5,-563.5,1,23,19))); // same snapped cells
assert(!same(snapshot,b(-2408,-563,1,23,19))); // pan crosses cell boundary
assert(!same(snapshot,b(-2401,-563,2,23,19))); // zoom changes snapshot
assert.equal(b(0,0,0,100,100),null);
'''.replace('MODULE', json.dumps(str(module)))
        subprocess.run([shutil.which('node'), '-e', script], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
