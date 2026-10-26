#!/usr/bin/env python3
"""Regression-only verification of fresh shared detector outputs for Room061.

Coordinates belong ONLY to this fixture oracle, never production detection.
Run the shared doorway/structure CLIs with a freshly built terminal consumer
first. A missing record, unsupported result, or classifier-preloaded false pass
is a regression, not an accepted geometry repair. No runtime/profile writes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def verify(doorways, structures):
    assert doorways['room_id'] == structures['room_id'] == 0x61
    for report in (doorways, structures):
        assert report['mode'] == 'detection_only' and not report['publishable']
        assert report.get('complete_bg_placements', report.get('denominators', {}).get('complete_bg_placements')) == 8192
    groups = {
        'north_interroom_stair_two_treads': (structures, {(1,x,5) for x in (31,32)}),
        'upper_left_interior_and_frame': (doorways, {(1,x,y) for x in (3,4) for y in (16,17)} | {(1,3,15),(1,3,18)}),
        'south_doorway_upper_band': (doorways, {(1,x,58) for x in range(30,34)}),
        'lower_left_BG2_not_BG1': (doorways, {(1,5,y) for y in range(31,35)} | {(1,6,31),(1,6,34)}),
        'east_prior_doorway': (doorways, {(1,56,32),(1,56,33)} | {(1,57,y) for y in range(31,35)}),
        'SW_high_missing_local_source': (structures, {(1,x,x+36) for x in range(2,12)} | {(1,x,x+35) for x in range(3,12)} | {(1,x,x+34) for x in range(4,12)}),
        'SE_high_missing_local_source': (structures, {(1,x,99-x) for x in range(52,62)} | {(1,x,98-x) for x in range(52,61)} | {(1,x,97-x) for x in range(52,60)}),
    }
    evidence = {}
    for name, (report, keys) in groups.items():
        records = {(r['layer'],r['x'],r['y']):r for r in report['placements']}
        found = []
        for key in sorted(keys):
            r = records[key]
            assert r['status'] == 'fail', (name,key,r['status'])
            wrong = [i for i,(a,b) in enumerate(zip(r['actual'],r['expected'])) if a != b]
            assert wrong and wrong == r['mismatch_pixels'], (name,key,'missing pixel proof')
            assert len(r['actual']) == len(r['expected']) == 64
            found.append(dict(placement=list(key),rule=r['rule'],actual=r['actual'],expected=r['expected'],mismatch_pixels=wrong))
        evidence[name] = found
    # Equal spans can conceal a one-pixel frame defect; do not test min/max alone.
    upper = evidence['upper_left_interior_and_frame']
    assert sorted(len(r['mismatch_pixels']) for r in upper) == [1,1,64,64,64,64]
    assert doorways['status_counts'] == {'fail':22,'pass':26}
    assert structures['by_rule']['diagonal_partial_pixel_geometry'] == {'fail':54,'pass':44,'unresolved':112,'excluded':24}
    assert structures['by_rule']['interroom_stair_floor_datum'] == {'fail':2,'pass':6,'excluded':8}
    return evidence


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--audit-dir',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    paths=[a.audit_dir/f'room-061-{kind}.json' for kind in ('doorways','structures')]
    reports=[json.loads(path.read_text()) for path in paths]
    evidence=verify(*reports)
    # Reinvoke the very same shared terminal consumer, not an isolated
    # classifier-only probe. BG1 is correct under the six broken BG2 members.
    terminal=[json.loads(line) for line in subprocess.check_output(reports[0]['consumer_command'],text=True).splitlines()]
    cells={(r['bg']-1,r['x'],r['y']):r for r in terminal}
    for r in evidence['lower_left_BG2_not_BG1']:
        _,x,y=r['placement']; counterpart=cells[0,x,y]
        assert counterpart['actual'] != cells[1,x,y]['actual']
        if x==5:
            assert sorted(counterpart['actual']) == sorted(r['expected'])
        r['bg1_correct_counterpart_actual']=counterpart['actual']
    a.output.write_text(json.dumps(dict(format='alttp-room061-detection-regression-v1',
        status='pass',meaning='all confirmed defects detected, NOT geometry approval',
        inputs={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
        expected_failure_classes=evidence),indent=2)+'\n')
    print('PASS: seven confirmed failure groups; 22 doorway, 2 tread, 54 diagonal placements; exact pixel evidence')


if __name__=='__main__':main()
