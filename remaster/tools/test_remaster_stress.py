"""No ROM/GUI required: fail closed on false stress coverage or comparability."""
import copy
import hashlib
import json
import plistlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import remaster_stress as stress


def workload(kind='many_lights', mode='deterministic'):
    s = dict(name='torches', kinds=[kind], route=[dict(frames=3, buttons=['right'])],
             representatives=[1, 3], offscreen_representatives=[1], presentations=[mode], warmup_frames=2,
             rom=dict(path='rom', sha256='a'*64), state=dict(path='state', sha256='b'*64), profile=dict(path='profile', sha256='c'*64),
             min_direct_samples=100, initial_room=18)
    return dict(id='torches-r3-h1-'+mode, scenario=s, representatives=3, offscreen_representatives=1, presentation=mode)


def report(w=None):
    w = w or workload()
    result = {k: 'fixed' for k in stress.IDENTITY}
    result.update(status='complete', stress_schema=1, route='3:right', measurement_frames=3, warmup_frames=2,
                  representative_emitters=3, offscreen_representative_emitters=1, presentation=w['presentation'],
                  rom_file_sha256='a'*64, state_sha256='b'*64, profile_sha256='c'*64,
                  stage_counters_disabled=True, audio_muted=True, debug_sphere_enabled=False,
                  serial_direct_override=False, serial_indirect_override=False, full_mesh_override=False,
                  full_sphere_samples_override=False, full_depth_samples_override=False,
                  expanded_sampled_depth_override=False, directional_disabled_override=False,
                  reference_receiver_emission_override=False, direct_samples_override='', transport_threads_override='default 8x8')
    frames = [dict(frame=n, room=18, link_x=100+n*40, link_y=100, plane=0, module=7, submodule=0, indoors=True,
                   refresh_ms=12, epoch=1, movable_placement_cells=4, packet_available=True,
                   effective_representatives=3, effective_offscreen_representatives=1) for n in range(3)]
    samples = [dict(emulated_frame=n+2, gpu_ms=10, scene_fields_ms=2, mesh_ms=.3, gpu_setup_ms=.1,
                    source_build_ms=.2, direct_samples=145, emitters=600, sphere_lights=0, sphere_samples=0,
                    depth_emitters=20, emitter_signature='1234') for n in range(3)]
    result['cases'] = [dict(name='Composite (0 bounces)', view=0, bounces=0, width=256, height=224,
                            samples=samples, gameplay_frames=frames, completed_measured_frames=3,
                            matches_first_case_ram=True, ram_sha256='d'*64, original_rgb555_sha256='e'*64,
                            measurement_wall_seconds=.1)]
    return result


def summary(w=None):
    w = w or workload()
    r = stress.validate_report(report(w), w)
    return dict(schema=1, suite_sha256='manifest', workloads={w['id']: stress.summarize_runs([r, r, r])})


