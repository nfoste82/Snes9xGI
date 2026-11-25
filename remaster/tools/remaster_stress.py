#!/usr/bin/env python3
"""Opt-in, pinned ALttP performance scenarios. No baseline updates during check.

Exit 0: pass, 1: performance regression, 2: invalid/missing evidence or setup.
Planning/comparison never launches an app; record/check require no running app.
"""
import argparse
import hashlib
import itertools
import json
import math
import os
import plistlib
from pathlib import Path
import shutil
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BUTTONS = {'none', 'left', 'right', 'up', 'down', 'a', 'b', 'x', 'y'}
KINDS = {'many_lights', 'movable', 'movement', 'transition', 'stairs_round_trip',
         'stairs_traversal', 'indoor_scroll', 'overworld_exit'}
IDENTITY = ('gpu', 'os', 'rom_file_sha256', 'state_sha256', 'profile_sha256',
            'presentation', 'representative_emitters', 'offscreen_representative_emitters',
            'hybrid_direct', 'hybrid_direct_cutoff', 'hybrid_direct_tail_samples',
            'hybrid_direct_override', 'hybrid_direct_cutoff_override', 'hybrid_direct_tail_samples_override',
            'connections', 'measurement_frames', 'warmup_frames', 'route',
            'window_width', 'window_height', 'stage_counters_disabled',
            'lighting_coordinate_scale', 'reflectance_boost', 'original_scene_contribution',
            'sample_accumulation', 'rom_fps')
TIMING_METRICS = ('gpu_ms', 'refresh_ms', 'scene_fields_ms', 'mesh_ms',
                  'gpu_setup_ms', 'source_build_ms')


def read(path):
    return json.loads(Path(path).read_text())


def write_new(path, value):
    with Path(path).open('x') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def integer(value, minimum, maximum, name):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f'{name}: expected integer {minimum}..{maximum}')
    return value


