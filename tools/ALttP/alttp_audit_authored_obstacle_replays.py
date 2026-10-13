#!/usr/bin/env python3
"""Immutable own-input corpus replay using the production cache capture probe.

This reports reproducibility, not room approval; missing sidecars, unsettled
states, profile pins and stale map/camera failures remain explicit denominators.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--captures', type=Path, required=True)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if not a.output.is_dir():
        p.error('output directory must exist')
    records = []
    for capture in sorted(a.captures.glob('frame_room*.s9xrmf')):
        paths = [Path(str(capture) + s) for s in
                 ('', '.profile.toml', '.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin')]
        missing = [str(f) for f in paths if not f.exists()]
        before = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in paths if f.exists()}
        r = dict(capture=str(capture), approval=False, missing=missing, sha256=before)
        if missing:
            r['status'] = 'unsupported_missing_evidence'
        else:
            prefix = a.output / ('replay-' + capture.stem)
            result = subprocess.run([str(a.probe), str(capture), str(prefix)], capture_output=True, text=True)
            r.update(status='refused' if result.returncode else 'replayed',
                     exit=result.returncode, log=result.stdout + result.stderr)
            if not result.returncode:
                r['result'] = json.loads(prefix.with_suffix('.json').read_text())
        r['inputs_immutable'] = all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == h for f, h in before.items())
        records.append(r)
    report = dict(approval=False, packets=len(records), records=records,
                  replayed=sum(r['status'] == 'replayed' for r in records),
                  unsupported=sum(r['status'] == 'unsupported_missing_evidence' for r in records),
                  refused=sum(r['status'] == 'refused' for r in records),
                  all_inputs_immutable=all(r['inputs_immutable'] for r in records))
    (a.output / 'authored-obstacle-replay-corpus.json').write_text(json.dumps(report, indent=2) + '\n')
    print({k: v for k, v in report.items() if k != 'records'})
    return int(not report['all_inputs_immutable'])


if __name__ == '__main__':
    raise SystemExit(main())
