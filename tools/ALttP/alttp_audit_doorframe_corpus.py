#!/usr/bin/env python3
"""Research replay every available complete live room packet with pinned metadata.

Never approval: capture states are independently pinned but metadata is newer.
Refused/unsettled packets and missing complete inputs remain explicit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--captures', type=Path, required=True)
    p.add_argument('--solver', type=Path, required=True)
    p.add_argument('--profile', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if not a.output.is_dir():
        p.error('output must exist')
    data = a.profile.read_bytes()
    pinned = a.output / 'corpus-profile.toml'
    pinned.write_bytes(data)
    records = []
    for capture in sorted(a.captures.glob('frame_room*.s9xrmf')):
        prefix = str(capture)
        record = dict(capture=prefix, research_replay=True, approval=False)
        missing = [s for s in ('.dungeon.json', '.dungeon-input.bin', '.profile.toml') if not Path(prefix+s).exists()]
        if missing:
            record.update(status='unsupported_missing_complete_packet', missing=missing)
        else:
            out = a.output / capture.stem
            out.mkdir(exist_ok=True)
            result = subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('alttp_replay_live_height_evidence.py')),
                prefix, '--solver', str(a.solver), '--output', str(out), '--rom', str(a.rom),
                '--research-profile', str(pinned)], capture_output=True, text=True)
            if result.returncode:
                record.update(status='refused_inputs', error=result.stderr)
            else:
                d = json.loads((out/'live-runtime.json').read_text())
                record.update(status='replayed', room=d['room_id'], frame_regions=len(d['doorframe_attachments']),
                    unknown_bg2_frames=d['unknown_doorframe_cells'], conflicts_bg2_frames=d['conflicting_doorframe_cells'],
                    required_planes=d['required_planes'], missing_planes=d['missing_planes'],
                    unknown_walls=d['unknown_wall_cells'], structural_conflicts=d['structural_conflicts'])
        records.append(record)
    report = dict(profile_sha256=hashlib.sha256(data).hexdigest(), packet_denominator=len(records),
                  replayed=sum(r['status']=='replayed' for r in records), records=records, approval=False)
    (a.output/'corpus.json').write_text(json.dumps(report, indent=2)+'\n')
    print('packets',len(records),'replayed',report['replayed'],'unsupported/refused',len(records)-report['replayed'])


if __name__ == '__main__':
    main()
