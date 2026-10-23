#!/usr/bin/env python3
"""Build a file:// area-review catalog (stdlib only; no web server).

Example:
  python3 remaster/tools/ALttP/alttp_overworld_review.py --capture 1536,1536=/tmp/a.s9xrmf

--capture X,Y=PATH registers a packet, not proof of matching runtime coverage.
--launcher X,Y=PATH registers a separately generated .command launcher.
--manifest accepts {"captures": [{"x":1536,"y":1536,"capture":"a.s9xrmf",
"launcher":"a.command","provenance":"...","matching_checks_passed":true}]}.
Paths in a manifest are relative to that manifest; CLI paths are relative to cwd.
Readiness additionally requires the review's explicit ready_for_review=true,
complete denominators, no blockers, and matching checks for every viewport.
"""

import argparse
import base64
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[3]
DEFAULT = ROOT / 'build/remaster-geometry'


def script_json(value):
    """JSON inside a script element must not contain HTML parser delimiters."""
    return (json.dumps(value, separators=(',', ':'), ensure_ascii=True)
            .replace('<', '\\u003c').replace('>', '\\u003e').replace('&', '\\u0026'))


def local_link(value, base):
    if not value:
        return None
    parsed = urlparse(str(value))
    if parsed.scheme:
        if parsed.scheme != 'file':
            raise ValueError('capture/launcher links must be local files')
        from urllib.parse import unquote
        path = Path(unquote(parsed.path))
    else:
        path = base / value
    path = path.resolve()
    return {'href': path.as_uri(), 'exists': path.is_file(), 'path': str(path)}


def registration(value):
    try:
        xy, path = value.split('=', 1)
        x, y = map(int, xy.split(','))
        return {'x': x, 'y': y, 'path': path}
    except ValueError as exc:
        raise argparse.ArgumentTypeError('expected X,Y=PATH (global area pixels)') from exc


def build_data(candidate, review, artwork=None, registrations=(), base=Path.cwd()):
    if candidate['pin'] != review['pin']:
        raise ValueError('candidate and review pins differ')
    ps = candidate['placements']
    bounds = review['complete_area_pixel_bounds']
    x0, y0, x1, y1 = bounds
    # Every layer/surface is selectable independently; no last-writer flattening.
    planes = sorted({(p['layer'], p['surface']) for p in ps})
    fields = sorted(set().union(*(p.keys() for p in ps)))
    pools = {k: [] for k in ('local', 'normals', 'occlusion')}
    indices = {k: {} for k in pools}
    rows = []
    for p in ps:
        row = []
        for key in fields:
            value = p.get(key)
            if key in pools and value is not None:
                token = json.dumps(value, separators=(',', ':'))
                if token not in indices[key]:
                    indices[key][token] = len(pools[key])
                    pools[key].append(value)
                value = indices[key][token]
            row.append(value)
        rows.append(row)
    viewports = []
    known = {(v['x'], v['y']) for v in review['required_viewports']}
    if any((r['x'], r['y']) not in known for r in registrations):
        raise ValueError('capture/launcher origin is not a required viewport')
    for v in review['required_viewports']:
        v = dict(v)
        for key in ('capture', 'launcher'):
            v[key] = local_link(v.get(key), base)
        for r in registrations:
            if (r['x'], r['y']) == (v['x'], v['y']):
                v.update({k: val for k, val in r.items()
                          if k not in ('_base', 'capture', 'launcher')})
                for key in ('capture', 'launcher'):
                    if key in r:
                        v[key] = local_link(r[key], r.get('_base', base))
        v['linked'] = bool(v['capture'] and v['capture']['exists'])
        if not v['linked']:
            v['matching_checks_passed'] = False
        viewports.append(v)
    counts = Counter(p['status'] for p in ps)
    solved = counts['solved']
    excluded = counts['empty'] + counts['non_geometric']
    blockers = list(dict.fromkeys(candidate.get('blockers', []) + review.get('blockers', [])))
    revision = hashlib.sha256(json.dumps(candidate, sort_keys=True,
                                       separators=(',', ':')).encode()).hexdigest()
    if review.get('candidate_sha256') and review['candidate_sha256'] != revision:
        blockers.append('review candidate digest differs from current candidate')
    # A static overview and mere packet registration never promote readiness.
    ready = (review.get('ready_for_review') is True and not blockers
             and solved + excluded == len(ps)
             and review['coverage']['placements'] == len(ps)
             and candidate.get('scope', {}).get('complete_gameplay_state') is True
             and bool(viewports) and all(v['linked'] and v.get('matching_checks_passed') is True
                                        for v in viewports))
    raster = None
    if artwork:
        raw = artwork.read_bytes()
        if raw[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', raw[16:24]) != (x1-x0, y1-y0):
            raise ValueError('overview must be a PNG matching complete-area bounds')
        raster = 'data:image/png;base64,' + base64.b64encode(raw).decode('ascii')
    return dict(format='ALTPOWHTML1', pin=candidate['pin'],
                revision=revision, review_candidate_revision=review.get('candidate_sha256'),
                generation_revision=review.get('generation_revision', candidate.get('revision')),
                status='ready for review' if ready else 'blocked', verdict=candidate.get('verdict'),
                counts=dict(counts), total=len(ps), solved=solved,
                missing=len(ps)-solved-excluded, conflicts=counts['conflict'],
                reported_coverage=review['coverage'], bounds=bounds, planes=planes,
                fields=fields, rows=rows, pools=pools, artwork=raster,
                viewports=viewports, blockers=blockers, scope=candidate.get('scope'),
                calibration=candidate.get('castle_calibration'), capture_checks=candidate.get('capture_checks'))


def generate(candidate_path, review_path, output, artwork=None, registrations=()):
    candidate = json.loads(candidate_path.read_text())
    review = json.loads(review_path.read_text())
    data = build_data(candidate, review, artwork, registrations, candidate_path.resolve().parent)
    template = Path(__file__).with_suffix('.html').read_text()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(template.replace('/*__REVIEW_DATA__*/', script_json(data)), encoding='utf-8')
    return data


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--candidate', type=Path, default=DEFAULT/'overworld-castle-authored-v1.json')
    p.add_argument('--review', type=Path, default=DEFAULT/'overworld-castle-authored-v1-review.json')
    p.add_argument('--artwork', type=Path, default=DEFAULT/'castle-authored-v1-overview.png')
    p.add_argument('--output', type=Path, default=DEFAULT/'overworld-castle-review/index.html')
    p.add_argument('--capture', type=registration, action='append', default=[])
    p.add_argument('--launcher', type=registration, action='append', default=[])
    p.add_argument('--manifest', type=Path)
    a = p.parse_args()
    registrations = []
    if a.manifest:
        for r in json.loads(a.manifest.read_text())['captures']:
            registrations.append(dict(r, _base=a.manifest.resolve().parent))
    for key in ('capture', 'launcher'):
        for r in getattr(a, key):
            registrations.append(dict(x=r['x'], y=r['y'], **{key: str(Path(r['path']).resolve())}))
    data = generate(a.candidate, a.review, a.output, a.artwork, registrations)
    print(f"{a.output.resolve()}\n{data['status']}: {data['solved']}/{data['total']} solved; "
          f"{data['missing']} missing; {data['conflicts']} conflicts; "
          f"{sum(v['linked'] for v in data['viewports'])}/{len(data['viewports'])} linked viewports")


if __name__ == '__main__':
    main()
