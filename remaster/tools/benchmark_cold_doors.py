#!/usr/bin/env python3
"""Bounded independent-process cold-build timing/parity driver (no GUI/state writes)."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess


REQUIRED_SIDECARS = ('', '.dungeon-input.bin', '.dungeon-templates.bin',
                     '.dungeon.json', '.profile.toml')
OPTIONAL_SIDECARS = ('.terrain-input.bin', '.sprite-input.bin', '.replacement-input.bin')


def input_pins(binaries, captures):
    pins = {}
    for path in list(map(Path, binaries)) + [Path(str(c) + s) for c in captures for s in REQUIRED_SIDECARS]:
        pins[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    for c in captures:
        for suffix in OPTIONAL_SIDECARS:
            path = Path(str(c) + suffix)
            # Null is an explicit absence pin, not a skipped input. Use exists
            # rather than is_file so directories/unreadable files fail closed.
            pins[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
    return pins


def verify_pins(expected, binaries, captures):
    actual = input_pins(binaries, captures)
    if expected != actual:
        changed = [path for path in expected if expected[path] != actual.get(path)]
        raise RuntimeError('benchmark input changed: ' + ', '.join(changed))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--before', required=True)
    p.add_argument('--after', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--samples', type=int, default=100)
    p.add_argument('--processes', type=int, default=0)
    p.add_argument('--parity', action='store_true')
    p.add_argument('--trace', action='store_true')
    p.add_argument('captures', nargs='+', type=Path)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    binaries = [a.before, a.after]
    pins = input_pins(binaries, a.captures)
    (a.output / 'pins.json').write_text(json.dumps(pins, indent=2))
    summary = {}
    for c in a.captures:
        rows = {'before': [], 'after': []}
        # Alternate binary order between independent invocations.
        for proc in range(a.processes or 1):
            paths = {}
            for label, binary in ([('before', a.before), ('after', a.after)] if proc % 2 == 0 else [('after', a.after), ('before', a.before)]):
                prefix = a.output / f'{c.stem}-{proc}-{label}'
                env = dict(os.environ, ALTTP_CLASSIFICATION_ALTERNATE=str(c),
                           ALTTP_CLASSIFICATION_SAMPLES=str(4 if a.processes else a.samples))
                if not a.parity:
                    env['ALTTP_CLASSIFICATION_TIMING_ONLY'] = '1'
                if a.trace:
                    env['S9X_REMASTER_LIGHTING_TRACE'] = '1'
                else:
                    env.pop('S9X_REMASTER_LIGHTING_TRACE', None)
                verify_pins(pins, binaries, a.captures)
                with prefix.with_suffix('.log').open('w') as log:
                    subprocess.run([binary, str(c), str(prefix)], env=env, stdout=log, stderr=log, check=True)
                verify_pins(pins, binaries, a.captures)
                with Path(str(prefix) + '-timing.csv').open() as f:
                    r = list(csv.DictReader(f))
                rows[label].extend(r[:1] if a.processes else r[2:])
                paths[label] = Path(str(prefix) + '-alternating.bin')
            if a.parity:
                if paths['before'].read_bytes() != paths['after'].read_bytes():
                    raise RuntimeError(f'complete output mismatch: {c}')
        summary[c.name] = {}
        for label, r in rows.items():
            v = sorted(float(x['endframe_ms']) for x in r)
            summary[c.name][label] = dict(n=len(v), p50=v[math.ceil(len(v)*.50)-1],
                                        p95=v[math.ceil(len(v)*.95)-1], p99=v[math.ceil(len(v)*.99)-1], max=v[-1],
                                        first_work={k: r[0][k] for k in r[0] if k not in ('sample', 'endframe_ms')})
        (a.output / 'summary.json').write_text(json.dumps(summary, indent=2))
        print(c.name, json.dumps(summary[c.name]), flush=True)


if __name__ == '__main__':
    main()
