"""Summarize pinned obstacle060058 review artifacts without viewport approval."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--review', type=Path, required=True)
    p.add_argument('--capture', type=Path, required=True)
    a = p.parse_args()
    out = {}
    for name in ('before.json', 'after.json', 'before-effective.json', 'after-effective.json',
                 'reference-before.json', 'reference-after.json', 'authored-obstacle-replay-corpus.json'):
        r = json.loads((a.review/name).read_text())
        out[name] = {k: v for k, v in r.items() if k != 'records'}
    gate = json.loads((a.review/'live-validation-v2.json').read_text())
    out['validation_summary'] = {k: v for k, v in gate.items() if not isinstance(v, (list, dict))}
    out['validation_list_denominators'] = {k: len(v) for k, v in gate.items() if isinstance(v, list)}
    runtime = json.loads((a.review/'live-local-seed-runtime.json').read_text())
    out['runtime_summary'] = {k: v for k, v in runtime.items() if not isinstance(v, (list, dict))}
    out['runtime_plane_support_denominators'] = [sum(r[0] == p for r in runtime['cells_plane_x_y_base']) for p in (0, 1)]
    out['runtime_lists'] = {k: len(v) for k, v in runtime.items() if isinstance(v, list)}
    out['runtime_counts'] = {k: runtime[k] for k in ('required_planes', 'missing_planes', 'persistent_support_required', 'persistent_support_missing')}
    out['gate_plane_denominators'] = gate['per_plane_walkable_denominator']
    for k, v in gate.items():
        if isinstance(v, dict):
            out['validation_summary'][k] = {kk: vv for kk, vv in v.items() if not isinstance(vv, (list, dict))}
    out['sha256'] = {str(a.capture)+s: hashlib.sha256(Path(str(a.capture)+s).read_bytes()).hexdigest()
                     for s in ('', '.dungeon.json', '.profile.toml', '.dungeon-input.bin', '.dungeon-templates.bin', '.terrain-input.bin')}
    before = json.loads((a.review/'reference-before.json').read_text())
    after = json.loads((a.review/'reference-after.json').read_text())
    out['affected_contexts'] = [dict(snapshot=b['snapshot'], before_solved=b['original_authored_solved'],
                                    after_solved=c['original_authored_solved'])
                                for b, c in zip(before['records'], after['records'])
                                if b.get('original_authored_solved') != c.get('original_authored_solved')]
    (a.review/'summary.json').write_text(json.dumps(out, indent=2)+'\n')
    print(json.dumps(out, indent=2))


if __name__ == '__main__':
    main()
