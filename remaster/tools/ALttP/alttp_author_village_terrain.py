"""Compile the approved village/river terrain into sparse coordinate offsets.

No asset/context arrays are edited. Only explicitly floor-marked assets receive
per-pixel channels. All other roles retain local geometry plus a scalar base.
Output is a separate review profile, never an in-place canonical modification.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import tomllib
import numpy as np
from PIL import Image, ImageDraw


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def floor_residual(role, delta, scalar):
    """Unknown roles are not permission to deform a placement."""
    return np.round(delta-scalar, 6) if role == 'floor' else np.zeros((8, 8))


def run(profile_path, terrain_dir, assembly_dir, output):
    output.mkdir(parents=True, exist_ok=False)
    source = profile_path.read_text()
    profile = tomllib.loads(source)
    (output / 'source.toml').write_text(source)
    terrain = np.load(terrain_dir / 'terrain-evidence.npz')
    approved = terrain['height'].astype(float)
    assert approved.shape == (1024, 1024) and np.isfinite(approved).all()
    records = json.loads((assembly_dir / 'placements.json').read_text())
    records = [r for r in records if 0 <= r['x'] < 128 and 192 <= r['y'] < 320]
    assert len(records) == 16384
    assets = {r['tile_hash']: r for r in profile['assets']}
    existing_scalars = {(r['x'], r['y']): r['offset'] for r in profile.get('overworld_offsets', [])
                        if r['world'] == 'light' and r['background'] == 1}
    contexts = {}
    for r in profile.get('tile_contexts', []):
        contexts.setdefault(r['tile_hash'], {})[r['name']] = r
    bindings = {(r['x'], r['y']): r for r in profile.get('context_bindings', [])
                if r['area'] == 320 and r['background'] == 1}
    offsets = []
    pixels = []
    audit = []
    effective = np.full_like(approved, np.nan)
    adjustment = np.zeros_like(approved)
    local_field = np.full_like(approved, np.nan)
    policy = np.zeros(approved.shape, np.uint8)
    yy, xx = np.mgrid[:8, :8]
    design = np.stack((np.ones(64), xx.ravel(), yy.ravel()), axis=1)
    # Explicit approved object footprints: rigid authored geometry must not bend
    # with the supporting terrain. World-local village coordinates.
    buildings = [(156,301,258,400),(575,108,738,205),(779,366,891,459),
                 (348,588,481,706),(586,587,697,696),(780,571,892,672),
                 (600,778,706,930),(413,794,484,885),(76,842,149,927)]
    trees = [(335,117,419,205),(854,32,924,131),(934,0,1024,105),
             (165,878,255,987),(843,776,920,867),(937,909,1024,1024)]
    rigid = buildings + trees
    for r in records:
        x, y = r['x']*8, (r['y']-192)*8
        region = np.s_[y:y+8, x:x+8]
        asset = assets.get(r['hash'], {})
        g = asset
        cs = contexts.get(r['hash'], {})
        binding = bindings.get((r['x'], r['y']))
        if binding and binding['tile_hash'] == r['hash']:
            g = cs[binding['context']]
        elif len(cs) == 1:
            g = next(iter(cs.values()))
        flip = ['normal', 'horz', 'vert', 'both'][int(r['flip_x'])+2*int(r['flip_y'])]
        values = g.get('height', asset.get('height'))
        if g is asset and asset.get('geometry_by_flip'):
            values = asset.get('flip_'+flip+'_height', values)
        local = np.array(values, float).reshape(8, 8) if values is not None else None
        if local is not None:
            if r['flip_x']:
                local = local[:, ::-1]
            if r['flip_y']:
                local = local[::-1]
            local_field[region] = local
        target = approved[region].copy()
        box = next((b for b in rigid if b[0] <= x+4 < b[2] and b[1] <= y+4 < b[3]), None)
        role = asset.get('tile_role')
        structural = box is None and role == 'wall' and (
            terrain['bank_face'][region].any() or
            ((x < 260 and 136 <= y <= 232) or (330 <= x <= 824 and 192 <= y <= 312)))
        stairs = box is None and 432 <= x < 480 and 256 <= y < 304
        if box is not None:
            bx0, by0, bx1, by1 = box
            contact = approved[min(1023, by1), bx0:bx1]
            target.fill(float(np.median(contact)))
            mode = 'rigid_object_contact'
        elif structural or stairs:
            mode = 'structural_ramp_attachment'
        elif role in ('obstacle', 'door', 'doorway'):
            target.fill(float(np.median(approved[min(1023, y+8), x:x+8])))
            mode = 'small_object_tile_contact'
        else:
            mode = 'terrain_plus_local_relief'
        # A plane captures pre-existing 0..7 wall ramp geometry; subtract that
        # carrier only. Residual authored detail stays additive and unchanged.
        carrier = np.zeros((8, 8))
        if (structural or stairs) and local is not None:
            carrier = (design @ np.linalg.lstsq(design, local.ravel(), rcond=None)[0]).reshape(8, 8)
        delta = np.round(target-carrier, 6)
        scalar = int(np.rint(np.median(delta)))
        # Keep accepted placement bases when refining an already-authored area.
        scalar = existing_scalars.get((r['x'], r['y']), scalar)
        residual = floor_residual(role, delta, scalar)
        if scalar:
            offsets.append(dict(world='light', background=1, x=r['x'], y=r['y'], offset=scalar))
        if np.any(residual != 0):
            pixels.append(dict(world='light', background=1, x=r['x'], y=r['y'],
                               height_offset=residual.ravel().tolist()))
        adjustment[region] = scalar+residual
        if local is not None:
            effective[region] = local+scalar+residual
        policy[region] = {'terrain_plus_local_relief':1, 'structural_ramp_attachment':2,
                          'rigid_object_contact':3, 'small_object_tile_contact':4}[mode]
        audit.append(dict(x=r['x'], y=r['y'], hash=r['hash'], role=role, mode=mode,
                          local_present=local is not None, scalar=scalar,
                          pixel_field=bool(np.any(residual != 0)),
                          target_min=float(target.min()), target_max=float(target.max()),
                          max_export_error=float(np.max(abs(scalar+residual+carrier-target)))))
    # Replace ONLY this Light BG2 coordinate rectangle. Preserve other worlds,
    # backgrounds, settings, and every authored local asset/context record.
    def in_area(r):
        return r['world'] == 'light' and r['background'] == 1 and 0 <= r['x'] < 128 and 192 <= r['y'] < 320
    text = re.sub(r'^schema_version\s*=\s*\d+', 'schema_version = '+str(max(38, profile['schema_version'])), source, count=1)
    for name in ('overworld_offsets', 'overworld_terrain'):
        def keep(m):
            return '' if in_area(tomllib.loads(m[0])[name][0]) else m[0]
        text = re.sub(r'^\[\['+name+r'\]\][\s\S]*?(?=^\[|\Z)', keep, text, flags=re.M)
    for name, entries in [('overworld_offsets', offsets), ('overworld_terrain', pixels)]:
        for r in entries:
            text += '\n[['+name+']]\n'+''.join(k+' = '+json.dumps(v, separators=(',', ':'))+'\n' for k,v in r.items())
    result = tomllib.loads(text)
    for name in profile.keys()-{'schema_version','overworld_offsets','overworld_terrain'}:
        assert profile[name] == result[name], name
    for name in ('overworld_offsets', 'overworld_terrain'):
        assert [r for r in profile.get(name, []) if not in_area(r)] == [r for r in result.get(name, []) if not in_area(r)]
    assert all(r['offset'] != 0 for r in offsets)
    assert all(any(v != 0 for v in r['height_offset']) for r in pixels)
    assert max(r['max_export_error'] for r in audit if r['role'] == 'floor') < 1e-6
    assert all(not r['pixel_field'] for r in audit if r['role'] != 'floor')
    dest = output/'alttp-kakariko-village-river-offsets-proposed.toml'
    dest.write_text(text)
    np.savez_compressed(output/'authored-fields.npz',approved=approved,adjustment=adjustment,
                        effective=effective,local=local_field,policy=policy)
    (output/'placements.json').write_text(json.dumps(audit,indent=2))
    art = Image.open(terrain_dir/'village-map.png').convert('RGB')
    def gray(z):
        v=np.rint(np.clip((np.nan_to_num(z)+32)/112,0,1)*255).astype('uint8')
        rgb=np.repeat(v[:,:,None],3,axis=2);rgb[~np.isfinite(z)]=(200,0,200)
        return Image.fromarray(rgb)
    review=Image.new('RGB',(3072,1080),'#181d23');d=ImageDraw.Draw(review)
    for j,(label,image) in enumerate([('Artwork',art),('Coordinate offsets (-32..80)',gray(adjustment)),('Effective local + offsets; magenta = missing local',gray(effective))]):
        review.paste(image,(j*1024,40));d.text((j*1024+10,12),label,fill='white')
    review.save(output/'review.png')
    report=dict(source=str(profile_path),source_sha256=digest(profile_path),approved_terrain_sha256=digest(terrain_dir/'terrain-height.npy'),
                proposal_sha256=digest(dest),world_rect=[0,1536,1024,1024],placements=len(audit),
                nonzero_scalars=len(offsets),per_pixel_height_fields=len(pixels),
                empty_default_coordinates=sum(r['scalar']==0 and not r['pixel_field'] for r in audit),
                missing_local_height=sum(not r['local_present'] for r in audit),
                max_floor_export_error=max(r['max_export_error'] for r in audit if r['role'] == 'floor'),
                nonfloor_per_pixel_fields=0,
                nonfloor_scalar_attachment_error=max(r['max_export_error'] for r in audit if r['role'] != 'floor'),
                mode_counts={mode:sum(r['mode']==mode for r in audit) for mode in sorted(set(r['mode'] for r in audit))},
                authored_local_data_and_outside_coordinates_preserved=True,
                limitations=['Mixed object/ground tiles use contact policy and need live review',
                             'Non-floor surfaces use integer scalar bases only; their authored local shapes are not fitted to the conceptual terrain silhouette',
                             'Missing local geometry remains missing; this is not complete geometry coverage',
                             'No new normal offsets authored; existing authored normals preserved',
                             'No live GPU shadows or actor grounding validation'])
    (output/'REPORT.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--terrain',type=Path,required=True)
    p.add_argument('--assembly',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    run(a.profile,a.terrain,a.assembly,a.output)
