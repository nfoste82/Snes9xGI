import base64
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace

from alttp_calculated_heights import CELL, WORLD_BYTES, CalculatedHeightConflict, CalculatedHeightStore
from alttp_tile_semantic_atlas import AnnotationStore
from test_alttp_tile_semantic_atlas import ROM_SHA256


class CalculatedHeights(unittest.TestCase):
    def test_atomic_area_replacement_and_world_isolation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'profile.toml'
            authored = f'schema_version = 37\n[game]\nrom_sha256 = "{ROM_SHA256}"\n'
            path.write_text(authored)
            annotations = AnnotationStore(root / 'annotations.json', ROM_SHA256, profile=path)
            weights = SimpleNamespace(config=lambda: dict(binding=dict(profile_path=str(path)),
                                                          revision=annotations.refresh()))
            store = CalculatedHeightStore(annotations, weights)
            empty = store.config()
            self.assertIsNone(empty['cells'])
            bounds = dict(x0=50, y0=10, width=3, height=1)
            inputs = weights.config()['revision']
            request = dict(world='light', bounds=bounds, cells=[[1, -12], [2, 0], [1, 7]], input_revision=inputs)
            light = store.save(request, empty['revision'])
            dark = store.save(dict(request, world='dark'), light['revision'])
            with self.assertRaises(CalculatedHeightConflict):
                store.save(request, light['revision'])
            with self.assertRaises(ValueError):
                store.save(dict(request, cells=[[1, 1.5], None, None]), dark['revision'])
            self.assertEqual(store.config()['revision'], dark['revision'])
            patched = store.save(dict(request, bounds=dict(bounds, width=2), cells=[None, [3, 0]]), dark['revision'])
            reopened = CalculatedHeightStore(annotations, weights).config()
            self.assertEqual(reopened, patched)
            raw = base64.b64decode(reopened['cells'])
            at = (10 * 512 + 50) * 3
            self.assertEqual(list(CELL.iter_unpack(raw[at:at+9])), [(0, 0), (3, 0), (1, 7)])
            self.assertEqual(list(CELL.iter_unpack(raw[WORLD_BYTES+at:WORLD_BYTES+at+9])), [(1, -12), (2, 0), (1, 7)])
            self.assertEqual(path.read_text(), authored)
            path.write_text(authored + '# changed\n')
            with self.assertRaises(CalculatedHeightConflict):
                store.save(request, patched['revision'])
            Path(patched['path']).write_bytes(b'broken')
            with self.assertRaisesRegex(ValueError, 'Invalid or ROM-mismatched'):
                store.config()
