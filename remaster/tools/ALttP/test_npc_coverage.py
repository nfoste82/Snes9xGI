import tempfile
from pathlib import Path
import unittest
import json
import subprocess

from alttp_npc_workbench import build_adapter

from alttp_npc_coverage import source_functions, integer_table, compare_observations, build_coverage, link_sequences


class CoverageTests(unittest.TestCase):
    def test_link_effect_timeline_and_equipment(self):
        engine = Path('build/remaster-geometry/room-preview-engine-v21').resolve()
        if not (engine / 'zelda3.sfc').exists():
            self.skipTest('Local ROM-backed reference engine not available')
        with tempfile.TemporaryDirectory() as tmp:
            adapter = build_adapter(engine, Path(tmp))
            def draw(action, sword, steps):
                command = [str(adapter), '0', '0', '0', 'link', '4', str(steps),
                           '0', '0', '0', str(action), str(sword), '1', '0']
                return json.loads(subprocess.check_output(command, cwd=engine))['poses']
            charge = draw(2, 2, 64)
            self.assertEqual(charge, draw(2, 2, 64))
            spin = draw(4, 2, 41)
            reference = (engine / 'src/player.c').read_text()
            graphics = integer_table(reference, 'kLinkSpinGraphicsByDir')
            delays = integer_table(reference, 'kLinkSpinDelays')
            timeline = [0] * 3
            for phase in range(1, 12):
                timeline.extend([phase] * (delays[phase] + 1))
            for direction in range(4):
                charged = charge[direction * 64:(direction + 1) * 64]
                release = spin[direction * 41:(direction + 1) * 41]
                self.assertFalse(any(p.get('effect') for p in charged[0]['parts']))
                self.assertTrue(any(p.get('effect') for p in charged[9]['parts']))
                self.assertTrue(any(p.get('effect') and p['address'] == 0x4d70 for p in charged[48]['parts']))
                for tick, phase in enumerate(timeline):
                    state = release[tick]['draw_state']
                    self.assertEqual(state['spin_frame'], graphics[direction * 12 + phase])
                    self.assertEqual(state['spin_step'], phase)
                    self.assertEqual(state['oam_action'], 15)
                self.assertEqual(release[len(timeline)]['draw_state']['oam_action'], 0)
                self.assertFalse(any(p.get('effect') for p in release[-1]['parts']))
                # The initial flash, rotating trail and fading tail are all present.
                addresses = {p['address'] for pose in release for p in pose['parts'] if p.get('effect')}
                self.assertTrue({0x4920, 0x48c0, 0x4d60, 0x4930, 0x4d70, 0x4b70, 0x4800, 0x4830} <= addresses)
                for pose in charged + release:
                    effects = [p for p in pose['parts'] if p.get('effect')]
                    self.assertTrue(all(any(p['indices']) for p in effects))
                    self.assertTrue(all(-48 <= p['x'] <= 48 and -48 <= p['y'] <= 48 for p in effects))
                    self.assertTrue(all(any(p['colors']) for p in effects))
            # All four sword levels must produce the effect, not just the sampled master sword.
            for sword in (1, 3, 4):
                poses = draw(4, sword, 25)
                self.assertTrue(all(any(p.get('effect') for p in poses[d * 25 + 18]['parts']) for d in range(4)))

    def test_local_link_bounds_and_adapter_table_agree(self):
        engine = Path('build/remaster-geometry/room-preview-engine-v21')
        if not engine.exists():
            self.skipTest('Local ROM-backed reference engine not available')
        sequences = link_sequences(engine)
        by_state = {s['adapter_state']: s for s in sequences}
        self.assertEqual(by_state[0]['steps'], 9)
        self.assertEqual(by_state[4]['steps'], 41)
        self.assertEqual(by_state[2]['steps'], 64)
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
