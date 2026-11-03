"""Add sparse terrain-normal rotations to the approved village height export.

Only floor-marked placements already carrying height_offset are eligible. Differentiate the
assembled approved support, with separate ground/bank/water surfaces, rather than
tile residuals (whose scalar decomposition is arbitrary). Non-floor input pixel
fields are rejected. Local authored normals are never
rewritten. Outputs are proposals, not in-place canonical edits.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import tomllib

import numpy as np
from PIL import Image, ImageDraw


def gradient(height, labels):
    """Central/one-sided assembled differences; never cross a surface boundary."""
    result = []
    for axis in (1, 0):
        plus = np.roll(height, -1, axis=axis)
        minus = np.roll(height, 1, axis=axis)
        pok = (labels == np.roll(labels, -1, axis=axis)) & (labels != 0)
        mok = (labels == np.roll(labels, 1, axis=axis)) & (labels != 0)
        if axis == 1:
            pok[:, -1] = False
            mok[:, 0] = False
        else:
            pok[-1] = False
            mok[0] = False
        d = np.zeros_like(height, dtype=float)
        both = pok & mok
        d[both] = (plus[both] - minus[both]) / 2
        forward = pok & ~mok
        back = mok & ~pok
        d[forward] = plus[forward] - height[forward]
        d[back] = height[back] - minus[back]
        result.append(d)
    return result


def height_normal(gx, gy):
    n = np.stack((-gx, -gy, np.ones_like(gx)), axis=-1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def carrier_rotation(before, after):
    """Return R(+Z), with R shortest-arc from +Z, satisfying R(before)=after.

    The quaternion's Z component is zero (the runtime's no-twist constraint).
    Both input carrier normals have positive Z, so their summed Z is nonzero.
    """
    s = before[..., 2] + after[..., 2]
    qx = -(after[..., 1] - before[..., 1]) / s
    qy = (after[..., 0] - before[..., 0]) / s
    w = 1 / np.sqrt(1 + qx*qx + qy*qy)
    qx *= w
    qy *= w
    return np.stack((2*w*qy, -2*w*qx, 1-2*(qx*qx+qy*qy)), axis=-1)


def rotate(support, detail):
    v = np.stack((-support[..., 1], support[..., 0], np.zeros_like(support[..., 0])), axis=-1)
    a = np.cross(v, detail)
    return detail + a + np.cross(v, a) / (1+support[..., 2:3])


def key(r):
    return r['world'], r['background'], r['x'], r['y']


def inside(r):
    return r['world'] == 'light' and r['background'] == 1 and 0 <= r['x'] < 128 and 192 <= r['y'] < 320


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(source, heights, terrain, output):
    raw = source.read_text()
    profile = tomllib.loads(raw)
    fields = np.load(heights / 'authored-fields.npz')
    evidence = np.load(terrain / 'terrain-evidence.npz')
    policies = json.loads((heights / 'placements.json').read_text())
    assets = {r['tile_hash']: r for r in profile['assets']}
    by_coordinate = {(p['x'], p['y']): p for p in policies}
    approved = evidence['height'].astype(float)
    assert np.array_equal(approved, fields['approved'])
    rows = [r for r in profile.get('overworld_terrain', []) if inside(r) and 'height_offset' in r]
    eligible = {key(r): r for r in rows}
    for r in rows:
        p = by_coordinate[r['x'], r['y']]
        if assets.get(p['hash'], {}).get('tile_role') != 'floor':
            raise ValueError('Per-pixel heights on non-floor coordinate; regenerate floor-only heights first')
    scalars = {key(r): r['offset'] for r in profile.get('overworld_offsets', [])}
    reconstructed = np.zeros_like(approved)
    for p in policies:
        x, y = p['x']*8, (p['y']-192)*8
        k = ('light', 1, p['x'], p['y'])
        r = eligible.get(k, {})
        reconstructed[y:y+8, x:x+8] = scalars.get(k, 0) + np.array(r.get('height_offset', [0]*64)).reshape(8, 8)
    # Native publication rounded float payloads; tolerate only sub-micro-unit
    # serialization error, not authored height changes.
    if not np.allclose(reconstructed, fields['adjustment'], rtol=0, atol=1e-6):
        raise ValueError('Source heights differ from the approved height export; refusing stale provenance')
    # Full connected support behind props avoids tile-foot scalar discontinuities
    # entering ground normals. Bank faces and water never bleed into the hillside.
    labels = np.ones(approved.shape, dtype=np.uint8)
    labels[evidence['bank_face']] = 2
    labels[evidence['river_water']] = 3
    floor_mask = np.zeros(approved.shape, dtype=bool)
    for p in policies:
        if assets.get(p['hash'], {}).get('tile_role') == 'floor':
            x, y = p['x']*8, (p['y']-192)*8
            floor_mask[y:y+8, x:x+8] = True
    labels[~floor_mask] = 0
    gx, gy = gradient(approved, labels)
    target = height_normal(gx, gy)
    support = np.zeros_like(target)
    support[..., 2] = 1
    carrier = support.copy()
    written = {}
    stats = []
    yy, xx = np.mgrid[:8, :8]
    design = np.stack((np.ones(64), xx.ravel(), yy.ravel()), axis=1)
    for p in policies:
        k = ('light', 1, p['x'], p['y'])
        if k not in eligible:
            continue
        x, y = p['x']*8, (p['y']-192)*8
        sl = np.s_[y:y+8, x:x+8]
        mode = p['mode']
        before = carrier[sl].copy()
        after = target[sl].copy()
        if mode in ('rigid_object_contact', 'small_object_tile_contact'):
            # A constant fractional offset translates an object; it doesn't tilt it.
            after = before.copy()
        elif mode == 'structural_ramp_attachment' and p['local_present']:
            coefficients = np.linalg.lstsq(design, fields['local'][sl].ravel(), rcond=None)[0]
            before[:] = height_normal(np.full((8, 8), coefficients[1]), np.full((8, 8), coefficients[2]))
        n = carrier_rotation(before, after)
        n = np.round(n, 9)
        n[np.abs(n) < 1e-9] = 0
        if np.max(np.abs(n - [0, 0, 1])) < 1e-8:
            n[:] = [0, 0, 1]
        else:
            written[k] = n.ravel().tolist()
        support[sl] = n
        carrier[sl] = before
        err = float(np.max(np.abs(rotate(n, before) - after)))
        assert err < 1e-8
        stats.append(dict(x=p['x'], y=p['y'], mode=mode, normal_field=k in written, carrier_rotation_error=err))
    assert np.isfinite(support).all()
    assert np.max(np.abs(np.linalg.norm(support, axis=-1) - 1)) < 2e-9
    # Change just normal channels on eligible records. Preserve heights and all
    # unrelated records textually, except the required schema version bump.
    def update(m):
        r = tomllib.loads(m[0])['overworld_terrain'][0]
        k = key(r)
        if k not in eligible:
            return m[0]
        text = re.sub(r'^normal_offsets?\s*=.*\n?', '', m[0], flags=re.M)
        if k in written:
            text = text.rstrip() + '\nnormal_offsets = ' + json.dumps(written[k], separators=(',', ':')) + '\n\n'
        return text
    text = re.sub(r'^\[\[overworld_terrain\]\][\s\S]*?(?=^\[|\Z)', update, raw, flags=re.M)
    text = re.sub(r'^schema_version\s*=\s*\d+', 'schema_version = '+str(max(39, profile['schema_version'])), text, count=1)
    result = tomllib.loads(text)
    stripped = lambda r: {k: v for k, v in r.items() if k not in ('normal_offset', 'normal_offsets')}
    for name in profile.keys() | result.keys():
        if name == 'schema_version':
            continue
        if name != 'overworld_terrain':
            assert profile.get(name) == result.get(name), name
        else:
            for old, new in zip(profile[name], result[name]):
                assert stripped(old) == stripped(new)
                if key(old) not in eligible:
                    assert old == new
            assert len(profile[name]) == len(result[name])
    output.mkdir(parents=True, exist_ok=False)
    (output / 'source.toml').write_text(raw)
    dest = output / 'alttp-kakariko-village-river-normals-proposed.toml'
    dest.write_text(text)
    np.savez_compressed(output / 'normal-fields.npz', support=support, carrier=carrier, target=target,
                        floor_mask=floor_mask, labels=labels)
    (output / 'placements.json').write_text(json.dumps(stats, indent=2))
    art = np.array(Image.open(terrain / 'village-map.png').convert('RGB'))
    encoded = np.rint((support+1)*127.5).clip(0, 255).astype('uint8')
    height_rgb = np.repeat(np.rint((approved+32)/72*255).clip(0, 255).astype('uint8')[..., None], 3, axis=2)
    panels = [('Artwork', art), ('Approved terrain -32..40 (unchanged)', height_rgb), ('Normal OFFSETS: +X right, +Y down, +Z viewer', encoded)]
    review = Image.new('RGB', (3072, 1060), '#181d23')
    d = ImageDraw.Draw(review)
    for j, (label, pixels) in enumerate(panels):
        review.paste(Image.fromarray(pixels), (1024*j, 30))
        d.text((1024*j+8, 8), label, fill='white')
    review.save(output / 'review.png')
    # Neutral support responses are offset diagnostics, not production lighting.
    neutral = []
    textured = []
    for light in ((-.8, -.5, 1), (.8, -.5, 1), (0, .8, .25)):
        light = np.array(light, float)
        light /= np.linalg.norm(light)
        shade = .12 + .88*np.maximum(0, (support*light).sum(axis=-1))
        neutral.append(np.rint(shade[..., None]*np.array([190, 195, 200])).astype('uint8'))
        textured.append(np.rint(shade[..., None]*art).astype('uint8'))
    Image.fromarray(np.concatenate(neutral, axis=1)).save(output / 'neutral-offset-lighting.png')
    Image.fromarray(np.concatenate(textured, axis=1)).save(output / 'artwork-offset-lighting.png')
    # Native-resolution transition crops, enlarged only with nearest-neighbor.
    for name, box in [('northeast', (784, 0, 1024, 344)), ('manor-ledge-stairs', (320, 192, 824, 320)), ('riverbank', (912, 384, 1024, 784)), ('west-ramp', (88, 128, 336, 264))]:
        crops = [Image.fromarray(pixels).crop(box) for _, pixels in panels]
        w, h = crops[0].size
        crop = Image.new('RGB', (w*3, h))
        for j, image in enumerate(crops):
            crop.paste(image, (w*j, 0))
        crop.resize((w*6, h*2), Image.Resampling.NEAREST).save(output / (name+'.png'))
    report = dict(source=str(source), source_sha256=sha(source), proposal_sha256=sha(dest),
                  approved_terrain_sha256=sha(terrain / 'terrain-height.npy'),
                  eligible_height_coordinates=len(eligible), written_normal_fields=len(written),
                  identity_normal_fields_omitted=len(eligible)-len(written),
                  mode_counts={mode: sum(p['normal_field'] and p['mode'] == mode for p in stats) for mode in sorted({p['mode'] for p in stats})},
                  max_unit_error=float(np.max(np.abs(np.linalg.norm(support, axis=-1)-1))),
                  max_carrier_mapping_error=max(p['carrier_rotation_error'] for p in stats),
                  heights_and_local_data_unchanged=True, canonical_unchanged=True,
                  method='Floor metadata only; assembled same-surface floor-neighbor gradients; normal = normalize(-dH/dx,-dH/dy,1). No wall or obstacle deformation.',
                  references=['User-approved village/river terrain and pinned height-export placement policies',
                              'Column/tree lighting references inspected for preservation of local facets/curvature; no object arrays imported'],
                  limitations=['Only floor-marked existing per-pixel height coordinates are eligible; all other roles use scalar bases only',
                               'Bank shading follows the approved projected height gradient, not a newly authored physical vertical-bank model',
                               'Existing mixed-tile/contact and approximate structural-attachment limitations remain',
                               'Neutral/artwork images visualize offset rotation, not composed local normals or runtime shadows',
                               'Live GPU lighting, meshes and actor grounding not validated'])
    (output / 'REPORT.json').write_text(json.dumps(report, indent=2))
    (output / 'review.html').write_text('''<!doctype html><meta charset="utf-8"><title>Kakariko per-pixel normal offsets</title>
<style>body{background:#181d23;color:#eef;font:17px sans-serif;margin:24px}img{max-width:100%;image-rendering:pixelated}a{color:#9df}</style>
<h1>Kakariko village / river per-pixel normal offsets</h1>
<p>Separate proposal; canonical unchanged. All heights and authored local normals are preserved.
Normals are added only to existing per-pixel-height coordinates; identity rotations are omitted.</p>
<img src="review.png"><h2>Neutral offset response: left / right / low-front</h2><img src="neutral-offset-lighting.png">
<h2>Artwork offset response</h2><img src="artwork-offset-lighting.png">
<p>These show rotation offsets, not complete composed local geometry or runtime lighting. Structural rotations compensate the fitted local carrier; their colors need not resemble the final face normal.</p>
<h2>Northeast hillside</h2><img src="northeast.png"><h2>Ledge / stairs</h2><img src="manor-ledge-stairs.png">
<h2>Riverbank</h2><img src="riverbank.png"><h2>West walk-around ramp</h2><img src="west-ramp.png">
<p><a href="REPORT.json">Method, counts, hashes and limitations</a></p>''')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--heights', type=Path, required=True)
    parser.add_argument('--terrain', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.heights, args.terrain, args.output)
