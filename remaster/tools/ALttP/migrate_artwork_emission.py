#!/usr/bin/env python3
"""Explicit, backed-up migration: discard emission RGB, preserve all intensities."""
import argparse
import copy
import json
from pathlib import Path
import re
import tomllib


def migrate(text):
    before = tomllib.loads(text)
    def replace(match):
        rgba = json.loads(match[1])
        if len(rgba) != 256:
            raise ValueError('Expected 256 emission RGBA values')
        return 'emission_intensity = ' + json.dumps(rgba[3::4])
    result, count = re.subn(r'^emission_rgba\s*=\s*(\[[^\n]*\])', replace, text, flags=re.M)
    result = re.sub(r'^emission_color_mask\s*=.*\n', '', result, flags=re.M)
    result = re.sub(r'^schema_version\s*=\s*\d+', 'schema_version = 31', result, count=1, flags=re.M)
    expected = copy.deepcopy(before)
    expected['schema_version'] = 31
    for asset in expected.get('assets', []):
        if 'emission_rgba' in asset:
            asset['emission_intensity'] = asset.pop('emission_rgba')[3::4]
        asset.pop('emission_color_mask', None)
    if tomllib.loads(result) != expected:
        raise ValueError('Migration changed unrelated profile data')
    return result, count


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    source = args.profile.read_text()
    result, count = migrate(source)
    if args.apply:
        backup = args.profile.with_suffix('.pre-artwork-emission.toml.bak')
        with backup.open('x') as file:
            file.write(source)
        args.profile.write_text(result)
        print(f'Backup: {backup}')
    print(f'{count} emission layers migrated; all 64 intensities per layer and unrelated data preserved' +
          ('' if args.apply else ' (dry run)'))
