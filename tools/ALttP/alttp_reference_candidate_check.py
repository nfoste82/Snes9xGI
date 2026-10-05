#!/usr/bin/env python3
"""Verify source/candidate normal arrays AND their exact TOML field text."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tomllib


def check(source, candidate):
    a, b = source.read_text(), candidate.read_text()
    assets_a = {v['tile_hash']:v for v in tomllib.loads(a)['assets']}
    assets_b = {v['tile_hash']:v for v in tomllib.loads(b)['assets']}
    def literals(text):
        result = {}
        for block in text.split('[[assets]]')[1:]:
            key = re.search(r'^tile_hash\s*=\s*"([^"]+)"',block,re.M).group(1)
            field = re.search(r'^normal_xyz\s*=\s*\[[^\]]*\]',block,re.M)
            if field:
                result[key] = field.group()
        return result
    literal_a, literal_b = literals(a), literals(b)
    changed = [key for key,asset in assets_a.items() if 'normal_xyz' in asset and
               asset['normal_xyz'] != assets_b.get(key,{}).get('normal_xyz')]
    text_changed = [key for key,value in literal_a.items() if value != literal_b.get(key)]
    return {'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'candidate_sha256':hashlib.sha256(candidate.read_bytes()).hexdigest(),
            'candidate_height_arrays_sha256': hashlib.sha256(json.dumps(
                {key:asset.get('height') for key,asset in assets_b.items()},
                sort_keys=True,separators=(',',':')).encode()).hexdigest(),
            'existing_normals':len(literal_a), 'changed_arrays':changed,
            'changed_field_text':text_changed, 'passed':not changed and not text_changed}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--report',type=Path)
    args = p.parse_args()
    if args.report and args.report.resolve() in {args.source.resolve(),args.candidate.resolve()}:
        p.error('report must be distinct from inputs')
    result = check(args.source,args.candidate)
    if args.report:
        args.report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result['passed'] else 1)
