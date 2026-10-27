#!/usr/bin/env python3
"""Group independent BG donors versus actual buffers, never infer art absence from metadata.

Input is alttp_layered_trace.h JSONL (non-JSON probe logging is ignored).
This is an exposure/source-conservation diagnostic, not an absolute datum oracle.
"""
import argparse
import collections
import json
from pathlib import Path

CHANNELS = ('height', 'normals', 'occlusion')

def classify(row):
    donor = row.get('sub', {})
    if donor.get('bg') != 1 or not donor.get('id') or not row.get('bg1_ink'):
        return None
    selected = row.get('scene', {})
    if selected.get('id') == donor['id'] and selected.get('k') == donor.get('k'):
        rule = 'bg1_donor_retained'
    elif selected.get('bg') == 2:
        rule = ('transparent_bg2_selected_over_opaque_bg1' if row.get('bg2_ink') == 0
                else 'opaque_bg2_owner_do_not_borrow_bg1')
    elif not selected.get('id'):
        rule = ('transparent_bg2_bg1_scene_omission' if row.get('bg2_ink') == 0
                else 'opaque_bg2_map_but_no_scene_receiver')
    else:
        rule = 'other_owner_not_automatic_bg1_fallback'
    available = [ch for ch in CHANNELS if donor.get('has_' + ch)]
    absent = [ch for ch in CHANNELS if not donor.get('has_' + ch)]
    # Missing normal authoring must not conceal independently available relief.
    return dict(rule=rule, donor_available=available, donor_absent=absent,
                lost=available if rule in ('transparent_bg2_selected_over_opaque_bg1',
                                          'transparent_bg2_bg1_scene_omission') else [])

def detect(rows):
    groups = {}; counts = collections.Counter(); stages = collections.Counter()
    for row in rows:
        stages[row['stage']] += 1
        if row['stage'] != 'final':
            continue
        counts['screen_pixels'] += 1
        c = classify(row)
        if c is None:
            counts['not_observed_opaque_bg1_sub_donor'] += 1
            continue
        counts[c['rule']] += 1
        d = row['sub']; s = row.get('scene', {})
        key = (c['rule'], row['x']//8, row['y']//8, d.get('hash'),
               tuple(c['donor_available']), tuple(c['donor_absent']), s.get('bg', 0))
        g = groups.setdefault(key, dict(**c, cell=[row['x']//8, row['y']//8],
            donor_bg=1, selected_bg=s.get('bg', 0), hash=d.get('hash'), pixels=0,
            samples=[], donor_local_values=set(), donor_actual_values=set(), donor_bases=set()))
        g['pixels'] += 1;g['donor_local_values'].add(d.get('local'));g['donor_actual_values'].add(d.get('actual'));g['donor_bases'].add(d.get('base'))
        if len(g['samples']) < 3:
            g['samples'].append(row)
    for g in groups.values():
        for k in ('donor_local_values', 'donor_actual_values', 'donor_bases'):
            g[k] = sorted(g[k], key=lambda x: (x is None, x))
    return dict(schema='layered-channel-diagnostic-v1', counts=dict(counts),
                stages=dict(stages),groups=list(groups.values()),
                limits=['Only independently observed opaque BG1 subscreen donors; absent donors are unsupported.',
                        'Opaque BG2 without channel data is not proof of transparent ownership.',
                        'No cross-support borrowing, authored zero is valid, missing channels remain absent.',
                        'Scene omission is detected channelwise; correct semantic admission requires independent occurrence proof.'])

def read_rows(path):
    with Path(path).open() as f:
        for line in f:
            if line.startswith('{'):
                try: row=json.loads(line)
                except json.JSONDecodeError: continue
                if 'stage' in row: yield row

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--trace',required=True);p.add_argument('--output',required=True);a=p.parse_args()
    out=detect(read_rows(a.trace));Path(a.output).write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out['counts'],sort_keys=True))
if __name__=='__main__':main()
