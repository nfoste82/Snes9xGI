"""Exact-coordinate scalar offsets shared with the native placement inspector."""
import math
import os
import re
import tempfile


def integer_terrain_heights(values):
    """One-time migration only: quantize legacy pixels, halves away from zero."""
    if len(values) != 64 or any(type(v) not in (int, float) or not math.isfinite(v) for v in values):
        raise ValueError('Invalid per-pixel terrain height offsets')
    return [math.floor(v + .5) if v >= 0 else math.ceil(v - .5) for v in values]


def integer_terrain_profile(text):
    """Change only overworld terrain height arrays, preserving all other text."""
    import tomllib
    def replace(match):
        block = match.group()
        row = tomllib.loads(block)['overworld_terrain'][0]
        if 'height_offset' not in row:
            return block
        values = integer_terrain_heights(row['height_offset'])
        if all(type(v) is int for v in row['height_offset']):
            return block
        return re.sub(r'(?ms)(^height_offset\s*=\s*)\[.*?\]',
                      lambda m: m[1] + '[' + ', '.join(map(str, values)) + ']', block, count=1)
    result = re.sub(r'(?ms)^\[\[overworld_terrain\]\][^\n]*\n.*?(?=^\[|\Z)', replace, text)
    expected = tomllib.loads(text)
    for row in expected.get('overworld_terrain', []):
        if 'height_offset' in row:
            row['height_offset'] = integer_terrain_heights(row['height_offset'])
    if tomllib.loads(result) != expected:
        raise ValueError('Terrain rounding changed unrelated profile data')
    return result


class OffsetConflict(ValueError):
    pass


