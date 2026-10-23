#!/usr/bin/env python3
"""Replay each complete packet with its OWN metadata and audit every obstacle.

Incomplete packets remain an explicit unsupported denominator. No source writes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--captures', type=Path, required=True)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if not a.output.is_dir():
        p.error('output must exist')
    records = []
    for capture in sorted(a.captures.glob('frame_room*.s9xrmf')):
        paths = {s: Path(str(capture)+s) for s in
                 ('', '.profile.toml', '.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin')}
        missing = [s for s, path in paths.items() if not path.exists()]
        r = dict(capture=str(capture), approval=False, missing=missing,
                 sha256={s: sha(path) for s, path in paths.items() if path.exists()})
        if missing:
            r['status'] = 'unsupported_missing_evidence'
        else:
            prefix = a.output / capture.stem
            result = subprocess.run([str(a.probe), str(capture), str(prefix)], capture_output=True, text=True)
            r.update(status='refused' if result.returncode else 'replayed', log=result.stdout+result.stderr)
            if not result.returncode:
                d = json.loads(prefix.with_suffix('.json').read_text())
                r['room'] = d['room']
                r['layers'] = [dict(bg=l['bg'], **l['obstacle_invariance'],
                                    required_planes=l['required_planes'], missing_planes=l['missing_planes'],
                                    conflicts=l['conflicts'], unknown_walls=l['unknown_walls']) for l in d['layers']]
        records.append(r)
    report = dict(approval=False, packets=len(records),
                  replayed=sum(r['status']=='replayed' for r in records), records=records)
    for key in ('placed', 'unresolved_base', 'canonical_buffer_samples', 'changed_local_samples'):
        report[key] = sum(l[key] for r in records for l in r.get('layers', []))
    (a.output/'corpus.json').write_text(json.dumps(report, indent=2)+'\n')
    print({k:v for k,v in report.items() if k!='records'})

if __name__ == '__main__':
    main()
