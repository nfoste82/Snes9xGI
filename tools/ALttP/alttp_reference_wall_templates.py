"""Opt-in local wall templates; lighting normals are never geometry outputs.

Templates select semantic topology, not similar artwork. H/V transformations
mirror samples and signed normal components. Existing normals always win.
Diagonal assembly parts require explicit, reviewed pixel profiles; absence of
such a profile is missing evidence, not permission to use a tile-wide ramp.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from alttp_height_samples import height_bytes


def calibrated_diagonal(source, cap_normals=None, floor_normals=None):
    """Review-only SE cross-section calibrated from authored cap/face samples.

    Require a constant x+y cross-section, a zero datum and at least three
    consecutive unit descents. No derivative lighting or normalized heights.
    A full face extends the measured unit slope to local zero at (7,7).
    An authored exact-up cap clips its high end. A user-identified mixed-floor
    reference clips its low end at the first complete exact-up diagonal band.
    Exact-up normals are region evidence only, never inverted into a slope.
    """
    heights, normals = source.get('height'), source.get('normal_xyz')
    if heights is None or normals is None or len(heights) != 64 or len(normals) != 192:
        raise ValueError('diagonal calibration requires authored height and normals')
    bands = {}
    for p, h in enumerate(heights):
        bands.setdefault(p // 8 + p % 8, set()).add(h)
    if any(len(v) != 1 for v in bands.values()) or min(heights) != 0:
        raise ValueError('diagonal calibration requires constant bands and local zero')
    cross = [next(iter(bands[s])) for s in range(15)]
    if any(cross[s] - cross[s+1] not in (0, 1) for s in range(14)):
        raise ValueError('diagonal calibration has inconsistent descent')
    if not any(all(cross[s+k] - cross[s+k+1] == 1 for k in range(3))
               for s in range(12)):
        raise ValueError('diagonal calibration lacks consecutive unit descent')
    cap = None
    if cap_normals is not None:
        if len(cap_normals) != 192:
            raise ValueError('invalid cap normal array')
        up = {p for p in range(64) if tuple(cap_normals[p*3:p*3+3]) == (128,128,255)}
        if not up:
            raise ValueError('authored cap has no exact up region')
        boundary = max(p // 8 + p % 8 for p in up)
        if up != {p for p in range(64) if p // 8 + p % 8 <= boundary}:
            raise ValueError('cap is not a complete diagonal half-plane')
        cap = 14 - boundary
    floor_boundary = 14
    floor_mask = None
    if floor_normals is not None:
        if cap_normals is not None or len(floor_normals) != 192:
            raise ValueError('invalid mixed-floor normal array or combined cap')
        up = {p for p in range(64) if tuple(floor_normals[p*3:p*3+3]) == (128,128,255)}
        if not up or len(up) == 64:
            raise ValueError('authored mixed floor needs both floor and face regions')
        floor_boundary = min(p // 8 + p % 8 for p in up)
        if up != {p for p in range(64) if p // 8 + p % 8 >= floor_boundary}:
            raise ValueError('floor is not a complete diagonal half-plane')
        floor_mask = sum(1 << p for p in up)
    values = tuple(min(cap, max(0, floor_boundary-x-y)) if cap is not None
                   else max(0, floor_boundary-x-y)
                   for y in range(8) for x in range(8))
    record = {'calibration_cross_section': cross, 'unit_slope': 1,
              'local_zero': [7,7], 'cap_height': cap,
              'status': 'calibrated_local_proposal_not_assembly_proof'}
    if floor_mask is not None:
        record.update(floor_mask=f'0x{floor_mask:016x}', floor_pixels=len(up),
                      wall_pixels=64-len(up), floor_boundary_x_plus_y=floor_boundary,
                      floor_mask_rows=[''.join('F' if y*8+x in up else 'W'
                                              for x in range(8)) for y in range(8)],
                      floor_normal_xyz=[128,128,255],
                      floor_normal_sha256=hashlib.sha256(bytes(floor_normals)).hexdigest(),
                      local_profile=list(values))
    return values, record


def transform(values, channels=1, hflip=False, vflip=False, normal=False):
    result = []
    for y in range(8):
        for x in range(8):
            p = (7 - y if vflip else y) * 8 + (7 - x if hflip else x)
            sample = list(values[p * channels:(p + 1) * channels])
            if normal:
                if hflip:
                    sample[0] = max(0, min(255, 256 - sample[0]))
                if vflip:
                    sample[1] = max(0, min(255, 256 - sample[1]))
            result.extend(sample)
    return tuple(result)


def transform_facing(facing, hflip, vflip):
    parts = facing.split('_')
    if hflip:
        parts = [{'east': 'west', 'west': 'east'}.get(p, p) for p in parts]
    if vflip:
        parts = [{'north': 'south', 'south': 'north'}.get(p, p) for p in parts]
    return '_'.join(parts)


class ReferenceTemplates:
    def __init__(self, path: Path, authored: dict, rom_sha256: str):
        raw = path.read_bytes()
        self.digest = hashlib.sha256(raw).hexdigest()
        self.config = json.loads(raw)
        if self.config.get('format') != 'alttp-reference-wall-templates-v1':
            raise ValueError('invalid reference wall template format')
        if self.config.get('rom_sha256') != rom_sha256:
            raise ValueError('reference wall template ROM mismatch')
        self.authored = authored
        self.entries = self.config['templates']
        if len({e['id'] for e in self.entries}) != len(self.entries):
            raise ValueError('duplicate reference template id')
        for entry in self.entries:
            if not entry.get('evidence'):
                raise ValueError('reference template requires evidence')
            for field, size in (('height', 64), ('normal_xyz', 192)):
                if field in entry and (len(entry[field]) != size or
                        any(type(v) is not int or not (-32768 if field == 'height' else 0) <= v <= (32767 if field == 'height' else 255) for v in entry[field])):
                    raise ValueError('invalid reference ' + field)
            if 'floor_mask' in entry:
                mask = int(entry['floor_mask'], 0)
                if not 0 <= mask < 1 << 64 or 'height' not in entry:
                    raise ValueError('floor mask needs explicit height')
                if any(entry['height'][p] != 0 for p in range(64) if mask >> p & 1):
                    raise ValueError('mixed floor pixels must have local height zero')

    def resolve(self, content_id, annotation):
        matches = []
        missing = []
        calibrations = []
        exclusions = [e['id'] for e in self.entries if e.get('status') == 'unresolved' and
                      (content_id == e['reference'] or content_id in e.get('tile_hashes', []))]
        if exclusions:
            return None, {'tile_hash': content_id, 'matches': [],
                          'missing_templates': exclusions, 'classification': 'missing',
                          'reason': 'explicit unresolved assembly part excludes generic templates'}
        owned = [e for e in self.entries if 'tile_hashes' in e and
                 (content_id == e['reference'] or content_id in e['tile_hashes'])]
        for entry in self.entries:
            # Explicit assembly parts take precedence over full-face templates.
            # Otherwise a freshly authored cap becomes an ambiguous generic match.
            if owned and entry not in owned:
                continue
            if content_id == entry['reference']:
                transforms = [(False, False)]
            elif 'tile_hashes' in entry:
                if content_id not in entry['tile_hashes']:
                    continue
                transforms = [(False, False)]
            else:
                if annotation.get('role') != entry.get('role') or annotation.get('context_dependent'):
                    continue
                if annotation.get('corner_type') != entry.get('corner_type'):
                    continue
                transforms = [(h, v) for h in (False, True) for v in (False, True)
                              if not (entry['facings'] in (['east'], ['west']) and v)
                              and not (entry['facings'] in (['north'], ['south']) and h)
                              if sorted(transform_facing(n, h, v) for n in entry['facings']) ==
                              sorted(annotation.get('normals', []))]
            source = self.authored.get(entry['reference'], {})
            # No inferred normal, no numeric derivative. A missing reference
            # normal remains missing even if the height happens to be present.
            heights = entry.get('height', source.get('height'))
            normals = entry.get('normal_xyz', source.get('normal_xyz'))
            calibration = None
            if entry.get('generator') == 'calibrated_diagonal':
                calibration_source = self.authored.get(entry['calibration_reference'], {})
                cap_normals = normals if entry.get('authored_cap') else None
                floor_normals = normals if entry.get('authored_floor') else None
                heights, calibration = calibrated_diagonal(calibration_source, cap_normals, floor_normals)
                if transforms:
                    calibrations.append({'template': entry['id'], **calibration})
            if entry.get('status') == 'unresolved' or heights is None or normals is None:
                if transforms:
                    missing.append(entry['id'])
                continue
            for values, size in ((heights, 64), (normals, 192)):
                if len(values) != size or any(type(v) is not int or not (-32768 if size == 64 else 0) <= v <= (32767 if size == 64 else 255) for v in values):
                    raise ValueError('invalid source reference samples: ' + entry['reference'])
            for h, v in transforms:
                matches.append((entry['id'], h, v,
                                transform(heights, hflip=h, vflip=v),
                                transform(normals, 3, h, v, normal=True)))
        # Multiple matches are accepted only when all resulting arrays agree.
        distinct = {(m[3], m[4]) for m in matches}
        record = {'tile_hash': content_id, 'matches': [
            {'template': m[0], 'hflip': m[1], 'vflip': m[2]} for m in matches],
            'missing_templates': missing,
            'classification': 'ambiguous' if len(distinct) > 1 else 'matched' if distinct else 'missing'}
        if calibrations:
            record['calibrations'] = calibrations
        if len(distinct) != 1:
            return None, record
        height, normal = next(iter(distinct))
        return {'height': height, 'normal_xyz': normal}, record

    def report(self, records):
        diagonal_directions = {}
        entries = {e['id']:e for e in self.entries}
        for record in records:
            if record['classification'] != 'matched':
                continue
            for match in record['matches']:
                entry = entries[match['template']]
                if entry.get('facings') == ['south_east']:
                    direction = transform_facing('south_east',match['hflip'],match['vflip'])
                    diagonal_directions[direction] = diagonal_directions.get(direction,0)+1
        return {'format': 'alttp-reference-wall-template-report-v1',
                'diagonal_matched_directions': diagonal_directions,
                'config_sha256': self.digest, 'records': records,
                'references': [{ 'template': e['id'], 'tile_hash': e['reference'],
                    'status': e.get('status', 'user_selected_reference'), 'evidence': e['evidence'],
                    'height_encoding': 'signed-int16-le',
                    'height_sha256': hashlib.sha256(height_bytes(self.authored.get(e['reference'], {}).get('height', []))).hexdigest(),
                    'normal_sha256': hashlib.sha256(bytes(self.authored.get(e['reference'], {}).get('normal_xyz', []))).hexdigest()}
                    for e in self.entries],
                'counts': {s: sum(r['classification'] == s for r in records)
                           for s in ('matched', 'ambiguous', 'missing')},
                'normal_policy': 'preserve every existing normal_xyz; copy transformed reference only when absent',
                'publishable': False}