def numeric(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
        raise ValueError(f'{name}: invalid nonnegative finite number')
    return value


def route_text(steps):
    if not isinstance(steps, list) or not steps:
        raise ValueError('route must contain steps')
    result, frames = [], 0
    for step in steps:
        n = integer(step['frames'], 1, 36000, 'route frames')
        buttons = step['buttons']
        if not isinstance(buttons, list) or not buttons or len(buttons) != len(set(buttons)):
            raise ValueError('route buttons must be a nonempty unique list')
        if set(buttons) - BUTTONS or ('none' in buttons and len(buttons) != 1):
            raise ValueError('unknown/mixed none button')
        if {'left', 'right'} <= set(buttons) or {'up', 'down'} <= set(buttons):
            raise ValueError('opposing route directions')
        result.append(f'{n}:'+ '+'.join(buttons))
        frames += n
    integer(frames, 1, 36000, 'total route frames')
    return ','.join(result), frames


def load_suite(path, require_pins=True):
    suite = read(path)
    if suite.get('schema') != 1 or not suite.get('scenarios'):
        raise ValueError('unsupported/empty stress suite')
    seen = set()
    for scenario in suite['scenarios']:
        name = scenario['name']
        if not isinstance(name, str) or not name or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-_' for c in name) or name in seen:
            raise ValueError('invalid/duplicate scenario name')
        seen.add(name)
        kinds = scenario['kinds']
        if not isinstance(kinds, list) or not kinds or set(kinds) - KINDS:
            raise ValueError(f'{name}: invalid scenario kinds')
        route_text(scenario['route'])
        integer(scenario.get('warmup_frames', 30), 0, 3600, 'warmup_frames')
        for key in ('representatives', 'offscreen_representatives'):
            values = scenario[key]
            if not isinstance(values, list) or not values or len(set(values)) != len(values):
                raise ValueError(f'{name}: empty/duplicate sweep {key}')
            for value in values:
                integer(value, 0, 8, key)
        for key in ('min_direct_samples', 'min_movable_cells', 'min_movement_pixels'):
            integer(scenario.get(key, 1), 1, 1000000, key)
        for key in ('initial_room', 'final_room'):
            if key in scenario:
                integer(scenario[key], 0, 319, key)
        if 'transition' in kinds and scenario.get('initial_room') == scenario.get('final_room'):
            raise ValueError(f'{name}: transition needs different initial_room/final_room')
        if 'transition' in kinds and ('initial_room' not in scenario or 'final_room' not in scenario):
            raise ValueError(f'{name}: transition must pin initial and final rooms')
        if any(x not in ('deterministic', 'live') for x in scenario['presentations']) or not scenario['presentations']:
            raise ValueError('invalid presentations')
        if len(set(scenario['presentations'])) != len(scenario['presentations']):
            raise ValueError('duplicate presentations')
        for key in ('rom', 'state', 'profile'):
            fixture = scenario[key]
            if not isinstance(fixture, dict) or not isinstance(fixture.get('path'), str):
                raise ValueError('fixtures need path and sha256')
            if require_pins and (len(fixture.get('sha256', '')) != 64 or any(c not in '0123456789abcdef' for c in fixture['sha256'])):
                raise ValueError(f'{name}: {key} is not pinned; run pin first')
    return suite


def fixture_path(manifest, fixture):
    path = Path(fixture['path']).expanduser()
    return path.resolve() if path.is_absolute() else (manifest.parent/path).resolve()


def verify_inputs(manifest, suite):
    for scenario in suite['scenarios']:
        for key in ('rom', 'state', 'profile'):
            path = fixture_path(manifest, scenario[key])
            if not path.is_file() or digest(path) != scenario[key]['sha256']:
                raise ValueError(f'{scenario["name"]}: missing/changed {key}: {path}')
        profile = fixture_path(manifest, scenario['profile'])
        if profile_companions(profile) != scenario['profile'].get('companions', {}):
            raise ValueError(f'{scenario["name"]}: changed/unpinned profile companions')


def profile_companions(profile):
    directory = Path(str(profile)+'.geometry-review')
    if not directory.exists():
        return {}
    if not directory.is_dir():
        raise ValueError('profile geometry-review companion must be a directory')
    return {p.name: digest(p) for p in sorted(directory.glob('*.bin')) if p.is_file()}


def workloads(suite):
    for scenario in suite['scenarios']:
        for reps, hidden, mode in itertools.product(scenario['representatives'], scenario['offscreen_representatives'], scenario['presentations']):
            yield dict(id=f'{scenario["name"]}-r{reps}-h{hidden}-{mode}', scenario=scenario,
                       representatives=reps, offscreen_representatives=hidden, presentation=mode)


def stats(values):
    if not values:
        raise ValueError('missing metric samples')
    v = sorted(numeric(x, 'metric sample') for x in values)
    return dict(median=statistics.median(v), p95=v[math.ceil(.95*len(v))-1],
                p99=v[math.ceil(.99*len(v))-1], max=v[-1], count=len(v))


def validate_report(report, workload):
    s = workload['scenario']
    route, frames = route_text(s['route'])
    if report.get('status') != 'complete' or report.get('stress_schema') != 1 or len(report.get('cases', [])) != 1:
        raise ValueError('incomplete/non-stress report')
    for key in IDENTITY:
        if key not in report:
            raise ValueError(f'missing report identity: {key}')
    expected = dict(route=route, measurement_frames=frames, warmup_frames=s.get('warmup_frames', 30),
                    representative_emitters=workload['representatives'],
                    offscreen_representative_emitters=workload['offscreen_representatives'],
                    presentation=workload['presentation'])
    expected.update({key+'_sha256' if key != 'rom' else 'rom_file_sha256': s[key]['sha256'] for key in ('rom', 'state', 'profile')})
    for key, value in expected.items():
        if report.get(key) != value:
            raise ValueError(f'{workload["id"]}: wrong effective {key}')
    if not report['stage_counters_disabled'] or not report.get('audio_muted') or report.get('debug_sphere_enabled'):
        raise ValueError('instrumentation/audio/sphere environment mismatch')
    for key in ('serial_direct_override', 'serial_indirect_override', 'full_mesh_override',
                'full_sphere_samples_override', 'full_depth_samples_override', 'expanded_sampled_depth_override',
                'directional_disabled_override', 'reference_receiver_emission_override'):
        if report.get(key) is not False:
            raise ValueError(f'experimental override: {key}')
    if report.get('direct_samples_override') != '' or report.get('transport_threads_override') != 'default 8x8':
        raise ValueError('experimental direct/layout override')
    case = report['cases'][0]
    for key in ('view', 'bounces', 'width', 'height'):
        integer(case[key], 0 if key in ('view', 'bounces') else 1, 65536, key)
    if case['bounces'] != 0 or case.get('name') != 'Composite (0 bounces)':
        raise ValueError('stress suite requires the Composite zero-bounce workload')
    observations = case.get('gameplay_frames', [])
    if [r.get('frame') for r in observations] != list(range(frames)):
        raise ValueError('incomplete/unordered gameplay evidence')
    for r in observations:
        numeric(r['refresh_ms'], 'refresh')
        if not r['packet_available'] or r['effective_representatives'] != workload['representatives'] or r['effective_offscreen_representatives'] != workload['offscreen_representatives']:
            raise ValueError('missing packet or area settings defeat representative sweep')
    samples = case.get('samples', [])
    tags = [r['emulated_frame'] for r in samples]
    if not tags or tags != sorted(set(tags)) or any(n < s.get('warmup_frames', 30) or n >= s.get('warmup_frames', 30)+frames for n in tags):
        raise ValueError('invalid GPU frame lineage')
    if case.get('completed_measured_frames') != len(samples) or not case.get('matches_first_case_ram'):
        raise ValueError('invalid GPU count/gameplay RAM')
    if workload['presentation'] == 'deterministic' and len(samples) != frames:
        raise ValueError('deterministic GPU sequence incomplete')
    for key in ('ram_sha256', 'original_rgb555_sha256'):
        if not isinstance(case.get(key), str) or len(case[key]) != 64:
            raise ValueError(f'missing final gameplay hash {key}')
    if 'initial_room' in s and observations[0]['room'] != s['initial_room']:
        raise ValueError('wrong initial room')
    if 'final_room' in s and observations[-1]['room'] != s['final_room']:
        raise ValueError('route did not reach expected final room')
    if 'many_lights' in s['kinds'] and min(r['direct_samples'] for r in samples) < s.get('min_direct_samples', 100):
        raise ValueError('many-light stress was not exercised')
    if 'movable' in s['kinds'] and max(r['movable_placement_cells'] for r in observations) < s.get('min_movable_cells', 1):
        raise ValueError('no active movable-group placements')
    if 'movement' in s['kinds']:
        by_room = {}
        for r in observations:
            by_room.setdefault(r['room'], []).append((r['link_x'], r['link_y']))
        distance = max(max(x for x, y in xy)-min(x for x, y in xy)+max(y for x, y in xy)-min(y for x, y in xy) for xy in by_room.values())
        if distance < s.get('min_movement_pixels', 64):
            raise ValueError('character did not move far enough within a room')
    if 'transition' in s['kinds'] and (len(set(r['room'] for r in observations)) < 2 or not any(r['module'] == 7 and r['submodule'] in (2, 18, 19) for r in observations)):
        raise ValueError('no observed room transition phase and room change')
    if 'stairs_round_trip' in s['kinds']:
        switches = [(a, b) for a, b in zip(observations, observations[1:]) if
                    a['indoors'] and b['indoors'] and a['room'] == b['room'] and a['plane'] != b['plane']]
        if (len(switches) < 2 or switches[0][0]['plane'] != switches[-1][1]['plane'] or
                not any(r['module'] == 7 and r['submodule'] == 16 for r in observations) or
                not any(b['link_y'] > a['link_y'] for a, b in zip(observations, observations[1:])) or
                not any(b['link_y'] < a['link_y'] for a, b in zip(observations, observations[1:]))):
            raise ValueError('no observed stair round trip with both plane changes and vertical directions')
    if 'indoor_scroll' in s['kinds'] and not any(r['indoors'] and r['module'] == 7 and r['submodule'] == 1 for r in observations):
        raise ValueError('no observed indoor doorway scroll phase')
    if 'stairs_traversal' in s['kinds'] and not any(r['indoors'] and r['module'] == 7 and r['submodule'] == 16 for r in observations):
        raise ValueError('no observed indoor staircase traversal phase')
    if 'overworld_exit' in s['kinds']:
        if (not observations[0]['indoors'] or observations[-1]['indoors'] or observations[-1]['module'] != 9 or
                not any(r['module'] == 15 for r in observations) or
                not any(a['indoors'] and not b['indoors'] for a, b in zip(observations, observations[1:]))):
            raise ValueError('no observed indoor-to-overworld exit and settled overworld gameplay')
    sequence = [{k: r[k] for k in ('frame', 'module', 'submodule', 'indoors', 'room', 'link_x', 'link_y', 'plane')} for r in observations]
    source_sequence = [{k: r[k] for k in ('emulated_frame', 'emitters', 'direct_samples', 'sphere_lights', 'sphere_samples', 'depth_emitters', 'emitter_signature')} for r in samples]
    behavior = dict(sequence_sha256=hashlib.sha256(json.dumps(sequence, sort_keys=True).encode()).hexdigest(),
                    ram_sha256=case['ram_sha256'], original_rgb555_sha256=case['original_rgb555_sha256'],
                    case={k: case[k] for k in ('view', 'bounces', 'width', 'height')})
    # Admitted frame subsets legitimately differ in live mode. Deterministic
    # mode must not appear faster merely by losing sources on the same frames.
    if workload['presentation'] == 'deterministic':
        behavior['source_sequence_sha256'] = hashlib.sha256(json.dumps(source_sequence, sort_keys=True).encode()).hexdigest()
    wall = numeric(case['measurement_wall_seconds'], 'measurement wall')
    if wall <= 0:
        raise ValueError('invalid wall interval')
    metrics = dict(gpu_ms=stats([r['gpu_ms'] for r in samples]),
                   refresh_ms=stats([r['refresh_ms'] for r in observations]),
                   scene_fields_ms=stats([r['scene_fields_ms'] for r in samples]),
                   mesh_ms=stats([r['mesh_ms'] for r in samples]),
                   gpu_setup_ms=stats([r['gpu_setup_ms'] for r in samples]),
                   source_build_ms=stats([r['source_build_ms'] for r in samples]))
    metrics['completion_fps'] = dict(median=len(samples)/wall)
    metrics['unsubmitted_fraction'] = dict(median=1-len(samples)/frames)
    if set(s['kinds']) & {'transition', 'stairs_round_trip', 'stairs_traversal', 'indoor_scroll', 'overworld_exit'}:
        transition_frames = [r for n, r in enumerate(observations) if
                             r['module'] == 7 and r['submodule'] in (1, 2, 5, 16, 18, 19) or
                             r['module'] in (8, 15, 16) or
                             n > 0 and any(observations[n-1][k] != r[k] for k in ('room', 'indoors', 'plane'))]
        metrics['transition_refresh_ms'] = stats([r['refresh_ms'] for r in transition_frames])
    return dict(identity={k: report[k] for k in IDENTITY}, behavior=behavior, metrics=metrics)


def summarize_runs(runs):
    if len(runs) < 2:
        raise ValueError('at least two independent runs required')
    for r in runs[1:]:
        if r['identity'] != runs[0]['identity'] or r['behavior'] != runs[0]['behavior']:
            raise ValueError('incomparable identity or gameplay sequence across runs')
    metrics = {key: {stat: statistics.median(r['metrics'][key][stat] for r in runs)
                     for stat in runs[0]['metrics'][key] if stat != 'count'} for key in runs[0]['metrics']}
    return dict(identity=runs[0]['identity'], behavior=runs[0]['behavior'], metrics=metrics, runs=runs)


def validate_summary(summary):
    if summary.get('schema') != 1 or not summary.get('workloads'):
        raise ValueError('unsupported/empty performance summaries')
    for name, workload in summary['workloads'].items():
        if set(workload['identity']) != set(IDENTITY) or not workload['behavior']:
            raise ValueError(f'{name}: incomplete summary identity/behavior')
        required = set(TIMING_METRICS) | {'completion_fps', 'unsubmitted_fraction'}
        if set(workload['metrics']) not in (required, required | {'transition_refresh_ms'}):
            raise ValueError(f'{name}: incomplete/unknown metric set')
        runs = workload['runs']
        for run in runs:
            if set(run['metrics']) != set(workload['metrics']):
                raise ValueError(f'{name}: inconsistent run metrics')
            for metric, values in run['metrics'].items():
                expected = {'median'} if metric in ('completion_fps', 'unsubmitted_fraction') else {'median', 'p95', 'p99', 'max', 'count'}
                if set(values) != expected:
                    raise ValueError(f'{name}: incomplete metric {metric}')
                for value in values.values():
                    numeric(value, metric)
                if metric == 'unsubmitted_fraction' and values['median'] > 1:
                    raise ValueError('invalid unsubmitted fraction')
        rebuilt = summarize_runs(runs)
        if any(workload[key] != rebuilt[key] for key in ('identity', 'behavior', 'metrics')):
            raise ValueError(f'{name}: summary disagrees with recorded runs')


def compare(base, candidate, relative=15, absolute=.5):
    validate_summary(base)
    validate_summary(candidate)
    if set(base['workloads']) != set(candidate['workloads']) or base['suite_sha256'] != candidate['suite_sha256']:
        raise ValueError('incomparable suite/workload list')
    checks = []
    for name, old in base['workloads'].items():
        new = candidate['workloads'][name]
        if old['identity'] != new['identity'] or old['behavior'] != new['behavior']:
            raise ValueError(f'{name}: incomparable inputs, settings, hardware or gameplay')
        for key, values in old['metrics'].items():
            for stat in ('median', 'p95', 'max') if key == 'transition_refresh_ms' else ('median', 'p95'):
                if stat not in values:
                    continue
                before, after = numeric(values[stat], key), numeric(new['metrics'][key][stat], key)
                if key == 'completion_fps':
                    limit = before*(1-relative/100)
                    failed = after < limit
                elif key == 'unsubmitted_fraction':
                    limit = before+.05  # percentage points, not milliseconds
                    failed = after > limit
                else:
                    limit = before+max(absolute, before*relative/100)
                    failed = after > limit
                checks.append(dict(workload=name, metric=key, stat=stat, before=before, after=after, limit=limit, regression=failed))
    return checks


def clean_environment():
    env = {k: v for k, v in os.environ.items() if not k.startswith(('S9X_REMASTER_', 'LIGHTING_', 'ROOM012_', 'MTL_'))}
    env['S9X_REMASTER_DISABLE_STAGE_COUNTERS'] = '1'
    env['S9X_REMASTER_BENCHMARK_COMPOSITE_ONLY'] = '1'
    return env


def isolate_application(app, output):
    """Copy/sign a private bundle and use a fresh Foundation home + prefs domain.

    HOME alone is insufficient on macOS. CFFIXED_USER_HOME redirects Foundation
    support paths, and a unique bundle ID avoids the user's cfprefsd domain.
    No existing application or preference files are rewritten.
    """
    bundle = app if app.suffix == '.app' else app.parent.parent.parent
    if not (bundle/'Contents/Info.plist').is_file():
        raise ValueError('isolation requires an app bundle or its bundled executable')
    private = output/'Snes9x-Stress.app'
    shutil.copytree(bundle, private, symlinks=True)
    info_path = private/'Contents/Info.plist'
    info = plistlib.loads(info_path.read_bytes())
    info['CFBundleIdentifier'] = 'org.snes9x.stress.'+hashlib.sha256(str(output.resolve()).encode()).hexdigest()[:24]
    info_path.write_bytes(plistlib.dumps(info))
    subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(private)], check=True, capture_output=True)
    home = output/'home'
    (home/'Library/Application Support').mkdir(parents=True, exist_ok=False)
    return private/'Contents/MacOS/Snes9x', home.resolve()


