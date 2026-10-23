#!/usr/bin/env python3
"""Reference-guided Link geometry proposal; never edits the source profile.

The north walking reference is authoritative, not a silhouette-generated depth
map. Transfer is restricted to corresponding body parts/materials and facing.
Unseen parts use explicit rounded head/limb, cloth, shield and blade models.
World axes: X screen-right, Y toward viewer, Z up. All arrays remain canonical.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tomllib
import zlib

import numpy as np

from alttp_npc_workbench import atomic_write, merge_profile


def placeholder(asset):
    return asset.get('normal_xyz') in ([128, 255, 128] * 64, [255, 128, 128] * 64)


def unit(v):
    v = np.asarray(v, dtype=float)
    return v / max(float(np.linalg.norm(v)), 1e-9)


def encode(v):
    return [int(round((float(c) + 1) * 127.5)) for c in unit(v)]


def facing_family(facing):
    return 'side' if facing in ('east', 'west') else facing


def kind(part):
    address = part['address']
    if address in (16384, 16400, 16640, 16656):
        return 'head'
    if address in (16416, 16432, 16672, 16688):
        return 'body'
    if address in (16496, 16512, 16752, 16768):
        return 'shield'
    return 'sword'


def material(index, category, y):
    if category in ('sword', 'shield'):
        return category
    if index in (11, 12):
        return 'hat' if category == 'head' else 'cloth'
    if index in (9, 10):
        return 'cloth'
    if index == 8:
        return 'hair'
    if index in (3, 4, 6):
        return 'skin'
    if index == 14:
        return 'boot' if y >= 10 else 'belt'
    return 'detail'


def groups(pose, assets):
    """Assemble OAM parts before geometry, including hidden opaque pixels."""
    slots = defaultdict(list)
    for p in pose['parts']:
        slots[p['slot'], kind(p)].append(p)
    result = []
    for (_, category), parts in slots.items():
        x0 = min(p['x'] for p in parts)
        y0 = min(p['y'] for p in parts)
        pixels = {}
        for part in parts:
            asset = assets.get(part['tile_hash'], {})
            for i, ink in enumerate(part['indices']):
                if not ink:
                    continue
                x = part['x'] - x0 + (7 - i % 8 if part['hflip'] else i % 8)
                y = part['y'] - y0 + (7 - i // 8 if part['vflip'] else i // 8)
                n = None
                if 'normal_xyz' in asset and not placeholder(asset):
                    n = unit(np.array(asset['normal_xyz'][i*3:i*3+3]) / 127.5 - 1)
                    if not asset.get('normal_ignore_tile_transform'):
                        n[0] *= -1 if part['hflip'] else 1
                        n[1] *= -1 if part['vflip'] else 1
                pixels[x, y] = dict(part=part, pixel=i, ink=ink, normal=n,
                    height=asset.get('height', [None]*64)[i],
                    material=material(ink, category, y))
        # Outlines, eyes and highlights follow the adjacent physical surface,
        # not separate spheres or bumps inferred from palette brightness.
        interiors = [(xy, p) for xy, p in pixels.items() if p['material'] != 'detail']
        for (x, y), p in pixels.items():
            if p['material'] == 'detail' and interiors:
                _, nearest = min(interiors, key=lambda q: (q[0][0]-x)**2 + (q[0][1]-y)**2)
                p['material'] = nearest['material']
        result.append(dict(category=category, pixels=pixels, facing=pose['facing'],
            action=pose['action'], step=pose['step'], x=x0, y=y0))
    return result


def analytic(group, x, y, pixel):
    """Explicit part models, not brightness-to-height or a whole-body cylinder."""
    category, surface = group['category'], pixel['material']
    pixels = group['pixels']
    same = [(px, py) for (px, py), p in pixels.items() if p['material'] == surface]
    xs, ys = zip(*same)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    cx, cy = (x0+x1)/2, (y0+y1)/2
    rx, ry = max(2.0, (x1-x0+1)/2), max(2.0, (y1-y0+1)/2)
    u = max(-0.92, min(0.92, (x-cx)/rx))
    v = max(-0.92, min(0.92, (y-cy)/ry))
    if category == 'sword':
        # Blade faces are planes; only their bevels turn. Principal direction
        # comes from the assembled weapon, never an individual 8px tile edge.
        coords = np.array(list(pixels), dtype=float)
        centered = coords - coords.mean(axis=0)
        _, _, axes = np.linalg.svd(centered, full_matrices=False)
        across = axes[-1]
        offsets = centered @ across
        at = float((np.array([x, y]) - coords.mean(axis=0)) @ across)
        width = max(1, float(np.max(np.abs(offsets))))
        bevel = abs(at) > width * 0.55
        n = unit((across[0] * math.copysign(0.55, at) if bevel else 0,
                  0.45 if bevel else 0.60, 0.72 if bevel else 0.80))
        return n, max(3, min(22, 9 - (group['y']+y-12)*0.40))
    if category == 'shield':
        # Broad shield face, softly crowned wood, distinct rim bevel.
        edge = max(abs(u), abs(v)) > 0.72
        n = unit((u*(0.60 if edge else 0.18), 0.88, 0.24-v*(0.32 if edge else 0.12)))
        return n, max(2, min(15, 10 - (y-y0)*0.55))
    if category == 'head':
        # Head dome: crown +Z; the visible lower face points toward +Y.
        # Only the far crest rolls slightly toward -Y, never toward -Z.
        if surface == 'hat':
            n = unit((u*0.70, 0.30+0.60*(v+1)/2, 0.95-0.75*(v+1)/2))
            if v < -0.65 and abs(u) < 0.55:
                n = unit((u*0.4, -0.12, 0.99))
            h = 10.5 + 2.5*math.sqrt(max(0, 1-u*u)) - max(0, y-6)*0.55
        elif surface == 'hair':
            n = unit((u*0.62, 0.66, 0.65-v*0.24))
            h = 10 - (y-5)*0.55 + 1.0*math.sqrt(max(0, 1-u*u))
        else:
            n = unit((u*0.65, 0.91, 0.28-v*0.30))
            h = 10 - (y-5)*0.60 + 0.8*math.sqrt(max(0, 1-u*u))
        return n, max(5, min(14, h))
    # Torso cloth, arms and each boot use independent material spans. The boot
    # toe/top points upward; cloth fronts remain predominantly viewer-facing.
    row = [px for (px, py), p in pixels.items() if py == y and p['material'] == surface]
    left = right = x
    while left-1 in row:
        left -= 1
    while right+1 in row:
        right += 1
    u = max(-0.9, min(0.9, (x-(left+right)/2)/max(1.5, (right-left+1)/2)))
    if surface == 'boot':
        n = unit((u*0.50, 0.62, 0.78 if y <= cy else 0.35))
        h = max(0, 3 - (y-y0)*0.65)
    elif surface == 'skin':
        n = unit((u*0.65, 0.75, 0.55-v*0.20))
        h = max(1, 10-y*0.65)
    elif surface == 'belt':
        n = unit((u*0.25, 0.96, 0.15))
        h = max(1, 10-y*0.75)
    else:
        n = unit((u*0.65, 0.78, 0.40-v*0.18))
        h = max(0, 10-y*0.75) + 0.5*math.sqrt(max(0, 1-u*u))
    return n, h


def transfer(group, references):
    """Match anatomical artwork patches, preserving authored normals as samples."""
    candidates = [r for r in references if r['category'] == group['category']
                  and facing_family(r['facing']) == facing_family(group['facing'])]
    ranked = []
    for r in candidates:
        common = set(group['pixels']) | set(r['pixels'])
        diff = sum(group['pixels'].get(xy, {}).get('ink', 0) !=
                   r['pixels'].get(xy, {}).get('ink', 0) for xy in common) / len(common)
        ranked.append((diff, r))
    ranked.sort(key=lambda item: item[0])
    reference = ranked[0][1] if ranked and ranked[0][0] < 0.48 else None
    output = {}
    for (x, y), p in group['pixels'].items():
        n, h = analytic(group, x, y, p)
        method = 'part-model'
        if reference:
            samples = []
            for (qx, qy), q in reference['pixels'].items():
                if q['normal'] is None or q['material'] != p['material']:
                    continue
                distance = (qx-x)**2 + (qy-y)**2
                if distance > 10:
                    continue
                # Artwork patch dissimilarity prevents e.g. eye outlines from
                # grabbing a cap edge merely because both use palette index 5.
                patch = sum(group['pixels'].get((x+dx, y+dy), {}).get('ink', 0) !=
                            reference['pixels'].get((qx+dx, qy+dy), {}).get('ink', 0)
                            for dx in (-1, 0, 1) for dy in (-1, 0, 1))
                samples.append((distance + patch*0.45, q))
            if samples:
                samples.sort(key=lambda item: item[0])
                best, q = samples[0]
                weight = 1.0 if best < 0.01 else max(0.35, 0.88-best*0.045)
                n = unit(q['normal']*weight + n*(1-weight))
                if q['height'] is not None:
                    h = q['height']*weight + h*(1-weight)
                method = 'authored-transfer'
        output[x, y] = (n, int(round(h)), method)
    return output


def png(path, width, height, rgb):
    def chunk(tag, data):
        return struct.pack('>I', len(data))+tag+data+struct.pack('>I', zlib.crc32(tag+data))
    lines = b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))+
                     chunk(b'IDAT', zlib.compress(lines))+chunk(b'IEND', b''))


def preview(output, actors, assets):
    selected = []
    # Each row is one action/facing, columns: art, normals, heights, neutral
    # lighting. Include animation endpoints and equipped examples separately.
    for actor in actors[:2]:
        for pose in actor['poses']:
            if pose['step'] == 0:
                selected.append((actor['id'], pose))
    width, row_height, scale = 768, 144, 4
    height = len(selected)*row_height
    rgb = bytearray([28, 30, 35]*width*height)
    index = []
    for row, (actor, pose) in enumerate(selected):
        index.append(dict(row=row, actor=actor, action=pose['action'], facing=pose['facing'], step=pose['step']))
        for p in reversed(pose['parts']):
            a = assets.get(p['tile_hash'], {})
            flip = ('both' if p['hflip'] and p['vflip'] else 'horz' if p['hflip'] else 'vert' if p['vflip'] else 'normal')
            normals = a.get(f'flip_{flip}_normal_xyz', a.get('normal_xyz')) if a.get('geometry_by_flip') else a.get('normal_xyz')
            heights = a.get(f'flip_{flip}_height', a.get('height')) if a.get('geometry_by_flip') else a.get('height')
            for i, ink in enumerate(p['indices']):
                if not ink:
                    continue
                x = 16+(p['x']-pose['bounds'][0]+(7-i%8 if p['hflip'] else i%8))*scale
                y = row*row_height+8+(p['y']-pose['bounds'][1]+(7-i//8 if p['vflip'] else i//8))*scale
                c = p['colors'][i]
                art = [(c>>s&31)*255//31 for s in (0, 5, 10)]
                if normals:
                    n = unit(np.array(normals[i*3:i*3+3])/127.5-1)
                    explicit = a.get('geometry_by_flip') and f'flip_{flip}_normal_xyz' in a
                    if not explicit and not a.get('normal_ignore_tile_transform'):
                        n[0] *= -1 if p['hflip'] else 1
                        n[1] *= -1 if p['vflip'] else 1
                    normal_color = encode(n)
                    light = int(255*(0.18+0.82*max(0, float(n @ unit((-0.5, 0.5, 0.85))))))
                    neutral = [light]*3
                else:
                    normal_color = neutral = [255, 0, 255]
                h = heights[i] if heights else None
                hcolor = [max(0, min(255, round(h/24*255)))]*3 if h is not None else [255, 0, 255]
                for col, color in enumerate((art, normal_color, hcolor, neutral)):
                    for dy in range(scale):
                        for dx in range(scale):
                            xx, yy = x+col*192+dx, y+dy
                            if xx < (col+1)*192 and yy < (row+1)*row_height:
                                at = (yy*width+xx)*3
                                rgb[at:at+3] = bytes(color)
    # Short pages are actually inspectable in image tools.
    for page, start in enumerate(range(0, len(selected), 12)):
        end = min(len(selected), start+12)
        png(output/f'review-{page+1:02}.png', width, (end-start)*row_height,
            rgb[start*row_height*width*3:end*row_height*width*3])
    atomic_write(output/'review-index.json', json.dumps(index, indent=2)+'\n')


def propose(manifest, text):
    parsed = tomllib.loads(text)
    assets = {a['tile_hash']: a for a in parsed.get('assets', [])}
    actors = [a for a in manifest['actors'] if a['name'] == 'Link']
    hashes = {p['tile_hash'] for a in actors for f in a['poses'] for p in f['parts']}
    foreign = {p['tile_hash'] for a in manifest['actors'] if a['name'] != 'Link'
               for f in a['poses'] for p in f['parts']}
    assert not hashes & (foreign | set(manifest['background_shared_hashes']))
    all_groups = [g for a in actors for f in a['poses'] for g in groups(f, assets)]
    references, seen = [], set()
    for g in all_groups:
        signature = (g['category'], g['facing'], tuple((xy, p['ink']) for xy, p in sorted(g['pixels'].items())))
        if signature in seen:
            continue
        seen.add(signature)
        if g['action'] == 'walking' and sum(p['normal'] is not None for p in g['pixels'].values()) >= len(g['pixels'])*0.85:
            references.append(g)
    proposals = defaultdict(lambda: defaultdict(list))
    seen.clear()
    for g in all_groups:
        signature = (g['category'], g['facing'], g['y'], tuple((xy, p['part']['tile_hash'], p['pixel'], p['part']['hflip'], p['part']['vflip']) for xy, p in sorted(g['pixels'].items())))
        if signature in seen:
            continue
        seen.add(signature)
        inferred = transfer(g, references)
        for xy, p in g['pixels'].items():
            part = p['part']
            h = part['tile_hash']
            n, height, method = inferred[xy]
            flip = (int(part['hflip']), int(part['vflip']))
            proposals[h][flip, p['pixel']].append((n, height, method))
    edits, overrides, details = {}, {}, {}
    disagreements = []
    for h in sorted(hashes):
        original = assets.get(h, {})
        need_n = 'normal_xyz' not in original or placeholder(original)
        need_h = 'height' not in original
        if not need_n and not need_h:
            continue
        assert not any(k.startswith(('flip_', 'facing_')) or k == 'geometry_by_flip' for k in original), h
        by_flip = defaultdict(dict)
        methods = Counter()
        for (flip, i), values in proposals[h].items():
            normals = np.array([v[0] for v in values])
            n = unit(normals.mean(axis=0))
            heights = [v[1] for v in values]
            spread = max(heights) - min(heights)
            angle = max(math.degrees(math.acos(float(np.clip(v @ n, -1, 1)))) for v in normals)
            if spread > 2 or angle > 30:
                disagreements.append(dict(tile_hash=h, flip=list(flip), pixel=i,
                    height_spread=spread, normal_max_angle=round(angle, 2)))
            by_flip[flip][i] = (n, int(round(float(np.median(heights)))))
            methods.update(v[2] for v in values)
        # Repeated placements are reconciled in canonical space by normal-vector
        # mean and median height. These are approximations, NOT conflict-free
        # evidence; large disagreements are explicitly retained in the report.
        # A vertically mirrored weapon is NOT a downward-facing sword face.
        # Use explicit gameplay-space flip sets where vector reflection would
        # make an upward normal point down or a front face point behind Link.
        flip_specific = any(f[1] for f in by_flip)
        if need_n and flip_specific:
            overrides[h] = {'geometry_by_flip': True}
        normals, heights = [128, 128, 255]*64, [0]*64
        for i in range(64):
            samples = [(flip, data[i]) for flip, data in by_flip.items() if i in data]
            if not samples:
                continue
            canonical = []
            for flip, (n, height) in samples:
                n = n.copy()
                if not original.get('normal_ignore_tile_transform'):
                    n[0] *= -1 if flip[0] else 1
                    n[1] *= -1 if flip[1] else 1
                canonical.append(n)
            normals[i*3:i*3+3] = encode(np.mean(canonical, axis=0))
            heights[i] = int(round(float(np.median([v[1][1] for v in samples]))))
        layers = {}
        if need_n:
            layers['normal_xyz'] = normals
        if need_h:
            layers['height'] = heights
        edits[h] = layers
        if need_n and flip_specific:
            for flip, data in sorted(by_flip.items()):
                name = 'both' if all(flip) else 'horz' if flip[0] else 'vert' if flip[1] else 'normal'
                ns = [128, 128, 255]*64
                hs = [0]*64
                for i, (n, height) in data.items():
                    ns[i*3:i*3+3] = encode(n)
                    hs[i] = height
                overrides[h][f'flip_{name}_normal_xyz'] = ns
                if need_h:
                    overrides[h][f'flip_{name}_height'] = hs
        details[h] = dict(layers=list(layers), replaced_placeholder=placeholder(original),
                          flip_specific=flip_specific and need_n, evidence=dict(methods))
    # Remove ONLY the user-confirmed placeholder normal lines before the normal
    # fill-only exporter. Every other existing field must survive unchanged.
    def remove_placeholder(match):
        block = match.group()
        record = tomllib.loads(block)['assets'][0]
        if record['tile_hash'] in edits and placeholder(record):
            block = re.sub(r'(?m)^normal_xyz\s*=.*\n?', '', block)
        return block
    cleaned = re.sub(r'(?ms)^\[\[assets\]\]\n.*?(?=^\[|\Z)', remove_placeholder, text)
    candidate, _ = merge_profile(cleaned, edits)
    def add_overrides(match):
        block = match.group()
        record = tomllib.loads(block)['assets'][0]
        extra = overrides.get(record['tile_hash'], {})
        return block.rstrip()+'\n'+''.join(f'{k} = {json.dumps(v)}\n' for k, v in extra.items())+'\n' if extra else block
    candidate = re.sub(r'(?ms)^\[\[assets\]\]\n.*?(?=^\[|\Z)', add_overrides, candidate)
    result = tomllib.loads(candidate)
    new_assets = {a['tile_hash']: a for a in result['assets']}
    for h, asset in assets.items():
        for key, value in asset.items():
            if h in edits and key == 'normal_xyz' and placeholder(asset):
                continue
            assert new_assets[h][key] == value, (h, key)
    for h in hashes:
        assert 'height' in new_assets[h] and 'normal_xyz' in new_assets[h]
        assert not placeholder(new_assets[h])
    report = dict(source_sha256=hashlib.sha256(text.encode()).hexdigest(),
        pose_count=sum(len(a['poses']) for a in actors), unique_link_tiles=len(hashes),
        authored_reference_groups=len(references), added_heights=sum('height' in e for e in edits.values()),
        added_normals=sum('normal_xyz' in e and 'normal_xyz' not in assets.get(h, {}) for h, e in edits.items()),
        replaced_placeholders=sum(d['replaced_placeholder'] for d in details.values()),
        preserved_detailed_normal_tiles=sum('normal_xyz' in assets.get(h, {}) and not placeholder(assets[h]) for h in hashes),
        preserved_height_tiles=sum('height' in assets.get(h, {}) for h in hashes),
        flip_specific_tiles=len(overrides), missing_height_tiles=0, missing_normal_tiles=0,
        reuse_disagreement_samples=len(disagreements),
        reuse_disagreement_tiles=len({d['tile_hash'] for d in disagreements}),
        reuse_disagreements=disagreements,
        limitations=['Reference-guided proposals, not recovered 3D ground truth.',
            'Catalogued poses only; unmodeled player actions/equipment remain outside coverage.',
            'Hidden pixels are authored, but live animation/lighting still needs user review.',
            'Reused placements with divergent proposals use mean normals/median heights; see reuse_disagreements.',
            'Heights use stable part-local references; placement-dependent weapon elevation is approximated.'],
        tiles=details)
    return candidate, report, actors, new_assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=Path('build/remaster-geometry/npc-workbench/poses.json'))
    parser.add_argument('--profile', type=Path, default=Path('remaster/profiles/ALttP/alttp-profile.toml'))
    parser.add_argument('--output', type=Path, default=Path('build/remaster-geometry/link-geometry'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    text = args.profile.read_text()
    candidate, report, actors, assets = propose(json.loads(args.manifest.read_text()), text)
    target = args.output/'link-profile.toml'
    atomic_write(target, candidate)
    validator = args.manifest.parent/'alttp-profile-validate'
    subprocess.run([str(validator.resolve()), str(target.resolve())], check=True)
    preview(args.output, actors, assets)
    assert args.profile.read_text() == text
    atomic_write(args.output/'source-profile.toml', text)
    atomic_write(args.output/'report.json', json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('tiles', 'reuse_disagreements')}, indent=2))


if __name__ == '__main__':
    main()
