#!/usr/bin/env python3
"""Verify an applied discovery plan and print compact evidence for handoff."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import tomllib

from alttp_diagonal_sets import REFERENCES, transfer


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report',type=Path,required=True)
    p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--baseline',type=Path,required=True)
    a = p.parse_args()
    report = json.loads(a.report.read_text())
    current,baseline = a.profile.read_text(),a.baseline.read_text()
    updates = report['proposed_updates']
    assets = {v['tile_hash']:v for v in tomllib.loads(current)['assets']}
    originals = {v['tile_hash']:v for v in tomllib.loads(baseline)['assets']}
    assert hashlib.sha256(baseline.encode()).hexdigest() == report['profile_sha256']
    for key in REFERENCES:
        assert assets[key] == originals[key],key
    for key in updates:
        band = report['decisions'][key]['bands'][0]
        reference = REFERENCES[('cap','upper','middle','foot').index(band)]
        expected = transfer(originals[reference])
        assert all(assets[key][f] == value for f,value in expected.items()),key
        assert ('occlusion' in assets[key]) == ('occlusion' in originals[reference]),key
    def strip(match):
        key = re.search(r'(?m)^tile_hash\s*=\s*"([^"]+)"',match.group())
        if key and key[1] in updates and key[1] in originals:
            block=match.group()
            # Existing role/settings survive; only previously absent fields
            # authorized by this discovery are removed for byte comparison.
            for field in updates[key[1]]:
                if field not in originals[key[1]]:
                    block=re.sub(rf'(?m)^{field}\s*=.*\n','',block)
            return block
        return '' if key and key[1] in updates else match.group()
    stripped = re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',strip,current)
    assert stripped == baseline,'unrelated profile bytes changed'
    print('Exact reference/unrelated-byte preservation PASS')
    print('Profile SHA256',hashlib.sha256(current.encode()).hexdigest())
    print('Groups',len(report['groups']),dict(Counter(g['status'] for g in report['groups'])))
    print('Assemblies',dict(Counter(r['family'] for r in report['assemblies'])))
    print('Proved placed bands',sum(len(r['placements']) for r in report['assemblies']))
    print('Recovered priority-only placements',[
        dict(room=r['room'],entrance=r['entrance'],underlying_writer=r['writer'],**p)
        for r in report['assemblies'] for p in r['placements'] if p.get('ownership_operation')])
    print('Omitted',dict(sum((Counter(r['omitted']) for r in report['assemblies']),Counter())))
    print('New authored usage counts',dict(sum((Counter(report['decisions'][k]['usages']) for k in updates),Counter())))
    print('Unsupported',[(r['room'],r.get('error')) for r in report['rooms'] if r['status']!='scanned'])
    for group in report['groups']:
        if group['status']=='verified_bands':
            print('Group',group['ids'],'rooms',sorted({r['room'] for r in group['contexts']}))


if __name__ == '__main__':
    main()
