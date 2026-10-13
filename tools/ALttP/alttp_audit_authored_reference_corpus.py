"""Audit available reference contexts with one pinned temporary group profile."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('manifest','profile','rom','probe','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args(); records=[]
    profile_bytes=a.profile.read_bytes()
    corpus=json.loads(a.manifest.read_text())
    for r in corpus['records']:
        raw=Path(r['snapshot']); attr=Path(str(raw)+'.attr'); state_path=Path(str(raw)+'.state.json')
        if not all(f.exists() for f in (raw,attr,state_path)):
            records.append(dict(snapshot=str(raw),status='unsupported_missing_inputs'));continue
        state=json.loads(state_path.read_text())
        command=[str(a.probe),str(raw),str(attr),str(a.profile),str(a.rom),*map(str,state['link_xy']),str(state['plane'])]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode:records.append(dict(snapshot=str(raw),status='refused',reason=result.stderr));continue
        record=json.loads(result.stdout);record.update(snapshot=str(raw),status='checked');records.append(record)
    summary=dict(contexts=len(records),rooms=len({r['room'] for r in records if 'room' in r}),
                 checked=sum(r['status']=='checked' for r in records),unsupported=sum(r['status']=='unsupported_missing_inputs' for r in records),
                 refused=sum(r['status']=='refused' for r in records),profile_sha256=hashlib.sha256(profile_bytes).hexdigest(),
                 profile_immutable=a.profile.read_bytes()==profile_bytes,approval=False,records=records)
    for key in ('required','solved','unmatched','contacts','witnessed','conflicts'):
        summary[key]=sum(r.get('original_authored_'+key,0) for r in records)
    summary['complete_cache_full_equal']=all(r.get('original_authored_cache_full_equal') and r.get('complete_maps_diagnostics_packet_equal') for r in records if r['status']=='checked')
    a.output.write_text(json.dumps(summary,indent=2)+'\n');print({k:v for k,v in summary.items() if k!='records'})
    return int(summary['refused'] or not summary['profile_immutable'])


if __name__=='__main__':raise SystemExit(main())
