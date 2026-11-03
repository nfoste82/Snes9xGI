"""Independent exported-profile checks. Run with the generator's NumPy Python.

Usage: python test_kakariko_terrain_export.py build/kakariko-terrain-review-v10
Checks actual TOML samples, not only the generator's unrounded scratch arrays.
"""
import json
from pathlib import Path
import sys
import tomllib
import numpy as np


def validate(directory):
    directory = Path(directory)
    profile = tomllib.loads((directory / 'alttp-kakariko-slope-proposed.toml').read_text())
    source = tomllib.loads((directory / 'source.toml').read_text())
    evidence = np.load(directory / 'proposal.npz')
    mask, plateau = evidence['ground_mask'], evidence['plateau_mask']
    scalars = {(r['x'], r['y']): r['offset'] for r in profile['overworld_offsets']
               if r['world'] == 'light' and r['background'] == 1}
    support = np.array([[scalars.get((x // 8, 192 + y // 8), 0)
                         for x in range(mask.shape[1])] for y in range(mask.shape[0])], float)
    normals = np.zeros((*mask.shape, 3))
    normals[:, :, 2] = 1
    varying = 0
    for r in profile['overworld_terrain']:
        if r['world'] != 'light' or r['background'] != 1:
            continue
        x, y = r['x'] * 8, (r['y'] - 192) * 8
        if not (0 <= x < mask.shape[1] and 0 <= y < mask.shape[0]):
            continue
        region = np.s_[y:y+8, x:x+8]
        support[region] += np.array(r.get('height_offset', [0] * 64)).reshape(8, 8)
        assert 'normal_offset' not in r, 'old per-tile normal remains in review crop'
        n = np.array(r['normal_offsets']).reshape(8, 8, 3)
        normals[region] = n
        varying += np.ptp(n.reshape(64, 3), axis=0).max() > 1e-6
    assert np.max(abs(support[plateau] - 40)) < 1e-6
    assert support[mask].min() >= -1e-6 and support[mask].max() <= 40.000001
    assert support[mask].min() < .01, 'lower landing must still reach zero'
    gradients = []
    for axis in (1, 0):
        g = np.zeros(mask.shape)
        for y, x in zip(*np.where(mask)):
            step = (0, 1) if axis == 1 else (1, 0)
            dy, dx = step
            prev = y >= dy and x >= dx and mask[y-dy, x-dx]
            nxt = y+dy < mask.shape[0] and x+dx < mask.shape[1] and mask[y+dy, x+dx]
            if prev and nxt:
                g[y, x] = (support[y+dy, x+dx] - support[y-dy, x-dx]) / 2
            elif nxt:
                g[y, x] = support[y+dy, x+dx] - support[y, x]
            elif prev:
                g[y, x] = support[y, x] - support[y-dy, x-dx]
        gradients.append(g)
    expected = np.stack((-gradients[0], -gradients[1], np.ones(mask.shape)), axis=2)
    expected /= np.linalg.norm(expected, axis=2)[:, :, None]
    error = float(np.max(abs(expected[mask] - normals[mask])))
    assert error < 2e-6, error  # exported fractional heights round to 1e-6
    assert varying > 100, 'normal arrays must genuinely vary within tiles'
    for key in source.keys() - {'schema_version', 'overworld_offsets', 'overworld_terrain'}:
        assert source[key] == profile[key], f'authored {key} changed'
    # Current user-edited input must retain all scalar coordinates/values.
    assert source['overworld_offsets'] == profile['overworld_offsets']
    result = dict(plateau_pixels=int(plateau.sum()), pixel_varying_tiles=int(varying),
                  exported_derivative_max_error=error, selected_ground_min=float(support[mask].min()),
                  selected_ground_max=float(support[mask].max()), authored_data_and_user_scalars_preserved=True)
    (directory / 'independent-validation.json').write_text(json.dumps(result, indent=2))
    print('PASS', json.dumps(result))


if __name__ == '__main__':
    validate(sys.argv[1])
