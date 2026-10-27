#!/usr/bin/env python3
"""Regression comparator: retain pinned pre-repair expectations and sources.

Not a geometry discovery tool. Missing/unproved members, changed expectations,
and absent selected sources cannot become successful repairs.
"""
import argparse
import json
from pathlib import Path


def compare(before, after, terminal, words=None):
    key = lambda r: (r['rule'], r['layer'], r['x'], r['y'])
    current = {key(r): r for r in after['placements']}
    cells = {(r['bg']-1, r['x'], r['y']): r for r in terminal}
    records = []
    for b in before['placements']:
        if b['status'] not in ('pass', 'fail'):
            continue
        a = current.get(key(b))
        source = cells.get((b['layer'], b['x'], b['y']))
        problems = []
        if not a or a['status'] not in ('pass', 'fail'):
            problems.append('missing_or_unproved_after_member')
        if not source or not source.get('has_effective_height'):
            problems.append('selected_source_unavailable')
        if a and a.get('expected') != b['expected']:
            problems.append('independent_expectation_changed')
        actual = source.get('actual') if source else None
        if actual and words is not None:
            from alttp_doorway_detection import displayed
            actual = displayed(actual, words[b['layer'],b['x'],b['y']])
        expected = b['expected']
        wrong = [i for i, (v, e) in enumerate(zip(actual, expected)) if v != e] if actual and expected else []
        if not actual or len(actual) != 64 or not expected or len(expected) != 64:
            problems.append('invalid_pixel_denominator')
        elif wrong:
            problems.append('baseline_expectation_mismatch')
        if a and a.get('actual') != actual:
            problems.append('detector_terminal_disagreement')
        if a and a.get('status') != 'pass':
            problems.append('fresh_detector_not_passing')
        records.append(dict(key=list(key(b)), before_status=b['status'], before_actual=b['actual'], expected=expected, actual=actual, mismatch_pixels=wrong, problems=problems, selected_source=source))
    prior = {key(b) for b in before['placements'] if b['status'] in ('pass','fail')}
    extras = [r for r in after['placements'] if r['status'] == 'fail' and key(r) not in prior]
    return dict(status='fail' if extras or any(r['problems'] for r in records) else 'pass', checked=len(records), target_members=sum(r['before_status']=='fail' for r in records), previously_passing=sum(r['before_status']=='pass' for r in records), failed=sum(bool(r['problems']) for r in records), mismatch_pixels=sum(len(r['mismatch_pixels']) for r in records), new_failures=extras, records=records)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('before','after','terminal','snapshot','output'):
        p.add_argument('--'+name, type=Path, required=True)
    a=p.parse_args()
    from alttp_room_geometry import RoomSnapshot
    snapshot=RoomSnapshot.read(a.snapshot)
    words={(l,x,y):snapshot.cell(l,x,y).word for l in range(2) for y in range(64) for x in range(64)}
    result=compare(json.loads(a.before.read_text()),json.loads(a.after.read_text()),[json.loads(line) for line in a.terminal.read_text().splitlines()],words)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='records'}))
    return result['status']!='pass'


if __name__=='__main__':
    raise SystemExit(main())
