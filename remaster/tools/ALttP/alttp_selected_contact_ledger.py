#!/usr/bin/env python3
"""Selected-raster conditional contact-path diagnostic, not room approval.

Consumes production-selector observations and physical-solver floor
inventory. Does not consume the adapter's regions, owner claims or visual bases.
Every path is cardinal, downhill, fully observed and unit-affine at its seams.
Only an actual visible zero-local floor edge with a unique plane is an anchor.
Physical owner/base authority is NOT independent gameplay ownership evidence.
Legacy raw-only observations are explicitly unsupported, never exposure proof.
"""
import argparse
import collections
import json
from pathlib import Path


def build(pixels, physical, scroll):
    cells = {}; records = []
    for r in pixels:
        supported = r.get('selection_policy') == 'S9xRemasterSelectVisibleChannels/v1'
        p = r.get('visible', {}) if supported else r.get('scene' if r['use_scene'] else 'main', {})
        if p.get('source') != 2 or p.get('bg') not in (1, 2):
            continue
        donor = r.get('donors', {}).get('height', {})
        reason = None
        if not supported:
            reason = 'unsupported_selection_observation'
        elif p.get('owner', 0) >> 24 != 2 or ((p.get('owner', 0) >> 16) & 255) != p['bg']-1:
            reason = 'unsupported_visible_owner'
        elif not donor.get('id') or not p.get('has_height'):
            reason = 'missing_visible_height'
        elif any(p.get(k) != donor.get(k) for k in ('id', 'owner', 'k')):
            reason = 'height_donor_not_visible_occurrence'
        x, y = (r['sx'] + scroll[0]) & 511, (r['sy'] + scroll[1]) & 511
        key = (p['bg'], x // 8, y // 8)
        if reason is None:
            cells.setdefault(key, {})[(x % 8, y % 8)] = p
        records.append((r, p, key, x, y, reason))
    floors = {}
    for key, samples in cells.items():
        q = physical.get(key)
        if not q or not samples or any(p.get('role') == 2 for p in samples.values()):
            continue
        bases = [(i, b, q.get('occupied_floor_owners',[-1,-1])[i]) for i, b in enumerate(q['plane_bases'])
                 if b != 255 and q.get('occupied_floor_owners',[-1,-1])[i]>=0 and q.get('collision',[255,255])[i]==0]
        if len(bases) == 1 and all(p['base'] == bases[0][1] for p in samples.values()):
            floors[key] = bases[0]
    graph = collections.defaultdict(list); anchors = collections.defaultdict(list)
    # Determine displayed downhill direction independently from actual selected
    # local relief, not template identity or the adapter's candidate classifier.
    for key, samples in cells.items():
        if not samples or not all(p.get('role') == 2 for p in samples.values()):
            continue
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            edge = [(7 if dx > 0 else 0, k) if dx else (k, 7 if dy > 0 else 0) for k in range(8)]
            if not all(a in samples and (a[0]-dx, a[1]-dy) in samples and samples[a]['local']-samples[(a[0]-dx,a[1]-dy)]['local'] == -1 for a in edge):
                continue
            for bg in (1, 2):
                dest = (bg, key[1]+dx, key[2]+dy); other = cells.get(dest, {})
                pairs = [(a, (0 if dx > 0 else 7, k) if dx else (k, 0 if dy > 0 else 7)) for k, a in enumerate(edge)]
                if not all(b in other for _, b in pairs):
                    continue
                deltas = {samples[a]['local']-1-other[b]['local'] for a,b in pairs}
                if dest in floors:
                    # A face terminates at the floor itself (step0), not one
                    # pixel below it. All eight selected edge values must agree.
                    bases = {floors[dest][1]-samples[a]['local'] for a,_ in pairs}
                    if len(bases)==1 and all(other[b]['local']==0 for _,b in pairs):
                        anchors[key].append((next(iter(bases)), floors[dest][0], floors[dest][2], dest))
                elif len(deltas)==1 and all(other[b].get('role')==2 and (b[0]+dx,b[1]+dy) in other and other[(b[0]+dx,b[1]+dy)]['local']-other[b]['local']==-1 for _,b in pairs):
                    graph[key].append((dest, next(iter(deltas))))
    def paths(key, seen):
        if key in seen: return []
        out = [(b,p,owner,f,[key]) for b,p,owner,f in anchors[key]]
        for dest, delta in graph[key]:
            out += [(b-delta,p,owner,f,[key]+path) for b,p,owner,f,path in paths(dest,seen|{key})]
        return out
    ledger=[]; counts=collections.Counter()
    for r,p,key,x,y,rejection in records:
        witnesses=paths(key,set()) if not rejection and p.get('role')==2 else []
        datums={(b,plane,owner) for b,plane,owner,_,_ in witnesses}
        reason=rejection or ('unique_selected_floor_contact_path' if len(datums)==1 else 'conflicting_contacts' if datums else 'no_independent_contact_path')
        expected=next(iter(datums))[0] if len(datums)==1 else None
        counts[reason]+=1
        ledger.append({'screen':[r['sx'],r['sy']],'world':[x,y],'cell':key,'hash':p['hash'],'k':p['k'],'has_height':p['has_height'],'has_normals':p['has_normals'],'base':p['base'],'local':p['local'],'expected_base':expected,'reason':reason,'visible':r.get('visible'),'donors':r.get('donors'),'authority':'conditional_physical_solver_not_gameplay_owner_proof','witnesses':[{'base':b,'plane':plane,'owner':owner,'floor_cell':floor,'path':path} for b,plane,owner,floor,path in witnesses]})
    return ledger,dict(counts)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('pixels',type=Path);p.add_argument('physical',type=Path);p.add_argument('--scroll',nargs=2,type=int,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    physical={(r['bg'],r['x'],r['y']):r for r in map(json.loads,a.physical.open())}
    ledger,counts=build(map(json.loads,a.pixels.open()),physical,a.scroll)
    with a.output.open('w') as f:
        for r in ledger:f.write(json.dumps(r)+'\n')
    print(json.dumps(counts,indent=2))


if __name__=='__main__':main()
