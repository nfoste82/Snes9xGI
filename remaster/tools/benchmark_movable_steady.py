#!/usr/bin/env python3
"""Pinned production EndFrame replay; controlled byte phases, NOT live timing.

Each flag's cached output is compared only with its own uncached semantics.
Replacement sidecar and absent optional capabilities are pinned before/after
every subprocess. No user input/profile/state file is ever written.
"""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import subprocess
from benchmark_cold_doors import input_pins, verify_pins


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', required=True)
    p.add_argument('--after', required=True)
    p.add_argument('--oracle', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--samples', type=int, default=36)
    p.add_argument('--processes', type=int, default=1, help='independent runs per binary/control')
    p.add_argument('--parity', action='store_true')
    p.add_argument('--trace', action='store_true')
    p.add_argument('capture', type=Path)
    a = p.parse_args()
    if a.samples < 8:
        p.error('--samples must be >= 8 (cold, irrelevant A/B/A, and resident return)')
    if a.processes < 1:
        p.error('--processes must be positive')
    a.output.mkdir(parents=True, exist_ok=True)
    binaries = [a.before, a.after, a.oracle]
    pins = input_pins(binaries, [a.capture])
    (a.output / 'pins.json').write_text(json.dumps(pins, indent=2))
    summary = {'scope': 'controlled byte-phase ablation, no live animation claim'}
    for off in (False, True):
        mode = 'private-off' if off else 'active'
        for label, binary in zip(('before', 'after', 'oracle'), binaries):
            prefix = a.output / f'{mode}-{label}'
            env = dict(os.environ, ALTTP_MOVABLE_STEADY_REGRESSION='1',
                       ALTTP_MOVABLE_SAMPLES=str(a.samples))
            if label == 'before':
                env['ALTTP_MOVABLE_RECORD_BASELINE'] = '1'
            if off:
                env['ALTTP_MOVABLE_PRIVATE_OFF'] = '1'
            if not a.parity:
                env['ALTTP_CLASSIFICATION_TIMING_ONLY'] = '1'
            if a.trace:
                env['S9X_REMASTER_LIGHTING_TRACE'] = '1'
            else:
                env.pop('S9X_REMASTER_LIGHTING_TRACE', None)
            groups = {'cold': [], 'warm': [], 'resident': []}
            for process in range(a.processes):
                run_prefix = Path(str(prefix) + f'-{process}')
                verify_pins(pins, binaries, [a.capture])
                with Path(str(run_prefix) + '.log').open('w') as log:
                    subprocess.run([binary, str(a.capture), str(run_prefix)], env=env,
                                   stdout=log, stderr=log, check=True)
                verify_pins(pins, binaries, [a.capture])
                rows = list(csv.DictReader(Path(str(run_prefix) + '-steady.csv').open()))
                groups['cold'].extend(rows[:1])
                groups['warm'].extend(rows[1:4])
                groups['resident'].extend(rows[7:])
                if a.parity:
                    import filecmp
                    oracle = a.output / f'{mode}-before-{process}-steady.bin'
                    if label != 'before' and not filecmp.cmp(Path(str(run_prefix) + '-steady.bin'), oracle, shallow=False):
                        raise RuntimeError(f'{mode}/{process}: {label}/baseline exact outputs differ')
            def stats(r):
                values = sorted(float(x['ms']) for x in r)
                return dict(n=len(r), p50=values[math.ceil(len(r)*.50)-1],
                            p95=values[math.ceil(len(r)*.95)-1], max=values[-1],
                            classifications=sum(int(x['classifications']) for x in r),
                            solves=sum(int(x['solves']) for x in r),
                            door_origins=sum(int(x['door_origins']) for x in r))
            summary[f'{mode}-{label}'] = {name: stats(rows) for name, rows in groups.items()}
        if a.parity:
            summary[mode + '-exact-parity'] = True
        (a.output / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
