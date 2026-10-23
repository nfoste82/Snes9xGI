"""Compare pinned complete-room migration audits without declaring room approval."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();records=[]
    for room in ('055','060','061'):
        records_by_phase={}
        for phase,directory in (('before',a.before),('after',a.after)):
            runtime=json.loads((directory/f'room-{room}-runtime.json').read_text())
            validation=json.loads((directory/f'room-{room}-validation-v2.json').read_text())
            records_by_phase[phase]=(runtime,validation)
        old,new=records_by_phase['before'],records_by_phase['after']
        # Only the pinned profile pathname differs between review directories.
        # Keep every other input digest and all coverage/geometry evidence exact.
        profiles_equal=(a.before/'fixture-input-profile.toml').read_bytes()==(a.after/'fixture-input-profile.toml').read_bytes()
        old[1]['inputs']['profile']=new[1]['inputs']['profile']
        v=new[1]
        records.append(dict(room=room,runtime_equal=old[0]==new[0],validation_equal=profiles_equal and old[1]==new[1],
                            support=[v['valid_walkable_placements'],v['required_walkable_placements']],
                            plane_denominators=v['per_plane_walkable_denominator'],
                            walls=[v['solved_wall_cells'],v['unknown_wall_cells']],
                            conflicts=len(v['conflicting_components']),validator_success=v['success']))
    unchanged=all(r['runtime_equal'] and r['validation_equal'] for r in records)
    report=dict(scope='three pinned complete default-state room fixtures; no exhaustive state corpus or gameplay comparison',
                profile_sha256=hashlib.sha256((a.before/'fixture-input-profile.toml').read_bytes()).hexdigest(),
                comparison_policy='ignore only identical pinned-profile path relocation',
                rooms=3,unchanged=unchanged,approval=False,records=records)
    a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
    return int(not unchanged)


if __name__=='__main__':raise SystemExit(main())
