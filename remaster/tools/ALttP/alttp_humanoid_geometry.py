#!/usr/bin/env python3
"""Part-aware humanoid NPC geometry proposals; approved Link promotion is opt-in.

Uses ROM artwork, explicit anatomical recipes and existing authored geometry.
Does not infer relief from brightness. Writes review artifacts, not main-profile
NPC edits. NumPy and the generated NPC workbench manifest are required.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tomllib

import numpy as np

from alttp_link_geometry import encode, placeholder, png, unit
from alttp_npc_workbench import atomic_write, merge_profile


# Explicit visual-family inventory, not an automatic name/biped classifier.
# Fractions describe head/body division and robe vs separate legs. Unusual
# silhouettes get their own recipe instead of borrowing Link's exact anatomy.
RECIPES = {
    'human': dict(head=0.44, legs=0.77, width=0.78, robe=False),
    'child': dict(head=0.50, legs=0.79, width=0.72, robe=False),
    'elder': dict(head=0.44, legs=0.89, width=0.80, robe=True),
    'dress': dict(head=0.42, legs=0.94, width=0.72, robe=True),
    'guard': dict(head=0.43, legs=0.77, width=0.78, robe=False),
    'thief': dict(head=0.42, legs=0.78, width=0.82, robe=False),
    'wizard': dict(head=0.43, legs=0.94, width=0.82, robe=True),
    'large-wizard': dict(head=0.39, legs=0.95, width=0.76, robe=True),
    'stocky': dict(head=0.43, legs=0.83, width=0.94, robe=False),
    'skeleton': dict(head=0.42, legs=0.72, width=0.67, robe=False),
    'statue': dict(head=0.40, legs=0.78, width=0.89, robe=False),
    'monster': dict(head=0.47, legs=0.78, width=0.90, robe=False),
    'fairy': dict(head=0.30, legs=0.91, width=0.55, robe=True),
    'ganon': dict(head=0.35, legs=0.78, width=0.88, robe=False),
    'seated': dict(head=0.50, legs=0.88, width=0.84, robe=True),
}

FAMILIES = {
    'elder': 'elder', 'smithy': 'stocky', 'kid': 'child', 'father': 'seated',
    'hideout': 'human', 'thief-hideout': 'thief', 'sweep': 'human', 'camper': 'seated',
    'lumber': 'human', 'flute': 'child', 'stump': 'child', 'maze-lady': 'dress',
    'maze-man': 'human', 'fortune-light': 'elder', 'fortune-dark': 'elder',
    'brothers': 'human', 'snitch-young': 'human', 'inn': 'seated', 'witch': 'wizard',
    'middle': 'human', 'grandson': 'child', 'snitch-old': 'dress', 'soldier': 'guard',
    'blue': 'guard', 'green': 'guard', 'red': 'guard', 'fast': 'guard', 'spear': 'guard',
    'archer': 'guard', 'bush-archer': 'guard', 'javelin': 'guard', 'bush-javelin': 'guard',
    'bombguard': 'guard', 'recruit': 'guard', 'archery': 'seated',
    'queen-light': 'fairy', 'queen-dark': 'fairy', 'uncle': 'human', 'sage': 'elder',
    'running': 'human', 'merchant': 'seated', 'zelda': 'dress', 'wife': 'elder',
    'agahnim': 'large-wizard', 'maiden': 'dress', 'oldman': 'elder',
    'bombshop': 'stocky', 'kiki': 'monster', 'blind-maiden': 'dress', 'bully': 'monster',
    'shop-light': 'elder', 'shop-dark': 'stocky', 'chestgame': 'human',
    'nice-thief': 'thief', 'drink': 'seated', 'thief': 'thief', 'fairy': 'fairy',
    'dig': 'monster', 'ganon': 'ganon', 'assistant': 'human',
    'armos-knight': 'statue', 'armos-statue': 'statue', 'stalfos': 'skeleton',
    'gibdo': 'skeleton', 'wizzrobe': 'wizard', 'freezor': 'statue', 'pengator': 'monster',
    'eyegore-green': 'monster', 'eyegore-red': 'monster', 'blind-boss': 'large-wizard',
}

# These include poses not safely represented by a single upright anatomy model.
REVIEW_NOTES = {
    'kid': 'The catalogued assembly includes a large net/effect; verify body/prop ownership in-game.',
    'stump': 'Tree transformation needs a separate wood/branch model and a reference.',
    'camper': 'Seated/reclining pose: verify contact height and garment orientation.',
    'uncle': 'Fallen pose needs horizontal body geometry; existing authored data is preserved.',
    'fairy': 'Wings and long dress are approximations; a front/side 3D reference would help.',
    'queen-light': 'Wings and long dress are approximations; a front/side 3D reference would help.',
    'queen-dark': 'Unusual body silhouette and wings; a 3D reference would help.',
    'ganon': 'Large snout, tusks, horns and belly use approximate distinct surfaces; a 3D reference would help.',
    'blind-boss': 'Detached head and death/attack pieces need phase-specific geometry.',
    'agahnim': 'Broad sleeves, hood and casting effects need in-game phase review.',
    'lumber': 'Two bodies and a saw are modeled separately; inspect tool/hand boundaries.',
}


def variant_asset(asset):
    return any(k.startswith(('flip_', 'facing_')) or k in ('geometry_by_flip', 'geometry_by_facing')
               for k in asset) or bool(asset.get('tile_contexts'))


def asset_blocks(text, callback):
    return re.sub(r'(?ms)^\[\[assets\]\]\n.*?(?=^\[|\Z)',
                  lambda m: callback(m.group(), tomllib.loads(m.group())['assets'][0]), text)


def promote_link(profile, directory, validator):
    """Three-way asset merge of the approved proposal, never replace newer edits."""
    source = (directory/'source-profile.toml').read_text()
    proposal = (directory/'link-profile.toml').read_text()
    before = profile.read_text()
    old, approved, current = [tomllib.loads(t) for t in (source, proposal, before)]
    maps = [{a['tile_hash']: a for a in p.get('assets', [])} for p in (old, approved, current)]
    base, wanted, now = maps
    changes, conflicts = {}, []
    for h, asset in wanted.items():
        for k, v in asset.items():
            if k == 'tile_hash' or base.get(h, {}).get(k) == v:
                continue
            present = now.get(h, {})
            if k in present and present[k] == v:
                continue
            if present.get(k) != base.get(h, {}).get(k):
                conflicts.append([h, k])
            else:
                changes.setdefault(h, {})[k] = v
    if conflicts:
        raise ValueError(f'Link promotion conflicts with newer edits: {conflicts}')
    # Apply only fields actually added/changed in the approved Link candidate.
    def update(block, asset):
        fields = changes.get(asset['tile_hash'], {})
        for k, v in fields.items():
            line = f'{k} = {json.dumps(v)}\n'
            if k in asset:
                block = re.sub(rf'(?m)^{re.escape(k)}\s*=.*\n?', lambda _: line, block)
            else:
                block = block.rstrip()+'\n'+line
        return block+'\n' if fields else block
    result = asset_blocks(before, update)
    for h in sorted(set(changes)-set(now)):
        result += f'\n[[assets]]\ntile_hash = "{h}"\n'
        result += ''.join(f'{k} = {json.dumps(v)}\n' for k, v in changes[h].items())
    # Add the candidate's exact missing OBJ material classification, using the
    # existing fill-only exporter (it preserves all current rules and groups).
    result, _ = merge_profile(result, {})
    new_groups = {k: v for k, v in approved.get('asset_groups', {}).items()
                  if k not in old.get('asset_groups', {})}
    for name, group in new_groups.items():
        if name in current.get('asset_groups', {}):
            if current['asset_groups'][name] != group:
                raise ValueError(f'Newer asset group conflicts with Link promotion: {name}')
            continue
        result += f'\n[asset_groups.{name}]\n'
        result += ''.join(f'{k} = {json.dumps(v)}\n' for k, v in group.items())
    for rule in approved.get('rules', []):
        if rule not in old.get('rules', []) and rule not in current.get('rules', []):
            result += '\n[[rules]]\n'+''.join(f'{k} = {json.dumps(v)}\n' for k, v in rule.items())
    after = tomllib.loads(result)
    actual = {a['tile_hash']: a for a in after['assets']}
    for h, asset in now.items():
        for k, v in asset.items():
            assert actual[h][k] == changes.get(h, {}).get(k, v), (h, k)
    for name in current:
        if name not in ('assets', 'asset_groups', 'rules'):
            assert after[name] == current[name], name
    candidate = directory/'link-promotion-merged.toml'
    atomic_write(candidate, result)
    subprocess.run([str(validator.resolve()), str(candidate.resolve())], check=True)
    assert profile.read_text() == before, 'Profile changed during promotion'
    atomic_write(profile, result)
    return dict(changed_tiles=len(changes), changed_fields=sum(map(len, changes.values())),
                before_sha256=hashlib.sha256(before.encode()).hexdigest(),
                after_sha256=hashlib.sha256(result.encode()).hexdigest())


def assemble(pose):
    """Deduplicate exact OAM repeats but retain all canonical hidden artwork."""
    pixels, occurrences, seen = {}, [], set()
    for part in pose['parts']:
        signature = (part['tile_hash'], part['x'], part['y'], part['hflip'], part['vflip'])
        if signature in seen:
            continue
        seen.add(signature)
        for i, ink in enumerate(part['indices']):
            if not ink:
                continue
            x = part['x']+(7-i%8 if part['hflip'] else i%8)
            y = part['y']+(7-i//8 if part['vflip'] else i//8)
            p = dict(part=part, pixel=i, x=x, y=y, ink=ink, color=part['colors'][i])
            occurrences.append(p)
            pixels.setdefault((x, y), p)
    return pixels, occurrences


def connected_components(mask):
    remaining, components = set(mask), []
    while remaining:
        seed = remaining.pop()
        stack, component = [seed], {seed}
        while stack:
            x, y = stack.pop()
            for q in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                if q in remaining:
                    remaining.remove(q)
                    stack.append(q)
                    component.add(q)
        components.append(component)
    return components


def surface_color(color, family):
    r, g, b = [(color>>s&31)/31 for s in (0, 5, 10)]
    # RGB hue identifies physical materials only. Brightness is not relief.
    if family not in ('statue', 'skeleton', 'monster', 'ganon') and r > 0.48 and r > g*1.12 and g > b*1.15:
        return 'skin'
    if max(r, g, b)-min(r, g, b) < 0.16:
        return 'neutral'
    return 'cloth'


def segment(actor, pose):
    family = FAMILIES[actor['draw_model']['key']]
    recipe = RECIPES[family]
    pixels, occurrences = assemble(pose)
    xs, ys = zip(*pixels)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    # Body center from broad rows, not the tip of a spear or a raised arm.
    rows = defaultdict(list)
    for x, y in pixels:
        rows[y].append(x)
    broad = sorted(rows, key=lambda y: len(rows[y]), reverse=True)[:max(3, len(rows)//2)]
    cx = float(np.median([float(np.median(rows[y])) for y in broad]))
    height = y1-y0+1
    body_width = float(np.median([len(rows[y]) for y in broad]))*recipe['width']
    radius = max(3, body_width/2)
    head_end = y0+height*recipe['head']
    leg_start = y0+height*recipe['legs']
    # Separate both lumberjacks around the actual gap, not one giant torso.
    centers = [cx]
    if actor['draw_model']['key'] == 'lumber':
        centers = [x0+(x1-x0)*0.20, x0+(x1-x0)*0.80]
        radius = max(4, (x1-x0)*0.17)
    props = set()
    # Detached narrow parts outside the torso are tools, not anatomy. Large
    # fairies retain wings as shallow surfaces, never as a cylindrical torso.
    for component in connected_components(pixels):
        xx, yy = zip(*component)
        if len(component) >= 4 and (max(xx)<cx-radius*0.9 or min(xx)>cx+radius*0.9):
            if max(xx)-min(xx) > 2*(max(yy)-min(yy)+1) or max(yy)-min(yy) > 2*(max(xx)-min(xx)+1):
                props |= component
    result = []
    for p in occurrences:
        x, y = p['x'], p['y']
        center = min(centers, key=lambda c: abs(x-c))
        side = abs(x-center)/radius
        zone = 'head' if y < head_end else 'robe' if recipe['robe'] else 'torso' if y < leg_start else 'leg'
        material = surface_color(p['color'], family)
        if family == 'fairy' and side > 1.15:
            zone = 'wing'
        elif (x, y) in props:
            zone = 'tool'
        elif side > 1.0 and head_end-height*0.05 < y < leg_start:
            zone = 'arm'
        elif zone == 'leg' and y >= y1-max(2, height*0.10):
            zone = 'boot'
        # Semantic palette details (outline, eyes, highlights) inherit an
        # anatomical surface by position; they don't create isolated bumps.
        p = dict(p, zone=zone, material=material, center=center)
        result.append(p)
    # Skin patches (face, ears, hands) have their own rounded relief. Hue is
    # used only to segment physical surfaces. Ignore dark outline pixels during
    # segmentation, then attach them to adjacent surfaces so facial line art
    # does not become a collection of sharp bumps.
    for p in result:
        if p['ink'] in (1, 5):
            neighbors = [q for q in result if q['zone'] == p['zone'] and q['ink'] not in (1, 5)
                         and (q['x']-p['x'])**2+(q['y']-p['y'])**2 <= 5]
            if neighbors:
                nearest = min(neighbors, key=lambda q: (q['x']-p['x'])**2+(q['y']-p['y'])**2)
                p['material'] = nearest['material']
    surfaces = defaultdict(set)
    for p in result:
        surfaces[p['zone'], p['material']].add((p['x'], p['y']))
    for p in result:
        patch = surfaces[p['zone'], p['material']]
        px, py = zip(*patch)
        p['surface_bounds'] = [min(px), min(py), max(px), max(py)]
    return dict(actor=actor['name'], family=family, recipe=recipe, pixels=pixels,
                occurrences=result, head_end=head_end, leg_start=leg_start,
                cx=cx, radius=radius, y0=y0, y1=y1, height=height,
                facing=pose['facing'])


def model_pixel(scene, p):
    x, y, zone = p['x'], p['y'], p['zone']
    radius, center = scene['radius'], p['center']
    height = scene['height']
    u = float(np.clip((x-center)/radius, -0.95, 0.95))
    family = scene['family']
    # All models use world Z up and +Y toward viewer. Geometry is independent
    # of where a debug preview camera happens to look.
    if zone == 'head':
        v = float(np.clip((y-scene['y0'])/max(1, scene['head_end']-scene['y0']), 0, 1))
        crown = max(0, 1-v/0.62)
        n = unit((u*0.78, 0.22+0.78*(1-crown), 0.98*crown+0.16))
        if crown > 0.85 and abs(u) < 0.5:
            n = unit((u*0.35, -0.10, 0.99))
        if family in ('guard', 'statue'):
            # Planar helmet/stone brow, bevelled at outer edges.
            if 0.55 < v < 0.72 and abs(u) < 0.70:
                n = unit((0, 0.98, 0.18))
        if family == 'skeleton' and v > 0.55:
            n = unit((u*0.45, 0.94, 0.20))
        if p['material'] == 'skin':
            # A visible face is a separate rounded front under the hat/hood.
            # Its top does not inherit the cap's upward dome normal.
            sx0, sy0, sx1, sy1 = p['surface_bounds']
            su = float(np.clip((x-(sx0+sx1)/2)/max(2, (sx1-sx0+1)/2), -0.9, 0.9))
            sv = float(np.clip((y-sy0)/max(1, sy1-sy0), 0, 1))
            n = unit((su*0.60, 0.92, 0.36-sv*0.22))
        local_height = scene['y1']-y + 0.9*math.sqrt(max(0, 1-u*u))
    elif zone in ('tool', 'wing'):
        # Broad planar prop face with edge bevel. A wing is shallow relief,
        # not a continuation of the abdomen or a separate opaque ball.
        n = unit((0.12*math.copysign(1, u), 0.86 if zone == 'tool' else 0.95,
                  0.49 if zone == 'tool' else 0.29))
        local_height = scene['y1']-y
    elif zone == 'arm':
        # Independent sleeve/arm cylinder with upward shoulder cap.
        sign = -1 if x<center else 1
        arm_center = center+sign*radius*1.03
        au = float(np.clip((x-arm_center)/max(2, radius*0.35), -0.9, 0.9))
        shoulder = max(0, 1-(y-scene['head_end'])/max(2, height*0.20))
        n = unit((au*0.7, 0.85, 0.18+0.65*shoulder))
        if p['material'] == 'skin':
            sx0, sy0, sx1, sy1 = p['surface_bounds']
            hu = float(np.clip((x-(sx0+sx1)/2)/max(1.5, (sx1-sx0+1)/2), -0.9, 0.9))
            n = unit((hu*0.62, 0.80, 0.48))
        local_height = scene['y1']-y+0.4
    elif zone in ('leg', 'boot'):
        sign = -1 if x<center else 1
        leg_center = center+sign*radius*0.43
        lu = float(np.clip((x-leg_center)/max(2, radius*0.40), -0.9, 0.9))
        top = zone == 'boot' and y < scene['y1']-height*0.035
        n = unit((lu*0.65, 0.60 if top else 0.90, 0.76 if top else 0.20))
        local_height = max(0, scene['y1']-y)+(0.6 if top else 0)
    else:
        # Robes/dresses remain continuous across the center; normal cloth fold
        # relief is small, never a deep split pretending a skirt is two legs.
        if zone == 'robe':
            fold = 0.09*math.sin((x-center)*math.pi/max(2, radius*0.35))
            n = unit((u*0.62+fold, 0.91, 0.20))
        elif family in ('statue', 'guard') and abs(u) < 0.55:
            n = unit((0.10*u, 0.97, 0.24))
        else:
            n = unit((u*0.68, 0.88, 0.27))
        local_height = scene['y1']-y+0.6*math.sqrt(max(0, 1-u*u))
    return n, int(round(max(0, local_height)))


def generate(manifest, text):
    parsed = tomllib.loads(text)
    assets = {a['tile_hash']: a for a in parsed.get('assets', [])}
    actors = [a for a in manifest['actors'] if a['draw_model']['key'] in FAMILIES]
    owners = defaultdict(set)
    for a in manifest['actors']:
        for pose in a['poses']:
            for p in pose['parts']:
                owners[p['tile_hash']].add(a['name'])
    selected_names = {a['name'] for a in actors}
    selected_hashes = {p['tile_hash'] for a in actors for f in a['poses'] for p in f['parts']}
    blocked = {}
    bg = set(manifest['background_shared_hashes'])
    for h in selected_hashes:
        if h in bg:
            blocked[h] = 'Shared with background artwork'
        elif owners[h]-selected_names:
            blocked[h] = 'Shared with excluded family: '+', '.join(sorted(owners[h]-selected_names))
        elif variant_asset(assets.get(h, {})):
            blocked[h] = 'Existing authored geometry variants preserved'
    # Named contexts are separate profile records, not asset keys.
    for record in parsed.get('tile_contexts', []):
        if record['tile_hash'] in selected_hashes:
            blocked[record['tile_hash']] = 'Existing named geometry contexts preserved'
    proposals = defaultdict(lambda: defaultdict(list))
    seen = set()
    inherited = []
    deferred = []
    scenes = []
    for a in actors:
        for pose in a['poses']:
            if a.get('graphics_inherited'):
                inherited.append(a['id'])
                continue
            # No upright cylinder on a tree, prone body or detached boss pieces.
            key = a['draw_model']['key']
            if key == 'stump' or (key == 'uncle' and pose['action'] == 'fallen') or (key == 'blind-boss' and pose['step'] > 0):
                deferred.append(dict(actor=a['name'], facing=pose['facing'], step=pose['step'], action=pose['action']))
                continue
            signature = (key, pose['facing'], tuple((p['tile_hash'], p['x'], p['y'], p['hflip'], p['vflip']) for p in pose['parts']))
            if signature in seen:
                continue
            seen.add(signature)
            scene = segment(a, pose)
            scenes.append(scene)
            for p in scene['occurrences']:
                h = p['part']['tile_hash']
                if h in blocked:
                    continue
                n, height = model_pixel(scene, p)
                part = p['part']
                flip = (int(part['hflip']), int(part['vflip']))
                # Existing detailed normals supply exact per-pixel reference;
                # do not train on uniform temporary fills.
                proposals[h][flip, p['pixel']].append((n, height, p['zone'], a['name']))
    edits, overrides, details, disagreements = {}, {}, {}, []
    for h, values in sorted(proposals.items()):
        original = assets.get(h, {})
        need_n = 'normal_xyz' not in original or placeholder(original)
        need_h = 'height' not in original
        if not (need_n or need_h):
            continue
        by_flip = defaultdict(dict)
        zones = Counter()
        for (flip, i), samples in values.items():
            n = unit(np.mean([v[0] for v in samples], axis=0))
            heights = [v[1] for v in samples]
            angle = max(math.degrees(math.acos(float(np.clip(v[0]@n, -1, 1)))) for v in samples)
            if max(heights)-min(heights)>2 or angle>30:
                disagreements.append(dict(tile_hash=h, flip=list(flip), pixel=i,
                    height_spread=max(heights)-min(heights), normal_max_angle=round(angle, 2),
                    actors=sorted({v[3] for v in samples})))
            by_flip[flip][i] = (n, int(round(float(np.median(heights)))))
            zones.update(v[2] for v in samples)
        ns, hs = [128, 128, 255]*64, [0]*64
        # Horizontal mirrors invert X naturally. Vertical artwork mirrors may
        # not invert viewer-facing Y, hence explicit gameplay-space flip sets.
        flip_specific = any(flip[1] for flip in by_flip)
        for i in range(64):
            samples = [(flip, data[i]) for flip, data in by_flip.items() if i in data]
            if not samples:
                continue
            vectors = []
            for flip, (n, _) in samples:
                n = n.copy()
                if not original.get('normal_ignore_tile_transform'):
                    n[0] *= -1 if flip[0] else 1
                    n[1] *= -1 if flip[1] else 1
                vectors.append(n)
            ns[i*3:i*3+3] = encode(np.mean(vectors, axis=0))
            hs[i] = int(round(float(np.median([data[1] for _, data in samples]))))
        layers = {}
        if need_n:
            layers['normal_xyz'] = ns
        if need_h:
            layers['height'] = hs
        edits[h] = layers
        if flip_specific and need_n:
            extra = {'geometry_by_flip': True}
            for flip, data in by_flip.items():
                name = 'both' if all(flip) else 'horz' if flip[0] else 'vert' if flip[1] else 'normal'
                fn, fh = [128, 128, 255]*64, [0]*64
                for i, (n, height) in data.items():
                    fn[i*3:i*3+3] = encode(n)
                    fh[i] = height
                extra[f'flip_{name}_normal_xyz'] = fn
                if need_h:
                    extra[f'flip_{name}_height'] = fh
            overrides[h] = extra
        details[h] = dict(owners=sorted(owners[h]), layers=list(layers), zones=dict(zones),
                          replaced_placeholder=placeholder(original), flip_specific=flip_specific and need_n)
    def remove(block, asset):
        if asset['tile_hash'] in edits and placeholder(asset):
            return re.sub(r'(?m)^normal_xyz\s*=.*\n?', '', block)
        return block
    cleaned = asset_blocks(text, remove)
    candidate, _ = merge_profile(cleaned, edits)
    def add(block, asset):
        extra = overrides.get(asset['tile_hash'], {})
        return block.rstrip()+'\n'+''.join(f'{k} = {json.dumps(v)}\n' for k, v in extra.items())+'\n' if extra else block
    candidate = asset_blocks(candidate, add)
    new = tomllib.loads(candidate)
    new_assets = {a['tile_hash']: a for a in new.get('assets', [])}
    for h, a in assets.items():
        for k, v in a.items():
            if h in edits and k == 'normal_xyz' and placeholder(a):
                continue
            assert new_assets[h][k] == v, (h, k)
    # Excluded actors, including approved Link, must be bit-for-bit unchanged.
    for h in set(owners)-selected_hashes | set(blocked):
        assert assets.get(h) == new_assets.get(h), h
    per_family = []
    for name in sorted(selected_names):
        aa = [a for a in actors if a['name'] == name]
        hashes = {p['tile_hash'] for a in aa for pose in a['poses'] for p in pose['parts']}
        per_family.append(dict(name=name, recipe=FAMILIES[aa[0]['draw_model']['key']],
            poses=sum(len(a['poses']) for a in aa), unique_tiles=len(hashes),
            changed_tiles=len(hashes & set(edits)), blocked_tiles={h: blocked[h] for h in sorted(hashes & set(blocked))},
            missing_heights=sorted(h for h in hashes if 'height' not in new_assets.get(h, {})),
            missing_normals=sorted(h for h in hashes if 'normal_xyz' not in new_assets.get(h, {}) or placeholder(new_assets[h])),
            review_note=REVIEW_NOTES.get(aa[0]['draw_model']['key'])))
    report = dict(source_sha256=hashlib.sha256(text.encode()).hexdigest(),
        selected_families=len(selected_names), selected_contexts=len(actors), selected_unique_tiles=len(selected_hashes),
        changed_tiles=len(edits), added_heights=sum('height' in v for v in edits.values()),
        added_normals=sum('normal_xyz' in v and 'normal_xyz' not in assets.get(h, {}) for h, v in edits.items()),
        replaced_placeholders=sum(v['replaced_placeholder'] for v in details.values()),
        flip_specific_tiles=len(overrides), blocked_tiles=blocked, inherited_contexts=sorted(set(inherited)),
        deferred_poses=deferred, reuse_disagreement_samples=len(disagreements),
        reuse_disagreement_tiles=len({d['tile_hash'] for d in disagreements}), reuse_disagreements=disagreements,
        families=per_family, tiles=details,
        limitations=['Part-model proposals, not recovered ground-truth 3D geometry.',
            'Existing heights, non-placeholder normals, geometry variants and named contexts are preserved.',
            'No reference transfer from Link onto unrelated anatomy; only the approved modeling principles are reused.',
            'Palette hue identifies material families; brightness never becomes relief.',
            'Shared estimates use mean normals/median heights; disagreements are reported, not proven solved.',
            'Deferred poses and hash sharing exclusions leave explicit coverage gaps.',
            'Generated-ramp-only runtime placement alignment does not apply to custom height maps.',
            'Offline neutral lighting is not live animation/GI validation.'])
    return candidate, report, actors, new_assets


def review(output, actors, assets):
    selected, seen = [], set()
    for a in actors:
        for pose in a['poses']:
            signature = (a['name'], pose['facing'], tuple((p['tile_hash'], tuple(p['colors'])) for p in pose['parts']))
            # Review each family/facing and distinct palette variant, first frame.
            if pose['step'] != 0 or signature in seen:
                continue
            seen.add(signature)
            selected.append((a, pose))
    width, rh = 896, 168
    index = []
    page = bytearray()
    for row, (a, pose) in enumerate(selected):
        if row % 10 == 0:
            page = bytearray([28, 30, 35]*width*rh*min(10, len(selected)-row))
        x0, y0, x1, y1 = pose['bounds']
        scale = max(1, min(4, 148//max(1, y1-y0), 200//max(1, x1-x0)))
        pixels, _ = assemble(pose)
        for (x, y), p in pixels.items():
            part, i = p['part'], p['pixel']
            asset = assets.get(part['tile_hash'], {})
            name = 'both' if part['hflip'] and part['vflip'] else 'horz' if part['hflip'] else 'vert' if part['vflip'] else 'normal'
            explicit = asset.get('geometry_by_flip') and f'flip_{name}_normal_xyz' in asset
            normals = asset[f'flip_{name}_normal_xyz'] if explicit else asset.get('normal_xyz')
            heights = asset.get(f'flip_{name}_height', asset.get('height')) if asset.get('geometry_by_flip') else asset.get('height')
            art = [(p['color']>>s&31)*255//31 for s in (0, 5, 10)]
            if normals:
                n = unit(np.array(normals[i*3:i*3+3])/127.5-1)
                if not explicit and not asset.get('normal_ignore_tile_transform'):
                    n[0] *= -1 if part['hflip'] else 1
                    n[1] *= -1 if part['vflip'] else 1
                normal_color = encode(n)
                light = round(255*(0.18+0.82*max(0, float(n@unit((-0.5, 0.5, 0.85))))))
                neutral = [light]*3
            else:
                normal_color = neutral = [255, 0, 255]
            h = heights[i] if heights else None
            hcolor = [max(0, min(255, round(h/40*255)))]*3 if h is not None else [255, 0, 255]
            for col, color in enumerate((art, normal_color, hcolor, neutral)):
                xx = col*224+12+(x-x0)*scale
                yy = (row%10)*rh+8+(y-y0)*scale
                for dy in range(scale):
                    for dx in range(scale):
                        at = ((yy+dy)*width+xx+dx)*3
                        page[at:at+3] = bytes(color)
        index.append(dict(page=row//10+1, row=row%10, name=a['name'], context=a['id'],
                          facing=pose['facing'], step=pose['step'], action=pose['action']))
        if row%10 == 9 or row == len(selected)-1:
            png(output/f'review-{row//10+1:02}.png', width, len(page)//(width*3), page)
    atomic_write(output/'review-index.json', json.dumps(index, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=Path('build/remaster-geometry/npc-workbench/poses.json'))
    parser.add_argument('--profile', type=Path, default=Path('remaster/profiles/ALttP/alttp-profile.toml'))
    parser.add_argument('--output', type=Path, default=Path('build/remaster-geometry/humanoid-geometry'))
    parser.add_argument('--promote-approved-link', action='store_true')
    args = parser.parse_args()
    validator = args.manifest.parent/'alttp-profile-validate'
    args.output.mkdir(parents=True, exist_ok=True)
    if args.promote_approved_link:
        promotion = promote_link(args.profile, Path('build/remaster-geometry/link-geometry'), validator)
        atomic_write(args.output/'link-promotion.json', json.dumps(promotion, indent=2)+'\n')
        print('Approved Link promotion:', json.dumps(promotion))
    source = args.profile.read_text()
    candidate, report, actors, assets = generate(json.loads(args.manifest.read_text()), source)
    target = args.output/'humanoid-profile.toml'
    atomic_write(target, candidate)
    subprocess.run([str(validator.resolve()), str(target.resolve())], check=True)
    review(args.output, actors, assets)
    assert args.profile.read_text() == source
    atomic_write(args.output/'source-profile.toml', source)
    atomic_write(args.output/'report.json', json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('families', 'tiles', 'reuse_disagreements', 'deferred_poses', 'blocked_tiles')}, indent=2))


if __name__ == '__main__':
    main()
