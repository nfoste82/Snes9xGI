#!/usr/bin/env python3
"""Pin evidence and summarize actual before/after review artifacts."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from PIL import Image

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('capture', type=Path)
    p.add_argument('evidence_capture', type=Path)
    a = p.parse_args()
    pins = {}
    for path in [a.capture, *[Path(str(a.evidence_capture)+s) for s in
                 ('', '.profile.toml', '.dungeon.json', '.dungeon-input.bin', '.dungeon-templates.bin')]]:
        data = path.read_bytes()
        pins[str(path)] = hashlib.sha256(data).hexdigest()
        pinned = a.output / ('immutable-'+path.name)
        if pinned.exists() and pinned.read_bytes() != data:
            raise ValueError('source changed since immutable pin: '+str(path))
        if not pinned.exists():
            pinned.write_bytes(data)
    report = dict(pins=pins, exact_capture_sidecars=False, matching_candidate=str(a.evidence_capture), approval=False)
    for name in ('captured042', 'after042-pixels'):
        d = json.loads((a.output/(name+'.json')).read_text())
        report[name] = dict(schema=d['schema'], scene_pixels=d['scene_pixels'],
            instances=len(d['instances']), changed_canonical_samples=sum(p[0]!=p[1] for i in d['instances'] for p in i['pixels']),
            normal_corrections=sum(bool(i['normal_correction']) for i in d['instances']),
            usages=dict(Counter(str((i['hash'],i['base'],i['override'],i['bg'])) for i in d['instances'])))
    for name in ('matched042-captured-composed-height', 'matched042-recomputed-composed-height'):
        Image.open(a.output/(name+'.ppm')).save(a.output/(name+'.png'))
    for room in ('055', '060', '061'):
        d = json.loads((a.output/f'room-{room}-validation-v2.json').read_text())
        report[room] = {k:d[k] for k in ('valid_walkable_placements', 'required_walkable_placements',
            'per_plane_walkable_denominator', 'solved_wall_cells', 'unknown_wall_cells')}
        report[room].update(missing=len(d['missing_placements']), conflicts=len(d['conflicting_components']))
    d = json.loads((a.output/'live-v2/live-validation-v2.json').read_text())
    report['matched_live_v2'] = {k:d[k] for k in ('valid_walkable_placements', 'required_walkable_placements',
            'per_plane_walkable_denominator', 'solved_wall_cells', 'unknown_wall_cells')}
    report['matched_live_v2'].update(missing=len(d['missing_placements']), conflicts=len(d['conflicting_components']))
    c = json.loads((a.output/'corpus/corpus.json').read_text())
    report['corpus'] = {k:v for k,v in c.items() if k!='records'}
    report['corpus']['status_counts'] = dict(Counter(r['status'] for r in c['records']))
    bases = Counter()
    overlaps = 0
    for r in c['records']:
        if r['status'] != 'replayed':
            continue
        d = json.loads((a.output/'corpus'/Path(r['capture']).stem).with_suffix('.json').read_text())
        for l in d['layers']:
            for cell in l['cells']:
                if cell['role'] != 3:
                    continue
                bases[cell['base']] += 1
                overlaps += all(b != 255 for b in cell['support'])
    report['corpus']['visible_signed_base_distribution'] = dict(sorted(bases.items()))
    report['corpus']['both_plane_support_obstacles'] = overlaps
    report['corpus']['refusals'] = [dict(capture=r['capture'], error=r['log']) for r in c['records'] if r['status']=='refused']
    (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()
