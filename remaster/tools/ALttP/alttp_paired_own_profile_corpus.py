#!/usr/bin/env python3
"""Paired own-profile complete capture replay; never movement/room approval."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--captures', type=Path, required=True)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--after', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    records = []
    keys = ('required_planes', 'missing_planes', 'structural_conflicts',
            'solved_wall_cells', 'unknown_wall_cells', 'configured_stair_fallbacks',
            'stair_attachments', 'doorframe_attachments', 'unknown_doorframe_cells')
    for capture in sorted(a.captures.glob('frame_room*.s9xrmf')):
        record = {'capture': str(capture), 'approval': False}
        missing = [suffix for suffix in ('.dungeon.json', '.dungeon-input.bin',
                   '.profile.toml', '.dungeon-templates.bin')
                   if not Path(str(capture)+suffix).is_file()]
        if missing:
            record.update(status='rejected_missing', missing=missing)
            records.append(record)
            continue
        state = json.loads(Path(str(capture)+'.dungeon.json').read_text())
        profile = Path(str(capture)+'.profile.toml').read_bytes()
        record.update(room=state.get('room'), state=state,
                      profile_sha256=hashlib.sha256(profile).hexdigest(),
                      capture_sha256=hashlib.sha256(capture.read_bytes()).hexdigest())
        for side in ('before', 'after'):
            out = a.output / capture.stem / side
            out.mkdir(parents=True, exist_ok=True)
            command = [sys.executable, '-B', str(Path(__file__).with_name(
                'alttp_replay_live_height_evidence.py')), str(capture), '--solver',
                str(getattr(a, side)), '--rom', str(a.rom), '--output', str(out)]
            result = subprocess.run(command, capture_output=True, text=True)
            (out/'command.json').write_text(json.dumps(command)+'\n')
            (out/'replay.log').write_text(result.stdout+result.stderr)
            record[side] = {'exit': result.returncode}
            if result.returncode == 0:
                data = json.loads((out/'live-runtime.json').read_text())
                record[side].update({key: data.get(key) for key in keys})
        record['status'] = 'valid' if all(record[s]['exit'] == 0 for s in ('before', 'after')) else 'rejected_replay'
        records.append(record)
    valid = [r for r in records if r['status'] == 'valid']
    summary = dict(discovered=len(records), valid=len(valid), rejected=len(records)-len(valid),
                   rooms=sorted({r['room'] for r in valid}),
                   profiles=len({r['profile_sha256'] for r in valid}),
                   changed=sum(r['before'] != r['after'] for r in valid), approval=False,
                   scope='own-profile settled WRAM/PPU replay; physical solver only, not visual scene or complete movement coverage')
    (a.output/'corpus.json').write_text(json.dumps(dict(summary=summary, records=records), indent=2)+'\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
