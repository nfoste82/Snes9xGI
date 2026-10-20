import tempfile
from pathlib import Path
import unittest

from alttp_npc_coverage import source_functions, integer_table, compare_observations, build_coverage, link_sequences


class CoverageTests(unittest.TestCase):
    def test_local_link_bounds_and_adapter_table_agree(self):
        engine = Path('build/remaster-geometry/room-preview-engine-v21')
        if not engine.exists():
            self.skipTest('Local ROM-backed reference engine not available')
        sequences = link_sequences(engine)
        by_state = {s['adapter_state']: s for s in sequences}
        self.assertEqual(by_state[0]['steps'], 9)
        self.assertEqual(by_state[4]['steps'], 12)
        self.assertEqual(by_state[2]['steps'], 6)
        self.assertEqual(len(by_state), len(sequences))

    def test_balanced_source_inventory(self):
        functions = source_functions('''
void Draw(int k) { if (k) { Other(k); } /* } */ }
void Other(int k) { const char *s = "}"; }
''')
        self.assertEqual(set(functions), {'Draw', 'Other'})
        self.assertIn('Other(k)', functions['Draw'])
        self.assertEqual(integer_table('static int Frames[3] = {0, 0x10, -1};', 'Frames'), [0, 16, -1])

    def test_all_dispatch_entries_including_unplaced_and_null(self):
        with tempfile.TemporaryDirectory() as tmp:
            engine = Path(tmp); (engine/'src').mkdir()
            table = ','.join(['&Sprite_Raven', 'NULL'] + ['&Sprite_Unknown']*241)
            for name in ('player.c', 'player_oam.c', 'nmi.c'):
                (engine/'src'/name).write_text('')
            (engine/'src/sprite_main.c').write_text('static void *kSpriteActiveRoutines[243] = {'+table+'};\n'
                                                  'void Sprite_Raven(int k) { Draw(k); }\nvoid Draw(int k) {}')
            report = build_coverage(engine, [], [])
            self.assertFalse(report['complete'])
            self.assertEqual(len(report['sprite_types']), 243)
            self.assertEqual(report['sprite_types'][1]['status'], 'no_dispatch')
            self.assertEqual(report['sprite_types'][0]['direct_calls'], ['Draw'])

    def test_artwork_and_authoring_gaps_are_separate(self):
        manifest = dict(actors=[dict(poses=[dict(parts=[dict(tile_hash='known')])])])
        observed = [dict(capture='test', tiles=[dict(tile_hash='known', has_height=False, has_normals=True),
                                              dict(tile_hash='missing', has_height=True, has_normals=False)])]
        result = compare_observations(manifest, observed)['captures'][0]
        self.assertEqual([t['tile_hash'] for t in result['absent_from_library']], ['missing'])
        self.assertEqual(result['missing_height'], ['known'])
        self.assertEqual(result['missing_normals'], ['missing'])