class StressContracts(unittest.TestCase):
    def test_route_validation(self):
        self.assertEqual(stress.route_text([dict(frames=20, buttons=['a', 'up']), dict(frames=2, buttons=['none'])]), ('20:a+up,2:none', 22))
        for buttons in ([], ['none', 'a'], ['right', 'left'], ['up', 'down'], ['a', 'a'], ['start']):
            with self.subTest(buttons=buttons), self.assertRaises(ValueError):
                stress.route_text([dict(frames=1, buttons=buttons)])

    def test_valid_many_lights_and_sweep(self):
        w = workload()
        self.assertEqual(len(list(stress.workloads(dict(scenarios=[w['scenario']])))), 2)
        self.assertEqual(stress.validate_report(report(), w)['metrics']['completion_fps']['median'], 30)

    def test_does_not_accept_cheaper_or_empty_stress(self):
        for change in ('light_count', 'representatives', 'area_override', 'missing_frame', 'gpu_missing', 'nan', 'experiment', 'lineage'):
            r = report();c = r['cases'][0]
            if change == 'light_count':
                c['samples'][1]['direct_samples'] = 1
            elif change == 'representatives':
                r['representative_emitters'] = 1
            elif change == 'area_override':
                c['gameplay_frames'][1]['effective_representatives'] = 1
            elif change == 'missing_frame':
                c['gameplay_frames'].pop()
            elif change == 'gpu_missing':
                c['samples'].pop();c['completed_measured_frames'] = 2
            elif change == 'nan':
                c['samples'][0]['gpu_ms'] = float('nan')
            elif change == 'experiment':
                r['serial_direct_override'] = True
            else:
                c['samples'][1]['emulated_frame'] = 2
            with self.subTest(change=change), self.assertRaises(ValueError):
                stress.validate_report(r, workload())

    def test_movement_is_observed_not_just_input(self):
        w = workload('movement');r = report(w)
        stress.validate_report(r, w)
        for f in r['cases'][0]['gameplay_frames']:
            f['link_x'] = 100
        with self.assertRaisesRegex(ValueError, 'did not move'):
            stress.validate_report(r, w)

    def test_movable_requires_active_placements(self):
        w = workload('movable');r = report(w)
        stress.validate_report(r, w)
        for f in r['cases'][0]['gameplay_frames']:
            f['movable_placement_cells'] = 0
        with self.assertRaisesRegex(ValueError, 'movable-group'):
            stress.validate_report(r, w)

    def test_transition_requires_room_and_phase(self):
        w = workload('transition');w['scenario']['final_room'] = 19
        r = report(w)
        with self.assertRaisesRegex(ValueError, 'final room'):
            stress.validate_report(r, w)
        frames = r['cases'][0]['gameplay_frames'];frames[-1]['room'] = 19
        with self.assertRaisesRegex(ValueError, 'transition phase'):
            stress.validate_report(r, w)
        frames[1]['submodule'] = 2
        stress.validate_report(r, w)

    def test_stair_round_trip_requires_both_crossings(self):
        w = workload('stairs_round_trip');r = report(w)
        with self.assertRaisesRegex(ValueError, 'stair round trip'):
            stress.validate_report(r, w)
        frames = r['cases'][0]['gameplay_frames']
        frames[0].update(plane=1, link_y=100)
        frames[1].update(plane=0, link_y=130, submodule=16)
        frames[2].update(plane=1, link_y=100)
        stress.validate_report(r, w)
        frames[2]['plane'] = 0
        with self.assertRaisesRegex(ValueError, 'stair round trip'):
            stress.validate_report(r, w)

    def test_exit_requires_actual_outdoor_gameplay_not_stale_room(self):
        w = workload('overworld_exit');r = report(w)
        with self.assertRaisesRegex(ValueError, 'overworld exit'):
            stress.validate_report(r, w)
        frames = r['cases'][0]['gameplay_frames']
        frames[1]['module'] = 15
        frames[2].update(indoors=False, module=9)
        stress.validate_report(r, w)  # Room RAM remains stale after leaving indoors.
        frames[2]['module'] = 8
        with self.assertRaisesRegex(ValueError, 'overworld exit'):
            stress.validate_report(r, w)

    def test_door_scroll_is_not_inferred_from_input(self):
        w = workload('indoor_scroll');r = report(w)
        with self.assertRaisesRegex(ValueError, 'doorway scroll'):
            stress.validate_report(r, w)
        r['cases'][0]['gameplay_frames'][1]['submodule'] = 1
        stress.validate_report(r, w)

    def test_single_stair_phase_is_required(self):
        w = workload('stairs_traversal');r = report(w)
        with self.assertRaisesRegex(ValueError, 'staircase traversal'):
            stress.validate_report(r, w)
        r['cases'][0]['gameplay_frames'][1]['submodule'] = 16
        stress.validate_report(r, w)

    def test_isolation_preserves_original_bundle_and_uses_private_home(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);app = root/'Original.app';contents = app/'Contents';contents.mkdir(parents=True)
            original = plistlib.dumps(dict(CFBundleIdentifier='original.identifier'))
            (contents/'Info.plist').write_bytes(original)
            output = root/'run';output.mkdir()
            with patch.object(stress.subprocess, 'run') as process:
                executable, home = stress.isolate_application(app, output)
            self.assertEqual((contents/'Info.plist').read_bytes(), original)
            copied = plistlib.loads((output/'Snes9x-Stress.app/Contents/Info.plist').read_bytes())
            self.assertTrue(copied['CFBundleIdentifier'].startswith('org.snes9x.stress.'))
            self.assertTrue((home/'Library/Application Support').is_dir())
            self.assertEqual(executable, output/'Snes9x-Stress.app/Contents/MacOS/Snes9x')
            self.assertEqual(process.call_args.args[0][:4], ['codesign', '--force', '--deep', '--sign'])

    def test_cpu_gpu_throughput_regressions_fail(self):
        base = summary();current = copy.deepcopy(base);name = next(iter(current['workloads']))
        runs = current['workloads'][name]['runs']
        for run in runs:
            m = run['metrics'];m['gpu_ms']['median'] = 20;m['refresh_ms']['p95'] = 20;m['completion_fps']['median'] = 10
        current['workloads'][name] = stress.summarize_runs(runs)
        checks = stress.compare(base, current)
        self.assertEqual(sum(c['regression'] for c in checks), 3)

    def test_unequal_scenario_or_hardware_is_invalid(self):
        for key in ('gpu', 'os', 'state_sha256', 'route', 'window_width', 'representative_emitters'):
            base = summary();new = copy.deepcopy(base);next(iter(new['workloads'].values()))['identity'][key] = 'changed'
            with self.subTest(key=key), self.assertRaises(ValueError):
                stress.compare(base, new)
        base = summary();new = copy.deepcopy(base);next(iter(new['workloads'].values()))['behavior']['sequence_sha256'] = 'changed'
        with self.assertRaises(ValueError):
            stress.compare(base, new)

    def test_source_disappearance_cannot_be_speedup(self):
        w = workload();a = report(w);b = copy.deepcopy(a);b['cases'][0]['samples'][1]['direct_samples'] = 144
        first, second = stress.validate_report(a, w), stress.validate_report(b, w)
        with self.assertRaises(ValueError):
            stress.summarize_runs([first, second, first])

    def test_live_subset_not_claimed_deterministic(self):
        w = workload(mode='live');r = report(w);r['cases'][0]['samples'].pop();r['cases'][0]['completed_measured_frames'] = 2
        result = stress.validate_report(r, w)
        self.assertAlmostEqual(result['metrics']['unsubmitted_fraction']['median'], 1/3)
        self.assertNotIn('source_sequence_sha256', result['behavior'])

    def test_two_repeats_accepted_one_rejected(self):
        run = stress.validate_report(report(), workload())
        self.assertEqual(len(stress.summarize_runs([run, run])['runs']), 2)
        with self.assertRaises(ValueError):
            stress.summarize_runs([run])

    def test_pin_new_manifest_and_detect_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);w = workload()
            for name in ('rom', 'state', 'profile'):
                (root/name).write_text(name)
            path = root/'suite.json';path.write_text(json.dumps(dict(schema=1, scenarios=[w['scenario']])))
            pinned = root/'pinned.json'
            self.assertEqual(stress.main(['pin', str(path), '--output', str(pinned)]), 0)
            suite = stress.load_suite(pinned);stress.verify_inputs(pinned, suite)
            (root/'state').write_text('changed')
            with self.assertRaises(ValueError):
                stress.verify_inputs(pinned, suite)

    def test_profile_companions_are_pinned_including_absence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);w = workload()
            for name in ('rom', 'state', 'profile'):
                (root/name).write_text(name)
            path = root/'suite.json';path.write_text(json.dumps(dict(schema=1, scenarios=[w['scenario']])))
            pinned = root/'pinned.json'
            self.assertEqual(stress.main(['pin', str(path), '--output', str(pinned)]), 0)
            suite = stress.load_suite(pinned);stress.verify_inputs(pinned, suite)
            companion = root/'profile.geometry-review';companion.mkdir()
            (companion/'room-012.bin').write_bytes(b'geometry')
            with self.assertRaisesRegex(ValueError, 'companions'):
                stress.verify_inputs(pinned, suite)
            second = root/'with-maps.json'
            self.assertEqual(stress.main(['pin', str(path), '--output', str(second)]), 0)
            suite = stress.load_suite(second);stress.verify_inputs(second, suite)
            (companion/'room-012.bin').write_bytes(b'changed geometry')
            with self.assertRaisesRegex(ValueError, 'companions'):
                stress.verify_inputs(second, suite)

    def test_record_refuses_running_app_before_launch(self):
        from argparse import Namespace
        with patch.object(stress, 'digest', return_value='fixed'), patch.object(stress, 'verify_inputs'), patch.object(stress.subprocess, 'run') as process:
            process.return_value.returncode = 0
            with self.assertRaisesRegex(ValueError, 'Quit Snes9x yourself'):
                stress.execute_suite(Namespace(suite=Path('suite')), {})
            self.assertEqual(process.call_count, 1)

    def test_clean_environment_and_no_rebaseline(self):
        with patch.dict(stress.os.environ, {'S9X_REMASTER_DIRECT_SAMPLES': '8', 'MTL_DEBUG_LAYER': '1'}):
            env = stress.clean_environment()
        self.assertNotIn('S9X_REMASTER_DIRECT_SAMPLES', env);self.assertNotIn('MTL_DEBUG_LAYER', env)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'baseline.json';stress.write_new(path, {})
            with self.assertRaises(FileExistsError):
                stress.write_new(path, {'replacement': True})

    def test_missing_metrics_and_forged_summaries_are_invalid(self):
        for change in ('empty', 'metric', 'percentile', 'forged', 'runs'):
            base = summary();new = copy.deepcopy(base);w = next(iter(new['workloads'].values()))
            if change == 'empty':
                w['metrics'] = {}
            elif change == 'metric':
                del w['metrics']['gpu_ms']
            elif change == 'percentile':
                del w['runs'][0]['metrics']['gpu_ms']['p95']
            elif change == 'forged':
                w['metrics']['gpu_ms']['median'] = 1
            else:
                w['runs'] = w['runs'][:1]
            with self.subTest(change=change), self.assertRaises(ValueError):
                stress.compare(base, new)

    def test_transition_spike_is_gated(self):
        w = workload('transition');w['scenario']['final_room'] = 19
        r = report(w);r['cases'][0]['gameplay_frames'][1]['submodule'] = 2
        r['cases'][0]['gameplay_frames'][-1]['room'] = 19
        run = stress.validate_report(r, w)
        base = dict(schema=1, suite_sha256='manifest', workloads={w['id']: stress.summarize_runs([run]*3)})
        candidate = copy.deepcopy(base)
        for run in candidate['workloads'][w['id']]['runs']:
            run['metrics']['transition_refresh_ms']['max'] = 40
        candidate['workloads'][w['id']] = stress.summarize_runs(candidate['workloads'][w['id']]['runs'])
        failures = [c for c in stress.compare(base, candidate) if c['regression']]
        self.assertEqual([(c['metric'], c['stat']) for c in failures], [('transition_refresh_ms', 'max')])

    def test_compare_cli_exit_codes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);base = summary();new = copy.deepcopy(base)
            stress.write_new(root/'base.json', base);stress.write_new(root/'new.json', new)
            args = ['compare', '--baseline', str(root/'base.json'), '--candidate', str(root/'new.json'), '--output', str(root/'pass.json')]
            self.assertEqual(stress.main(args), 0)
            name = next(iter(new['workloads']))
            for run in new['workloads'][name]['runs']:
                run['metrics']['gpu_ms'].update(median=20, p95=20, p99=20, max=20)
            new['workloads'][name] = stress.summarize_runs(new['workloads'][name]['runs'])
            (root/'new.json').write_text(json.dumps(new))
            args[-1] = str(root/'regression.json')
            self.assertEqual(stress.main(args), 1)
            new['workloads'][next(iter(new['workloads']))]['metrics'] = {}
            (root/'new.json').write_text(json.dumps(new))
            args[-1] = str(root/'invalid.json')
            self.assertEqual(stress.main(args), 2)
            self.assertFalse((root/'invalid.json').exists())


if __name__ == '__main__':
    unittest.main()
