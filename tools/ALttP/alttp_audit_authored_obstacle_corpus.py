#!/usr/bin/env python3
"""Replay immutable supported packet corpus with a temporary reusable example.

Source profiles remain per-capture; only schema22 footprint records are appended
to separate research copies. Rejected packet evidence remains in denominators.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tomllib
from alttp_authored_obstacles import audit
from alttp_room_geometry import RoomSnapshot
from alttp_collision_reachability import read_attributes


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--example', type=Path, required=True)
    p.add_argument('--solver', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if not a.output.is_dir(): p.error('existing output directory required')
    group_text = a.example.read_text().split('\n[[obstacle_groups]]', 1)[1]
    group_text = '\n[[obstacle_groups]]'+group_text
    manifest = json.loads(a.manifest.read_text())
    records = []
    for r in manifest['records']:
        record = dict(capture=r['capture'], status=r['status'], approval=False)
        records.append(record)
        if r['status'] != 'replayed':
            record['reason'] = r.get('log', 'missing complete sidecars'); continue
        cap = Path(r['capture']); directory = a.output/cap.stem; directory.mkdir()
        pins = {f: hashlib.sha256(Path(f).read_bytes()).hexdigest() for f in r['sha256']}
        source = Path(str(cap)+'.profile.toml'); raw_profile = source.read_bytes()
        research = directory/'research.toml'
        research.write_text(re.sub(r'^schema_version = \d+', 'schema_version = 22', raw_profile.decode(), count=1)+group_text)
        cmd = [sys.executable, '-B', str(Path(__file__).with_name('alttp_replay_live_height_evidence.py')),
               str(cap), '--solver', str(a.solver), '--rom', str(a.rom), '--research-profile', str(research), '--output', str(directory)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode:
            record.update(status='research_refused', reason=result.stderr); continue
        sol = json.loads((directory/'live-runtime.json').read_text())
        snap = RoomSnapshot.read(directory/'live-solver-input.raw')
        proof = audit(snap, read_attributes((directory/'live-solver-input.raw.attr').read_bytes(), snap.room), tomllib.loads(research.read_text()), sol)
        (directory/'independent-proof.json').write_text(json.dumps(proof, indent=2)+'\n')
        record.update(room=snap.room, status='research_replayed', source_profile_sha256=hashlib.sha256(raw_profile).hexdigest(),
                      source_profile_immutable=source.read_bytes() == raw_profile,
                      all_capture_inputs_immutable=all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == h for f, h in pins.items()),
                      proof={k: proof[k] for k in ('success', 'required_cells', 'solved_cells', 'missing_cells', 'conflicting_cells', 'per_layer')},
                      mismatch_count=len(proof['mismatches']), support_required=sol['required_planes'], support_missing=sol['missing_planes'],
                      unknown_walls=sol['unknown_wall_cells'], structural_conflicts=sol['structural_conflicts'])
    summary = dict(approval=False, packets=len(records), research_replayed=sum(r['status']=='research_replayed' for r in records),
                   unsupported=sum(r['status']=='unsupported_missing_evidence' for r in records),
                   refused=sum(r['status'] in ('refused', 'research_refused') for r in records),
                   example_sha256=hashlib.sha256(a.example.read_bytes()).hexdigest(), records=records)
    for k in ('required_cells', 'solved_cells', 'missing_cells', 'conflicting_cells'):
        summary[k] = sum(r.get('proof', {}).get(k, 0) for r in records)
    summary['mismatches'] = sum(r.get('mismatch_count', 0) for r in records)
    summary['all_capture_inputs_immutable'] = all(r.get('all_capture_inputs_immutable', True) for r in records)
    (a.output/'corpus.json').write_text(json.dumps(summary, indent=2)+'\n')
    print({k:v for k,v in summary.items() if k != 'records'})
    return int(bool(summary['mismatches']))


if __name__ == '__main__': sys.exit(main())
