#!/usr/bin/env python3
"""Exact authorized-field merge; current settings and all other normals win."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tomllib

AUTHORIZED = {
    'v1:4bpp:503ab068dfa941d3': ('normal_xyz',),
    'v1:4bpp:31aba23efb31666b': ('normal_xyz',),
    'v1:4bpp:a4aa6ebd24fefeb7': ('normal_xyz', 'height'),
    'v1:4bpp:1a032ad7756c49db': ('height',),
}


def merge(current, backup):
    a, b = tomllib.loads(current), tomllib.loads(backup)
    aa = {v['tile_hash']: v for v in a['assets']}
    bb = {v['tile_hash']: v for v in b['assets']}
    differences = [{'tile_hash': key, 'fields': sorted(
        f for f in set(aa.get(key, {})) | set(bb.get(key, {}))
        if aa.get(key, {}).get(f) != bb.get(key, {}).get(f))}
        for key in sorted(set(aa) | set(bb)) if aa.get(key) != bb.get(key)]
    blocks = {}
    for block in backup.split('[[assets]]')[1:]:
        key = re.search(r'^tile_hash\s*=\s*"([^"]+)"', block, re.M).group(1)
        blocks[key] = block
    edits = []
    def update(match):
        block = match.group()
        key = re.search(r'^tile_hash\s*=\s*"([^"]+)"', block, re.M).group(1)
        for field in AUTHORIZED.get(key, ()):
            pattern = rf'(?m)^{field}\s*=\s*\[[^\]]*\]'
            literal = re.search(pattern, blocks.get(key, ''))
            # Full-floor sample is explicitly authorized even if absent in subset.
            if literal is None:
                if key == 'v1:4bpp:1a032ad7756c49db' and field == 'height':
                    value = 'height = [' + ', '.join(['0'] * 64) + ']'
                else:
                    raise ValueError(f'authorized backup field missing: {key} {field}')
            else:
                value = literal.group()
            if re.search(pattern, block):
                block = re.sub(pattern, lambda _: value, block, count=1)
            else:
                block = block.rstrip() + '\n' + value + '\n\n'
            edits.append({'tile_hash': key, 'field': field})
        return block
    result = re.sub(r'(?ms)^\[\[assets\]\].*?(?=^\[|\Z)', update, current)
    if len(edits) != sum(map(len, AUTHORIZED.values())):
        raise ValueError('authorized assets absent in current source')
    merged = tomllib.loads(result)
    ma = {v['tile_hash']: v for v in merged['assets']}
    for key, asset in aa.items():
        for field, value in asset.items():
            if field not in AUTHORIZED.get(key, ()) and ma[key].get(field) != value:
                raise ValueError('unauthorized field change')
    if any(merged[k] != v for k, v in a.items() if k != 'assets'):
        raise ValueError('settings changed')
    return result, {'current_assets': len(aa), 'backup_assets': len(bb),
        'backup_only_assets': sorted(set(bb) - set(aa)),
        'current_only_assets': len(set(aa) - set(bb)), 'authorized_fields': edits,
        'non_asset_section_differences': [k for k in set(a) | set(b)
                                        if k != 'assets' and a.get(k) != b.get(k)],
        'shared_asset_differences': [d for d in differences if d['tile_hash'] in aa and d['tile_hash'] in bb],
        'rationale': 'Backup is a subset/older source. Keep current settings and every non-authorized field; copy only four newly authorized samples. Full floor is user-authorized zero even if absent from backup.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--current', type=Path, required=True)
    p.add_argument('--backup', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    args = p.parse_args()
    inputs = {args.current.resolve(), args.backup.resolve()}
    if args.output.resolve() in inputs or args.report.resolve() in inputs or args.output == args.report:
        p.error('outputs must be distinct from inputs and each other')
    snapshots = {path: path.read_bytes() for path in (args.current, args.backup)}
    text, report = merge(snapshots[args.current].decode(), snapshots[args.backup].decode())
    report['input_sha256'] = {str(k): hashlib.sha256(v).hexdigest() for k, v in snapshots.items()}
    report['merged_sha256'] = hashlib.sha256(text.encode()).hexdigest()
    if any(path.read_bytes() != data for path, data in snapshots.items()):
        raise ValueError('source changed during merge')
    args.output.write_text(text)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('shared_asset_differences',)}, indent=2))


if __name__ == '__main__':
    main()
