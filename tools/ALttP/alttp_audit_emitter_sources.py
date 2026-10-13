#!/usr/bin/env python3
"""Audit own-profile completed-room captures; never fabricate source bases."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--captures', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path,
                        help='existing review directory for packets, inventories and report')
    args = parser.parse_args()
    if not args.output.is_dir():
        parser.error('--output must be an existing review directory')
    results = []
    for sidecar in sorted(args.captures.glob('*.s9xrmf.dungeon.json')):
        capture = Path(str(sidecar).removesuffix('.dungeon.json'))
        state = json.loads(sidecar.read_text())
        record = {'capture': str(capture), 'room': state['room'],
                  'profile_pin': state.get('profile_serialized_fnv1a64'),
                  'plane': state['plane']}
        missing = [str(capture) + suffix for suffix in
                   ('', '.dungeon-input.bin', '.dungeon-templates.bin', '.profile.toml')
                   if not Path(str(capture) + suffix).is_file()]
        if missing:
            record.update(status='unsupported_missing_inputs', missing=missing)
        elif state.get('submodule', 0) or state.get('collision_mode', 0):
            record.update(status='unsupported_state')
        else:
            result = subprocess.run([str(args.probe), str(capture),
                                     str(args.output / capture.name)],
                                    text=True, capture_output=True)
            record.update(exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
            match = re.search(r'effective emitting=(\d+) unknown=(\d+) support_only=(\d+) structural_only=(\d+)', result.stdout)
            record['status'] = 'audited' if result.returncode == 0 and match else 'rejected'
            if match:
                record.update(zip(('emitting', 'unknown', 'support_only', 'structural_only'),
                                  map(int, match.groups())))
                report = json.loads(Path(str(args.output / capture.name)+'.json').read_text())
                record['structural_conflicts'] = sum(v['conflicts'] for v in report['layers'])
                record['unknown_walls'] = sum(v['unknown_walls'] for v in report['layers'])
                record['missing_support'] = sum(sum(v['missing']) for v in report['layers'])
        results.append(record)
    counts = {key: sum(r['status'] == key for r in results) for key in
              ('audited', 'unsupported_missing_inputs', 'unsupported_state', 'rejected')}
    summary = {'captures': len(results), 'rooms': sorted({r['room'] for r in results}),
               **counts, **{key: sum(r.get(key, 0) for r in results if r['status'] == 'audited')
                            for key in ('emitting', 'unknown', 'support_only', 'structural_only',
                                        'structural_conflicts', 'unknown_walls', 'missing_support')}}
    (args.output / 'emitter-source-audit.json').write_text(
        json.dumps({'summary': summary, 'results': results}, indent=2) + '\n')
    print(json.dumps(summary, sort_keys=True))


if __name__ == '__main__':
    main()
