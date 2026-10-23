"""Intrinsic-only completion rules for terminal trim and immutable LOW feet.

No placement offsets are inferred. A template-only foot remains unobserved at
runtime, but its canonical band is fixed by the complete immutable ROM columns.
"""
from alttp_diagonal_sets import ROLES, REFERENCES, transfer, eligibility
from alttp_diagonal_gaps import conversion_equivalent


def remaining_proposals(entries, discovery, assets, semantics):
    updates,proofs={},{}
    byid={e['tile_hash']:e for e in entries}
    owned={}
    actual_entrances={(r['room'],r['requested_entrance']):int(dict(v.split('=') for v in r['renderer'].split())['entrance'])
                      for r in discovery['rooms'] if r['status']=='scanned'}
    for assembly in discovery['assemblies']:
        for p in assembly['placements']:
            owned[(assembly['room'],assembly['entrance'],p['layer'],p['x'],p['y'])]=p
    for e in entries:
        key=e['tile_hash']
        if not e['gap'] or any(f in assets.get(key,{}) for f in ('height','normal_xyz')):continue
        bands={p['band'] for p in e['templates']}
        if bands=={'trim'} and e['placements']:
            if eligibility(key,{4},assets,semantics,REFERENCES,set()):continue
            # Exact final dispatch+word for EVERY observed usage, no room/hash
            # exception. Canonical band4 lies wholly beyond the zero foot seam.
            if all((p:=owned.get((c['room'],actual_entrances.get((c['room'],c['entrance']),c['entrance']),c['layer'],c['x'],c['y']))) and
                   p['tile_hash']==key and p['band']=='trim' and p['word']==c['word'] for c in e['placements']):
                updates[key]={'height':[0]*64,'normal_xyz':[128,128,255]*64}
                proofs[key]=dict(rule='proved_terminal_band4_zero_floor_trim',uses=len(e['placements']),
                    formula='max(0,min(24,31-8*4-x-y)) == 0 for all64 samples',
                    normal='flat +Z continuation after the reference zero-foot region',placement_base='not inferred')
        elif bands=={'foot'} and {p['family'] for p in e['templates']}=={'low'} and not e['placements']:
            # All templates decode the same canonical band under flips. Existing
            # source semantics agree; global complete maps contain no other use.
            if eligibility(key,{3},assets,semantics,REFERENCES,set()):continue
            if not discovery.get('low_cardinal_evidence'):continue
            companions=[]
            for t in discovery['loaded_templates']:
                if any(p['tile_hash']==key for p in t['bands']):
                    ordered={p['band']:p['tile_hash'] for p in t['bands']}
                    if t['handler']<=20 or any(assets.get(ordered[b],{}).get('height')!=
                            assets[REFERENCES[i]]['height'] for i,b in enumerate(ROLES[:3])):
                        break
                    companions.append((t['room'],t['handler']))
            else:
                if companions:
                    updates[key]=transfer(assets[REFERENCES[3]])
                    proofs[key]=dict(rule='immutable_low_template_foot_with_three_exact_authored_companions',
                        contexts=companions,observed_placements=0,placement_base='unobserved; not inferred')
    # Palette-half siblings receive exactly the same intrinsic regions.
    for e in entries:
        key=e['tile_hash']
        if not e['gap'] or any(f in assets.get(key,{}) for f in ('height','normal_xyz')):continue
        for source in list(updates):
            if not e['placements'] and not e['templates'] and conversion_equivalent(e,byid[source]):
                if not eligibility(key,{3},assets,semantics,REFERENCES,set()):
                    updates[key]=updates[source].copy()
                    proofs[key]=dict(rule='exact_ROM_source_palette_half_conversion',source=source)
    return updates,proofs
