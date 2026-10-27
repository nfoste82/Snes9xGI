#!/usr/bin/env python3
"""Detection-only authored-vs-selected source census; no geometry synthesis.

Inspects all observed BG buffers, deduplicates physical canonical samples while
retaining buffer ownership. Missing authored data is not an activation failure.
Contexts are inventoried separately, never implicitly treated as selected assets.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import tomllib
from alttp_exposed_height_detection import scene_source_issue


def classify(asset, source):
    issues=[]
    for field, count, selected in (('height',64,'has_height'),('normal_xyz',192,'has_normals')):
        authored=len(asset.get(field,[]))==count
        if authored and not source.get(selected):issues.append('authored_'+field+'_not_selected')
        if not authored and not source.get(selected):issues.append('missing_asset_'+field+'_and_selected_source')
    return issues


def audit(profile, pixels, scroll):
    assets={int(a['tile_hash'].split(':')[-1],16):a for a in profile.get('assets',[])}
    groups={};seen=set();buffers=Counter();transparent=Counter();scene_issues=[]
    for p in pixels:
        issue=scene_source_issue(p)
        if issue:scene_issues.append(issue)
        for buffer in ('main','sub','scene'):
            s=p.get(buffer,{})
            if not s.get('id') or s.get('source')!=2 or s.get('bg') not in (1,2):continue
            if s.get('ink',0)<=0:transparent[buffer]+=1;continue
            h=int(s['hash'],16);a=assets.get(h,{})
            wx,wy=(scroll[0]+p['sx'])&511,(scroll[1]+p['sy'])&511
            key=(s['bg'],wx//8,wy//8,h)
            g=groups.setdefault(key,dict(bg=s['bg'],x=wx//8,y=wy//8,hash=f'{h:016x}',asset_role=a.get('tile_role','unknown'),
                asset_height=a.get('height'),asset_normals=a.get('normal_xyz'),issues=Counter(),buffers={},samples=[]))
            reasons=classify(a,s)
            buffers[buffer]+=1
            g['buffers'][buffer]=g['buffers'].get(buffer,0)+1
            for reason in reasons:g['issues'][reason]+=1
            sample=(key,s['k'],buffer)
            if sample not in seen:
                seen.add(sample);g['samples'].append(dict(buffer=buffer,k=s['k'],sx=p['sx'],sy=p['sy'],selected=s,
                    field_known=p['field_known'],participation=p['participation'],use_scene=p['use_scene']))
    rows=list(groups.values());counts=Counter()
    for g in rows:
        for reason in g['issues']:counts[reason]+=1
        g['distinct_canonical_pixels']=len({s['k'] for s in g['samples']})
    return dict(approval=False,observed_placements=len(rows),buffer_opaque_samples=dict(buffers),transparent_samples=dict(transparent),
        issue_placement_counts=dict(counts),groups=rows,scene_source_issues=scene_issues,scene_counts=dict(Counter(q['reason'] for q in scene_issues)),contexts_with_geometry=[dict(tile_hash=c['tile_hash'],name=c.get('name'),height_count=len(c.get('height',[])),normal_count=len(c.get('normal_xyz',[]))) for c in profile.get('tile_contexts',[])],
        limitations=['Diagnostics are not absolute-height contracts. Authored-vs-selected discrepancies may be intentional masks or context selection.',
        'Only captured opaque BG samples; OBJ and offscreen sources require separate complete inventories.',
        'Caller must independently establish packet/map/camera identity; no identity assertions waived.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('profile','pixels','state','output'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();state=json.loads(a.state.read_text())
    result=audit(tomllib.loads(a.profile.read_text()),(json.loads(l) for l in a.pixels.read_text().splitlines()),state['scroll'])
    result['room']=state['room'];result['inputs']={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in (a.profile,a.pixels,a.state)}
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('room','observed_placements','issue_placement_counts','buffer_opaque_samples')}))


if __name__=='__main__':main()
