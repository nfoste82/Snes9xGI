"""Exact-coordinate scalar offsets shared with the native placement inspector."""
import math
import os
import re
import tempfile


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
                    terrain[key(row)] = row['height_offset']
            heights = dict(offsets)
            nonuniform = []
            for at, values in terrain.items():
                if len(values) != 64 or any(not isinstance(v, (int, float)) or isinstance(v, bool) or
                                            not math.isfinite(v) for v in values):
                    raise ValueError('Invalid per-pixel terrain height offsets')
                if any(v != values[0] for v in values):
                    heights.pop(at, None)
                    nonuniform.append(at)
                else:
                    heights[at] = offsets.get(at, 0) + values[0]
            return dict(revision=revision, path=str(self.annotations.profile_path) if self.annotations.profile_path else None,
                        enabled=doc.get('overworld_placement', {}).get('mode') == 'authored_overworld_offsets',
                        offsets=offsets, heights=heights, nonuniform=nonuniform, terrain=list(terrain))

    def save(self, world, x, y, offset, revision):
        return self.save_group(world, [dict(x=x, y=y)], offset, revision)

    def save_group(self, world, locations, offset, revision):
        if not isinstance(locations, list):
            raise ValueError('Expected a list of locations')
        if offset is not None and (type(offset) is not int or not -32767 <= offset <= 32767):
            raise ValueError('Offset must be an integer from -32767 to 32767, or null to remove')
        return self.save_values(world, [dict(at, offset=offset) if isinstance(at, dict) else at for at in locations], revision)

    def save_values(self, world, locations, revision):
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
            text = re.sub(r'(?ms)^\[\[overworld_offsets\]\][^\n]*\n.*?(?=^\[|\Z)', replace, text)
            if any(value is not None for value in targets.values()):
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
                if field not in ('schema_version', 'overworld_offsets', 'overworld_placement'):
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