class HeightOffsetStore:
    def __init__(self, annotations):
        self.annotations = annotations

    def config(self):
        self.annotations.refresh()
        with self.annotations.lock:
            revision = self.annotations.revision()
            doc = self.annotations._profile_document or {}
            offsets, terrain = {}, {}
            def key(row):
                return f'{row["world"]}:{row["x"]}:{row["y"]}'
            for row in doc.get('overworld_offsets', []):
                if row['background'] == 1:
                    offsets[key(row)] = row['offset']
            for row in doc.get('overworld_terrain', []):
                if row['background'] == 1 and 'height_offset' in row:
                    values = row['height_offset']
                    if len(values) != 64 or any(type(v) is not int for v in values):
                        raise ValueError('Per-pixel terrain height offsets must be 64 integers; '
                                         'run the explicit --round-profile migration for legacy floats')
                    terrain[key(row)] = values
            heights = dict(offsets)
            nonuniform = []
            for at, values in terrain.items():
                if any(v != values[0] for v in values):
                    nonuniform.append(at)
            return dict(revision=revision, path=str(self.annotations.profile_path) if self.annotations.profile_path else None,
                        enabled=doc.get('overworld_placement', {}).get('mode') == 'authored_overworld_offsets',
                        offsets=offsets, heights=heights, nonuniform=nonuniform, terrain=list(terrain),
                        terrain_heights=terrain)

    def remove_terrain_heights(self, world, locations, revision):
        if not isinstance(locations, list):
            raise ValueError('Expected a list of locations')
        return self.save_values(world, [dict(at, offset=None) if isinstance(at, dict) else at
                                      for at in locations], revision, remove_terrain=True)

    def save(self, world, x, y, offset, revision):
        return self.save_group(world, [dict(x=x, y=y)], offset, revision)

    def save_group(self, world, locations, offset, revision):
        if not isinstance(locations, list):
            raise ValueError('Expected a list of locations')
        if offset is not None and (type(offset) is not int or not -32767 <= offset <= 32767):
            raise ValueError('Offset must be an integer from -32767 to 32767, or null to remove')
        return self.save_values(world, [dict(at, offset=offset) if isinstance(at, dict) else at for at in locations], revision)

    def save_values(self, world, locations, revision, remove_terrain=False):
        if world not in ('light', 'dark') or not isinstance(locations, list) or not 0 < len(locations) <= 512*512:
            raise ValueError('Expected light/dark world and a nonempty list of locations')
        targets = {}
        for at in locations:
            if (not isinstance(at, dict) or set(at) != {'x', 'y', 'offset'} or
                    any(type(v) is not int or not 0 <= v < 512 for v in (at['x'], at['y']))):
                raise ValueError('Expected cell coordinates within 512×512')
            offset = at['offset']
            if offset is not None and (type(offset) is not int or not -32767 <= offset <= 32767):
                raise ValueError('Offset must be an integer from -32767 to 32767, or null to remove')
            key = (at['x'], at['y'])
            if key in targets and targets[key] != offset:
                raise ValueError('Conflicting offsets for the same coordinate')
            targets[key] = offset
        current = self.config()
        with self.annotations.lock:
            store = self.annotations
            if current['revision'] != revision or store.revision() != revision:
                raise OffsetConflict('Profile/metadata changed; reload before saving')
            if not store.profile_path:
                raise ValueError('Select a profile with --profile to save placement offsets')
            baseline = store.profile_bytes
            text = baseline.decode()
            def replace(match):
                import tomllib
                row = tomllib.loads(match.group())['overworld_offsets'][0]
                if row['world'] == world and row['background'] == 1 and (row['x'], row['y']) in targets:
                    return ''
                return match.group()
            if remove_terrain:
                def strip_height(match):
                    import tomllib
                    row = tomllib.loads(match.group())['overworld_terrain'][0]
                    if row['world'] != world or row['background'] != 1 or (row['x'], row['y']) not in targets:
                        return match.group()
                    if not set(row) - {'world', 'background', 'x', 'y', 'height_offset'}:
                        return ''  # Native profiles reject empty terrain records.
                    # Remove only this field, including multiline arrays. Retain
                    # normals and every other property of the placement.
                    return re.sub(r'(?ms)^height_offset\s*=\s*\[.*?\][ \t]*(?:\n|$)', '', match.group())
                text = re.sub(r'(?ms)^\[\[overworld_terrain\]\][^\n]*\n.*?(?=^\[|\Z)', strip_height, text)
            else:
                text = re.sub(r'(?ms)^\[\[overworld_offsets\]\][^\n]*\n.*?(?=^\[|\Z)', replace, text)
            if not remove_terrain and any(value is not None for value in targets.values()):
                text += ''.join(f'\n[[overworld_offsets]]\nworld = "{world}"\nbackground = 1\n'
                                f'x = {x}\ny = {y}\noffset = {offset}\n' for (x, y), offset in sorted(targets.items()) if offset is not None)
                text = re.sub(r'(?m)^schema_version\s*=\s*\d+\s*$',
                              f'schema_version = {max(37, store._profile_document["schema_version"])}', text, count=1)
                if 'overworld_placement' not in store._profile_document:
                    text += '\n[overworld_placement]\nmode = "authored_overworld_offsets"\n'
                else:
                    text = re.sub(r'(?ms)(^\[overworld_placement\][^\n]*\n)(.*?)(?=^\[|\Z)',
                                  lambda m: m[1] + re.sub(r'(?m)^mode\s*=.*$', 'mode = "authored_overworld_offsets"', m[2]), text)
            import tomllib
            parsed = tomllib.loads(text)
            # Never rewrite assets, terrain arrays, normals, other coordinates or groups.
            for field in set(parsed) | set(store._profile_document):
                if remove_terrain and field == 'overworld_terrain':
                    expected = []
                    for row in store._profile_document.get(field, []):
                        row = dict(row)
                        if row['world'] == world and row['background'] == 1 and (row['x'], row['y']) in targets:
                            row.pop('height_offset', None)
                            if not set(row) - {'world', 'background', 'x', 'y'}:
                                continue
                        expected.append(row)
                    if parsed.get(field, []) != expected:
                        raise ValueError('Terrain removal changed unrelated placement fields')
                elif field not in (() if remove_terrain else ('schema_version', 'overworld_offsets', 'overworld_placement')):
                    if parsed.get(field) != store._profile_document.get(field):
                        raise ValueError(f'Offset edit changed unrelated {field}')
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(mode='w', dir=store.profile_path.parent,
                                                 prefix=store.profile_path.name + '.', suffix='.tmp', delete=False) as out:
                    temporary = out.name
                    out.write(text)
                    out.flush()
                    os.fsync(out.fileno())
                if store.profile_path.read_bytes() != baseline:
                    raise OffsetConflict('Profile changed on disk; reload before saving')
                os.replace(temporary, store.profile_path)
            finally:
                if temporary and os.path.exists(temporary):
                    os.unlink(temporary)
            store.profile_bytes = text.encode()
            store._profile_document = parsed
        return self.config()


if __name__ == '__main__':
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser(description='Round existing overworld terrain height arrays to integer pixels.')
    parser.add_argument('--round-profile', type=Path, required=True)
    args = parser.parse_args()
    baseline = args.round_profile.read_bytes()
    rounded = integer_terrain_profile(baseline.decode()).encode()
    if rounded != baseline:
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=args.round_profile.parent, prefix=args.round_profile.name + '.',
                                             suffix='.tmp', delete=False) as out:
                temporary = out.name
                out.write(rounded)
                out.flush()
                os.fsync(out.fileno())
            if args.round_profile.read_bytes() != baseline:
                raise OffsetConflict('Profile changed on disk; retry rounding the current profile')
            os.replace(temporary, args.round_profile)
        finally:
            if temporary and os.path.exists(temporary):
                os.unlink(temporary)
        print('Saved integer overworld terrain pixels; unrelated profile data verified unchanged.')
    else:
        print('Overworld terrain pixels are already integers; no changes.')
