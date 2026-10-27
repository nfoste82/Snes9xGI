#!/usr/bin/env python3
"""Detect lost authored closed-Door relief without inventing an assembly datum.

Doorway masks intentionally have a different contract. This detector checks only
explicit Door roles with a canonical authored height and no context variants.
Offsets may vary between placements; within a closed panel the selected geometry
must preserve authored relief. Missing effective sources are independent failures.
This does not prove ownership, closed-state dispatch, or correct absolute bases.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import tomllib


def check(asset, terminal):
    if asset.get('tile_role') != 'door':
        return None
    result = dict(bg=terminal['bg'], x=terminal['x'], y=terminal['y'],
                  hash=terminal['hash'], rule='authored_closed_panel_relief',
                  selected_source={k: terminal.get(k) for k in
                    ('has_effective_height', 'asset_has_height', 'placement_height',
                     'context_height', 'base', 'geometry_status', 'plane_bases')})
    local = asset.get('height', [])
    if asset.get('geometry_by_flip') or asset.get('geometry_by_facing'):
        return dict(result, status='unsupported', reason='context_variant_needs_selection_proof')
    if len(local) != 64:
        return dict(result, status='unsupported', reason='missing_authored_panel_height')
    actual = terminal.get('actual', [])
    if len(actual) != 64:
        return dict(result, status='unsupported', reason='missing_terminal_samples')
    residual = [a-b for a,b in zip(actual, local)]
    # Both arrays are canonical: map H/V must NOT be applied to only one side.
    reason = ('missing_selected_source' if not terminal.get('has_effective_height')
              else 'nonconstant_authored_relief_offset' if len(set(residual)) != 1 else None)
    return dict(result, status='fail' if reason else 'pass_conditional', reason=reason,
                authored64=local, actual64=actual, residual64=residual,
                allowed_expected_family='authored64 + one independently proved placement datum',
                observed_offset=residual[0] if len(set(residual)) == 1 else None)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile', type=Path, required=True)
    p.add_argument('--terminal', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a=p.parse_args()
    assets={x['tile_hash'].split(':')[-1]:x for x in tomllib.loads(a.profile.read_text())['assets']}
    terminal=[json.loads(x) for x in a.terminal.read_text().splitlines()]
    if len(terminal)!=8192 or len({(x['bg'],x['x'],x['y']) for x in terminal})!=8192:
        p.error('requires complete 8192 unique terminal placements')
    records=[r for t in terminal if (r:=check(assets.get(t['hash'].zfill(16),{}),t)) is not None]
    result=dict(format='alttp-authored-door-detection-v1',complete_placements=len(terminal),
                status_counts=dict(Counter(r['status'] for r in records)),
                reason_counts=dict(Counter(r['reason'] for r in records if r['reason'])),
                placements=records, limitations=[
                    'Explicit Door metadata is conditional intent, not captured dynamic dispatch proof.',
                    'Constant residual does not prove an absolute datum, ownership or exposure.',
                    'Doorway masks/openings are not evaluated by the closed-panel contract.'])
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result['status_counts']))
    return int(any(r['status']=='fail' for r in records))


if __name__=='__main__':
    raise SystemExit(main())