def execute_suite(args, suite):
    suite_pin = digest(args.suite)
    verify_inputs(args.suite, suite)
    running = subprocess.run(['pgrep', '-x', 'Snes9x'], capture_output=True, text=True)
    if running.returncode == 0:
        raise ValueError('Quit Snes9x yourself before running the stress suite; no app was touched')
    if running.returncode != 1:
        raise ValueError('could not check for competing emulator')
    app = args.app.expanduser().resolve()
    executable = app/'Contents/MacOS/Snes9x' if app.suffix == '.app' else app
    if not executable.is_file():
        raise ValueError('missing app executable')
    bundle_contents = app/'Contents' if app.suffix == '.app' else executable.parent.parent
    artifacts = [executable]
    artifacts += [bundle_contents/'Frameworks/snes9x_framework.framework/Versions/A'/name for name in ('snes9x_framework', 'Resources/default.metallib')]
    app_pins = {str(p): digest(p) for p in artifacts}
    args.output.mkdir(parents=True, exist_ok=False)
    executable, home = isolate_application(app, args.output)
    environment = clean_environment()
    environment.update(HOME=str(home), CFFIXED_USER_HOME=str(home))
    write_new(args.output/'launch.json', dict(artifacts=app_pins, suite_sha256=suite_pin,
                                            isolated_executable=str(executable), isolated_home=str(home)))
    collected = {w['id']: [] for w in workloads(suite)}
    # Rotate workload order across repetitions to avoid always timing the most
    # expensive scene at a particular temperature. Never run GPU jobs in parallel.
    all_work = list(workloads(suite))
    for repeat in range(args.repeats):
        order = all_work[repeat % len(all_work):]+all_work[:repeat % len(all_work)]
        for w in order:
            if digest(args.suite) != suite_pin:
                raise ValueError('manifest changed during suite')
            verify_inputs(args.suite, suite)
            if {str(p): digest(p) for p in artifacts} != app_pins:
                raise ValueError('app changed during suite')
            s = w['scenario'];route, frames = route_text(s['route'])
            directory = args.output/f'{w["id"]}-run{repeat+1}'
            cmd = [str(executable), '--remaster-benchmark']
            options = dict(rom=fixture_path(args.suite, s['rom']), state=fixture_path(args.suite, s['state']),
                           profile=fixture_path(args.suite, s['profile']), output=directory,
                           frames=frames, **{'warmup-frames': s.get('warmup_frames', 30)}, route=route,
                           representatives=w['representatives'], **{'offscreen-representatives': w['offscreen_representatives']},
                           presentation=w['presentation'])
            for key, value in options.items():
                cmd += ['--benchmark-'+key, str(value)]
            print(w['id'], f'run {repeat+1}/{args.repeats}', flush=True)
            with (args.output/(directory.name+'.log')).open('w') as log:
                subprocess.run(cmd, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout, check=True)
            verify_inputs(args.suite, suite)
            collected[w['id']].append(validate_report(read(directory/'report.json'), w))
    if {str(p): digest(p) for p in artifacts} != app_pins:
        raise ValueError('app changed during suite')
    if digest(args.suite) != suite_pin:
        raise ValueError('manifest changed during suite')
    return dict(schema=1, suite_sha256=suite_pin, artifacts=app_pins,
                workloads={key: summarize_runs(runs) for key, runs in collected.items()})


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    pin = sub.add_parser('pin', help='Create NEW local manifest with fixture hashes; does not launch app')
    pin.add_argument('suite', type=Path);pin.add_argument('--output', type=Path, required=True)
    plan = sub.add_parser('plan', help='Validate pins and print expanded scenario matrix; no app launch')
    plan.add_argument('suite', type=Path)
    for command in ('record', 'check'):
        q = sub.add_parser(command)
        q.add_argument('suite', type=Path);q.add_argument('--baseline', type=Path, required=True)
        q.add_argument('--output', type=Path, required=True);q.add_argument('--app', type=Path, default=ROOT/'build/release/Snes9x.app')
        q.add_argument('--repeats', type=int, default=2);q.add_argument('--timeout', type=float, default=1800)
        q.add_argument('--relative-percent', type=float, default=15);q.add_argument('--absolute-ms', type=float, default=.5)
    q = sub.add_parser('compare', help='Compare collected summaries without running game')
    q.add_argument('--baseline', type=Path, required=True);q.add_argument('--candidate', type=Path, required=True)
    q.add_argument('--output', type=Path, required=True)
    q.add_argument('--relative-percent', type=float, default=15);q.add_argument('--absolute-ms', type=float, default=.5)
    args = p.parse_args(argv)
    try:
        if args.command == 'pin':
            suite = load_suite(args.suite, False)
            for s in suite['scenarios']:
                for key in ('rom', 'state', 'profile'):
                    path = fixture_path(args.suite, s[key]);s[key] = dict(path=str(path), sha256=digest(path))
                    if key == 'profile':
                        s[key]['companions'] = profile_companions(path)
            write_new(args.output, suite);return 0
        if args.command == 'plan':
            suite = load_suite(args.suite);verify_inputs(args.suite, suite)
            for w in workloads(suite):
                print(w['id'], route_text(w['scenario']['route'])[1], 'frames')
            return 0
        if not 0 < args.relative_percent < 100 or not math.isfinite(args.absolute_ms) or args.absolute_ms < 0:
            raise ValueError('invalid noise thresholds')
        if args.command == 'compare':
            base, current = read(args.baseline), read(args.candidate)
            checks = compare(base, current, args.relative_percent, args.absolute_ms)
            write_new(args.output, dict(passed=not any(c['regression'] for c in checks), checks=checks))
        else:
            integer(args.repeats, 2, 100, 'repeats')
            if not math.isfinite(args.timeout) or args.timeout <= 0:
                raise ValueError('invalid timeout')
            if args.command == 'record' and args.baseline.exists():
                raise ValueError('baseline exists; deliberate rebaseline needs a new path')
            base = read(args.baseline) if args.command == 'check' else None
            if base is not None:
                validate_summary(base)
            suite = load_suite(args.suite)
            if base is not None and (base.get('schema') != 1 or base.get('suite_sha256') != digest(args.suite) or
                                     set(base.get('workloads', {})) != {w['id'] for w in workloads(suite)}):
                raise ValueError('baseline does not match pinned scenario matrix')
            current = execute_suite(args, suite)
            write_new(args.output/'summary.json', current)
            if args.command == 'record':
                write_new(args.baseline, current);print('Recorded baseline:', args.baseline);return 0
            checks = compare(base, current, args.relative_percent, args.absolute_ms)
            write_new(args.output/'comparison.json', dict(passed=not any(c['regression'] for c in checks), checks=checks))
        failed = [c for c in checks if c['regression']]
        for c in failed:
            print('REGRESSION', c['workload'], c['metric'], c['stat'], c['before'], '->', c['after'])
        print(len(failed), 'regressions /', len(checks), 'checks')
        return 1 if failed else 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print('Stress suite invalid:', error, file=sys.stderr);return 2


if __name__ == '__main__':
    sys.exit(main())
