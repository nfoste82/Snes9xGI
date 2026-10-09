"""Pin available room062 evidence without overwriting sources or existing inputs."""
import argparse
import hashlib
import json
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--captures',type=Path,required=True)
    p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if not a.output.is_dir():p.error('output must exist')
    records=[]
    for capture in sorted(a.captures.glob('frame_room062*.s9xrmf')):
        r=dict(capture=str(capture),files=[],missing=[],approval=False)
        for suffix in ('','.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin','.profile.toml'):
            source=Path(str(capture)+suffix)
            if not source.exists():r['missing'].append(suffix);continue
            blob=source.read_bytes();target=a.output/source.name
            with target.open('xb') as f:f.write(blob)
            r['files'].append(dict(source=str(source),pinned=str(target),bytes=len(blob),
                sha256=hashlib.sha256(blob).hexdigest(),mtime_ns=source.stat().st_mtime_ns))
        records.append(r)
    blob=a.profile.read_bytes();target=a.output/'research-profile.toml'
    with target.open('xb') as f:f.write(blob)
    report=dict(records=records,packet_denominator=len(records),
        complete=sum(not r['missing'] for r in records),
        research_profile_sha256=hashlib.sha256(blob).hexdigest(),approval=False)
    with (a.output/'inputs.json').open('x') as f:f.write(json.dumps(report,indent=2)+'\n')
    print({k:v for k,v in report.items() if k!='records'})

if __name__=='__main__':main()
