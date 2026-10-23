#!/usr/bin/env python3
"""Assembled, anatomical normal proposals for the remaining bipedal NPCs.

Normals only; never writes the source profile. Detailed normals (including +X
fills), heights, variants and named contexts are authoritative. The rejected
humanoid generator is used only for profile/PNG plumbing and family inventory,
not for its geometry or fractional whole-silhouette segmentation.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tomllib

import numpy as np

from alttp_humanoid_geometry import FAMILIES, assemble, asset_blocks, variant_asset
from alttp_link_geometry import encode, png, unit
from alttp_npc_workbench import atomic_write
from alttp_reference_npc_geometry import cloth_normal, flat_bevel, smoothstep, top_facing


EXCLUDED = {'ganon', 'agahnim', 'blue', 'green', 'red', 'armos-knight', 'stump', 'uncle'}
KEYS = set(FAMILIES)-EXCLUDED
GUARDS = {'soldier', 'fast', 'spear', 'archer', 'bush-archer', 'javelin',
          'bush-javelin', 'bombguard', 'recruit'}
ROBES = {'elder', 'sweep', 'maze-lady', 'fortune-light', 'fortune-dark',
         'snitch-old', 'sage', 'zelda', 'wife', 'maiden', 'oldman',
         'blind-maiden', 'shop-light', 'wizzrobe', 'blind-boss', 'witch'}
SEATED = {'father', 'inn', 'archery', 'merchant', 'drink'}
FAIRIES = {'fairy', 'queen-light', 'queen-dark'}
COLORS = dict(head=(100,140,235), face=(235,175,135), torso=(120,190,140),
              robe=(60,150,90), arm=(230,145,65), hand=(240,200,130),
              leg=(170,120,210), boot=(145,95,60), prop=(220,215,120),
              wing=(195,160,245), shoulder=(220,100,120))


def unauthored(asset):
    """Only a whole-tile +Y fill, NOT the historical +X placeholder heuristic."""
    return asset.get('normal_xyz') == [128,255,128]*64


def rgb(color):
    return np.array([(color>>s)&31 for s in (0,5,10)], dtype=float)/31


def extents(points):
    return (min(p['x'] for p in points), min(p['y'] for p in points),
            max(p['x'] for p in points), max(p['y'] for p in points))


def skin(color, key):
    if key in GUARDS or key in {'stalfos','gibdo','freezor','pengator','wizzrobe',
                                'eyegore-green','eyegore-red','armos-statue'}:
        return False
    r,g,b=rgb(color)
    return r>0.40 and r>g*1.10 and g>b*1.22


def body_landmarks(actor, pose, pixels):
    """Use the draw assembly's overlapping head/body slots, not tile boundaries.

    Most people draw a 16px head over a 16px body starting 7–12px lower.
    Equipment-bearing and multi-person assemblies have explicit exceptions.
    """
    key=actor['draw_model']['key']
    ps=list(pixels.values())
    x0,y0,x1,y1=extents(ps)
    slots=defaultdict(list)
    for p in ps:slots[p['part']['slot']].append(p)
    head_slot=4 if key in {'archer','bush-archer'} else 2 if key=='bombguard' else 3 if key in GUARDS-{'soldier','recruit'} else 0
    # Select a central head slot for sword/tutorial/knife-guard layouts by
    # position when it differs from the spear guard contract.
    if key in {'soldier','recruit'}:
        head_slot=min(slots,key=lambda s:(min(p['y']for p in slots[s]),abs(np.mean([p['x']for p in slots[s]])-7.5)))
    hs=slots.get(head_slot,ps)
    hx0,hy0,hx1,hy1=extents(hs)
    cx=(hx0+hx1)/2
    if key in GUARDS:cx=7.5
    if key in FAIRIES|{'eyegore-green','eyegore-red','freezor','armos-statue'}:
        cx=(x0+x1)/2
    radius=max(3,(hx1-hx0+1)/2)
    if key in FAIRIES:radius=max(6,(x1-x0+1)*0.40)
    head_top=hy0
    # Per-assembly landmarks, measured on the inspected artwork. Heights here
    # are sprite-space part divisions, NOT changes to runtime height layers.
    if key=='oldman':head_end=9
    elif key=='zelda':head_end=7
    elif key in {'maiden','blind-maiden'}:head_end=5
    elif key in GUARDS:head_end=5
    elif key in {'eyegore-green','eyegore-red','freezor'}:head_end=y0+9
    elif key=='armos-statue':head_end=y0+12
    elif key=='blind-boss':head_end=13
    elif key=='wizzrobe':head_end=5
    elif key in {'stalfos','gibdo'}:head_end=0
    elif key=='pengator':head_end=2
    elif key=='witch':head_end=12
    elif key in FAIRIES:head_end=y0+8
    else:
        lower=[min(p['y']for p in qs) for s,qs in slots.items()
               if s!=head_slot and abs(np.mean([p['x']for p in qs])-cx)<8
               and min(p['y']for p in qs)>hy0+3]
        head_end=min(lower)+3 if lower else hy0+11
    # Legs start below the tunic, independent of head/equipment extents.
    leg_start=y1-5 if key not in {'stalfos','gibdo'} else y1-9
    if key in {'eyegore-green','eyegore-red'}:leg_start=y1-6
    if key in FAIRIES:leg_start=y1-4
    radius=min(11,max(4,radius))
    return dict(cx=cx,radius=radius,head_top=head_top,head_end=head_end,
                leg_start=leg_start,y0=y0,y1=y1,key=key,facing=pose['facing'])


def segment(actor, pose):
    pixels,occ=assemble(pose)
    lm=body_landmarks(actor,pose,pixels)
    key=lm['key'];cx=lm['cx'];r=lm['radius'];he=lm['head_end']
    result=[]
    for p in occ:
        x,y=p['x'],p['y'];slot=p['part']['slot']
        center=cx;head_top=lm['head_top'];head_end=he;radius=r
        prop=False
        if key=='lumber':
            prop=slot<=6
            center=-24.5 if slot in (7,8) else 38
            radius=7.5;head_top=-8;head_end=6
        if key=='smithy':prop=slot==1
        if key=='witch':prop=slot in (1,3,4)
        if key in {'fast','spear','javelin','bush-javelin'}:prop=slot in (0,1)
        if key=='bombguard':prop=slot==0
        if key in {'archer','bush-archer'}:prop=slot in (2,3)
        side=(x-center)/radius
        zone='head' if y<head_end else 'robe' if key in ROBES or key in FAIRIES else 'torso'
        if prop:zone='prop'
        elif key in FAIRIES and abs(side)>0.62 and y<head_end+7:zone='wing'
        elif y>=head_end and y>=lm['leg_start'] and key not in ROBES|FAIRIES:
            zone='boot' if y>=lm['y1']-2 else 'leg'
        elif y>=head_end-1 and abs(side)>0.62 and y<lm['leg_start']:
            zone='arm'
        # The face is a connected skin patch below crown/hair, never inferred
        # from the brightest palette entries (eyes and jewelry can be white).
        material='skin' if skin(p['color'],key) else 'cloth'
        if key in FAIRIES and not prop:
            # Central small head, bare upper torso and arms, then gathered
            # skirt. Long side hair/wings are not the sides of a giant head.
            if y<head_end and abs(side)>0.40:zone='wing'
            elif head_end<=y<head_end+8 and abs(side)<0.55:zone='torso'
            elif head_end<=y<head_end+12 and abs(side)>=0.55:zone='arm'
            elif y>=head_end+8:zone='robe'
        if zone=='head' and material=='skin' and pose['facing']!='north' and y>head_top+2:zone='face'
        if zone=='arm' and material=='skin' and y>head_end+2:zone='hand'
        result.append(dict(p,zone=zone,material=material,center=center,
                           radius=radius,head_top=head_top,head_end=head_end))
    # Attach dark line art to adjacent anatomy. Do not give eyes, seams or black
    # outlines their own disconnected spheres; exclude opaque overlap copies.
    interior=[p for p in result if max(rgb(p['color']))>0.28]
    grid=defaultdict(list)
    for p in interior:grid[p['x'],p['y']].append(p)
    for p in result:
        if max(rgb(p['color']))>0.28:continue
        near=[q for dx in range(-2,3)for dy in range(-2,3)
              for q in grid.get((p['x']+dx,p['y']+dy),[])
              if q['part']['slot']==p['part']['slot'] and
              (q['zone']==p['zone'] or {q['zone'],p['zone']}<= {'head','face','arm','hand'})]
        if near:
            q=min(near,key=lambda q:(q['x']-p['x'])**2+(q['y']-p['y'])**2)
            p['zone'],p['material']=q['zone'],q['material']
    # Anatomy grouping spans tile edges and mirrored OAM halves. Only real
    # limbs split left/right. Head and robe MUST keep one shared centerline.
    groups=defaultdict(list)
    for p in result:
        side=-1 if p['x']<p['center'] else 1
        group=(p['center'],p['zone'],side if p['zone'] in ('arm','hand','leg','boot') else 0)
        p['group']=group;groups[group].append(p)
    for group,qs in groups.items():
        b=extents(qs)
        # Row centers capture bent sleeves/legs and walking stance. A smoothed
        # centerline, not separate per-row domes, supplies the limb tangent.
        coords=set((p['x'],p['y']) for p in qs)
        rows=defaultdict(list)
        for x,y in coords:rows[y].append(x)
        path={y:float(np.median([x for yy,xx in rows.items()if abs(yy-y)<=2 for x in xx])) for y in rows}
        for p in qs:
            p['surface_bounds']=b;p['path']=path
    # Gentle, spatially smoothed light-from-above evidence within chromaticity
    # groups. A white collar and brown face never calibrate one another.
    for p in result:
        p['lighting_cue']=lighting_cue(p,grid)
    return lm,result


def lighting_cue(p,grid):
    c=rgb(p['color']);value=float(max(c))
    if value<0.28:return 0.0
    chroma=c/max(0.01,float(np.linalg.norm(c)))
    samples=[]
    for dx in range(-2,3):
        for dy in range(-2,3):
            for q in grid.get((p['x']+dx,p['y']+dy),[]):
                qc=rgb(q['color']);qv=float(max(qc))
                if qv<0.28 or q['zone']!=p['zone']:continue
                if np.linalg.norm(qc/max(0.01,float(np.linalg.norm(qc)))-chroma)>0.16:continue
                samples.append((qv,math.exp(-(dx*dx+dy*dy)/5)))
    if len(samples)<3:return 0.0
    mean=sum(v*w for v,w in samples)/sum(w for _,w in samples)
    # Bounded evidence only. No discrete change of surface type, no height
    # embossing, no deep sockets from dark paint. Average local evidence too.
    return float(np.clip((value-mean)*0.38,-0.07,0.12))


def rounded(x,y,cx,cy,rx,ry,top=0.18):
    u=float(np.clip((x-cx)/max(1,rx),-0.94,0.94))
    v=float(np.clip((y-cy)/max(1,ry),-1,1))
    return unit((u*0.70,math.sqrt(max(0.12,1-u*u)),top-v*0.22))


def limb_normal(p, boot=False):
    """Tube around the assembled limb's bent axis in the world X/Z plane."""
    x,y=p['x'],p['y'];x0,y0,x1,y1=p['surface_bounds'];path=p['path']
    nearest=lambda yy:min(path,key=lambda yy0:abs(yy0-yy))
    cy=path[nearest(y)];ya,yb=nearest(y-2),nearest(y+2)
    slope=(path[yb]-path[ya])/max(1,yb-ya)
    radius=max(1.8,min(4,(x1-x0+1)/2+0.4))
    u=float(np.clip((x-cy)/radius,-0.9,0.9))
    n=unit((u*0.72,math.sqrt(max(0.15,1-u*u)),0.15+u*slope*0.55))
    depth=min(4,max(2,(y1-y0+1)*0.7))
    # The top of a trouser segment is hidden under the tunic, not an exposed
    # horizontal shelf. Likewise a palm is not automatically a new cap.
    strength=1 if boot else 0.90 if p['zone']=='arm' else 0.25 if p['zone']=='hand' else 0
    return top_facing(n,x,y,cy,y0,radius,depth,strength)


def model_pixel(lm,p):
    x,y=p['x'],p['y'];z=p['zone'];key=lm['key'];cx=p['center'];r=p['radius']
    x0,y0,x1,y1=p['surface_bounds']
    he=p['head_end'];ht=p['head_top']
    if z=='head':
        # Crown is actually world-up. Cap blends into forehead, not +Y at the
        # uppermost row. Hair/hat side planes retain lateral curvature.
        n=rounded(x,y,cx,(ht+he)/2,r+0.5,max(3,(he-ht)/2))
        n=top_facing(n,x,y,cx,ht,r,max(3,(he-ht)*0.65))
        if key in FAIRIES:
            n=rounded(x,y,cx,(ht+he)/2,3.8,max(3,(he-ht)/2))
            n=top_facing(n,x,y,cx,ht,3.8,4)
        if key in {'wizzrobe','witch'}:
            # Pointed hat is a sloping cone plus a shallow brim. Broad orange
            # highlights belong to its upward slope, not a huge round face.
            cone=unit(((x-cx)/max(3,r)*0.45,0.50,0.78))
            n=unit(n*0.25+cone*0.75)
        if key in GUARDS or key in {'armos-statue','freezor'}:
            # Broad armored/stone face with only its real silhouette bevelled.
            plane=unit(((x-cx)*0.025,0.99,0.12))
            blend=smoothstep(ht+2,he-1,y)*(1-smoothstep(0.6,1,abs(x-cx)/r))
            n=unit(n*(1-blend)+plane*blend)
    elif z=='face':
        fc=(x0+x1)/2
        n=rounded(x,y,fc,(y0+y1)/2,max(2,(x1-x0+1)/2+1),max(2,(y1-y0+1)/2),0.14)
        # Shallow continuous nose/cheek relief, oriented with facing. It is
        # not one tiny bump per painted highlight or one face per OAM quadrant.
        side=-1 if lm['facing']=='west' else 1 if lm['facing']=='east' else 0
        nose_x=fc+side*1.4
        nose=math.exp(-((x-nose_x)/1.7)**2-((y-(y0+y1)/2)/2)**2)
        n=unit(n+np.array([(x-nose_x)*nose*0.13,0,nose*0.12]))
    elif z in ('arm','hand','leg','boot'):
        n=limb_normal(p,z=='boot')
        if z=='arm' and p['material']!='skin' and key not in GUARDS:
            # Sleeves have folds around elbow direction, separate from robe.
            fold=cloth_normal(x,y,(x0+x1)/2,y0,max(3,x1-x0+1),max(6,y1-y0+1),0.24)
            n=unit(n*0.84+fold*0.16)
    elif z=='prop':
        if key=='bombguard':
            n=rounded(x,y,(x0+x1)/2,(y0+y1)/2,max(2,(x1-x0+1)/2),max(2,(y1-y0+1)/2))
            n=top_facing(n,x,y,(x0+x1)/2,y0,max(2,(x1-x0+1)/2),4)
        else:n=flat_bevel(x,y,(x0,y0,x1,y1))
    elif z=='wing':
        n=unit(((x-cx)*0.018,0.90,0.40))
    elif z=='robe':
        n=cloth_normal(x,y,cx,he,max(5,r),max(8,lm['y1']-he+1),0.48 if key in FAIRIES else 0.68)
        # Draped shoulder/hunched upper back faces up at the garment attachment.
        shoulder=smoothstep(0.28,0.72,abs(x-cx)/r)
        n=top_facing(n,x,y,cx,he,r,4.5,0.72*shoulder)
    else:
        n=rounded(x,y,cx,(he+lm['leg_start'])/2,r,max(3,(lm['leg_start']-he)/2),0.20)
        shoulder=smoothstep(0.25,0.70,abs(x-cx)/r)
        n=top_facing(n,x,y,cx,he,r,4,0.90*shoulder)
        if key in GUARDS or key in {'eyegore-green','eyegore-red','armos-statue','freezor'}:
            # Cuirass/stone face has a broad real plane, not an inflated tube.
            plane=unit(((x-cx)*0.022,0.99,0.12))
            w=(1-smoothstep(0.45,0.85,abs(x-cx)/r))*smoothstep(he+1,he+4,y)
            n=unit(n*(1-w)+plane*w)
        elif key not in {'stalfos','gibdo'}:
            f=cloth_normal(x,y,cx,he,r,max(6,lm['leg_start']-he),0.30)
            n=unit(n*0.80+f*0.20)
    # Mummy wrappings and skeletons retain anatomy instead of inflating the
    # black background inside their ribcages into one torso cylinder.
    if key=='gibdo' and z in ('torso','arm','leg'):
        n=unit(n+np.array([0.035*math.sin(x*0.7+y*0.9),0,0.035*math.cos(x*0.7+y*0.9)]))
    if key=='stalfos' and z=='torso':
        # Vertebral column and paired rib arcs are physical relief, not every
        # bright pixel. No normals are emitted into transparent rib gaps.
        n=unit(n+np.array([0.10*math.sin((x-cx)*0.9),0,0.13*math.sin((y-he)*1.25)]))
    # Albedo is supporting evidence under an above-lit prior, not a normal or
    # depth map. Bound its influence and retain the anatomical cap's +Z.
    cue=p.get('lighting_cue',0)
    return unit(n+np.array([0,0,cue]))


def generate(manifest,source):
    parsed=tomllib.loads(source);assets={a['tile_hash']:a for a in parsed.get('assets',[])}
    actors=[a for a in manifest['actors']if a['draw_model']['key']in KEYS]
    names={a['name']for a in actors}
    owners=defaultdict(set)
    for a in manifest['actors']:
        for f in a['poses']:
            for p in f['parts']:owners[p['tile_hash']].add(a['name'])
    hashes={p['tile_hash']for a in actors for f in a['poses']for p in f['parts']}
    eligible={h for h in hashes if 'normal_xyz'not in assets.get(h,{}) or unauthored(assets[h])}
    bg=set(manifest['background_shared_hashes']);contexts={r['tile_hash']for r in parsed.get('tile_contexts',[])}
    blocked={}
    for h in sorted(eligible):
        if h in bg:blocked[h]='Shared background identity'
        elif owners[h]-names:blocked[h]='Shared with excluded actor: '+', '.join(sorted(owners[h]-names))
        elif variant_asset(assets.get(h,{})) or h in contexts:blocked[h]='Authored variants or named context'
    proposals=defaultdict(lambda:defaultdict(list));seen=set();poses=[];skipped=set();deferred=[]
    for a in actors:
        if a.get('graphics_inherited'):
            skipped.add(a['id']);continue
        for f in a['poses']:
            if a['draw_model']['key']=='blind-boss' and f['step']>0:
                deferred.append(dict(actor=a['name'],step=f['step'],facing=f['facing']));continue
            sig=(a['draw_model']['key'],f['facing'],tuple((p['tile_hash'],p['x'],p['y'],p['hflip'],p['vflip'])for p in f['parts']))
            if sig in seen:continue
            seen.add(sig)
            if not any(p['tile_hash']in eligible-blocked.keys()for p in f['parts']):continue
            lm,points=segment(a,f);poses.append((a,f,points))
            for p in points:
                part=p['part'];h=part['tile_hash']
                if h not in eligible or h in blocked:continue
                n=model_pixel(lm,p)
                if not assets.get(h,{}).get('normal_ignore_tile_transform'):
                    n[0]*=-1 if part['hflip']else 1
                    n[1]*=-1 if part['vflip']else 1
                proposals[h][p['pixel']].append((n,p['zone'],a['name']))
    edits={};conflicts=[];details={}
    for h,data in sorted(proposals.items()):
        ns=[128,128,255]*64;labels=Counter()
        for i,samples in data.items():
            dominant=Counter(s[1]for s in samples).most_common(1)[0][0]
            vectors=[s[0]for s in samples if s[1]==dominant]
            n=unit(np.median(vectors,axis=0));ns[i*3:i*3+3]=encode(n)
            labels.update(s[1]for s in samples)
            angle=max(math.degrees(math.acos(float(np.clip(s[0]@n,-1,1))))for s in samples)
            if angle>30 or len({s[1]for s in samples})>1:
                conflicts.append(dict(tile_hash=h,pixel=i,max_degrees=round(angle,2),
                                      surfaces=dict(Counter(s[1]for s in samples))))
        edits[h]=ns
        details[h]=dict(owners=sorted(owners[h]),surfaces=dict(labels),replaced_y=unauthored(assets.get(h,{})))
    def update(block,a):
        h=a['tile_hash']
        if h not in edits:return block
        line='normal_xyz = '+json.dumps(edits[h])+'\n'
        return re.sub(r'(?m)^normal_xyz\s*=.*\n?',lambda _:line,block) if 'normal_xyz'in a else block.rstrip()+'\n'+line+'\n'
    candidate=asset_blocks(source,update)
    for h in sorted(set(edits)-set(assets)):
        candidate+='\n[[assets]]\ntile_hash = '+json.dumps(h)+'\nnormal_xyz = '+json.dumps(edits[h])+'\n'
    actual={a['tile_hash']:a for a in tomllib.loads(candidate)['assets']}
    for h,a in assets.items():
        for k,v in a.items():assert actual[h][k]==(edits[h]if h in edits and k=='normal_xyz'else v),(h,k)
    for k,v in parsed.items():
        if k!='assets':assert tomllib.loads(candidate)[k]==v,k
    families=[]
    for name in sorted(names):
        aa=[a for a in actors if a['name']==name];hs={p['tile_hash']for a in aa for f in a['poses']for p in f['parts']}
        remaining=[h for h in hs if 'normal_xyz'not in actual.get(h,{})or unauthored(actual[h])]
        families.append(dict(name=name,changed_tiles=len(hs&edits.keys()),remaining=len(remaining),
                             remaining_hashes=sorted(remaining),inherited=all(a.get('graphics_inherited')for a in aa)))
    report=dict(source_sha256=hashlib.sha256(source.encode()).hexdigest(),changed_tiles=len(edits),
        replaced_y=sum(d['replaced_y']for d in details.values()),added_normals=sum(h not in assets or 'normal_xyz'not in assets[h]for h in edits),
        changed_families=sum(f['changed_tiles']>0 for f in families),families=families,tiles=details,
        blocked=blocked,skipped_inherited_contexts=sorted(skipped),deferred_poses=deferred,
        reuse_disagreement_tiles=len({d['tile_hash']for d in conflicts}),reuse_disagreements=conflicts,
        eligible_tiles=len(eligible),remaining_eligible_tiles=sorted(eligible-edits.keys()),
        remaining_reasons={h:blocked.get(h,'Only inherited/unverified graphics or deferred phase has this identity')
                           for h in sorted(eligible-edits.keys())},
        limitations=['Approximate authored anatomical models, not recovered high-resolution ground truth.',
            'Only missing maps and exact whole-tile +Y fills replaced. +X fills are preserved.',
            'All heights and detailed normal maps preserved; normal-only folds have no new occlusion relief.',
            'Inherited/unverified graphics and detached Blind phases not modeled.',
            'Shared hashes use dominant semantic surface and median vectors; conflicts remain explicit.',
            'Brightness is bounded local same-chromaticity evidence, not brightness-to-height conversion.',
            'Offline lighting and seam audits do not validate live animation or GI.'])
    return candidate,report,actual,poses


def world_normal(asset,part,i):
    ns=asset.get('normal_xyz')
    if ns is None:return None
    n=unit(np.array(ns[i*3:i*3+3])/127.5-1)
    if not asset.get('normal_ignore_tile_transform'):
        n[0]*=-1 if part['hflip']else 1;n[1]*=-1 if part['vflip']else 1
    return n


def preview(output,poses,assets):
    selected=[];seen=set()
    for a,f,points in poses:
        sig=(a['name'],f['facing'])
        if sig in seen:continue
        seen.add(sig);selected.append((a,f,points))
    index=[];width,rh=1280,168
    for page,start in enumerate(range(0,len(selected),8),1):
        rows=selected[start:start+8];data=bytearray([28,30,35]*width*rh*len(rows))
        for row,(a,f,points)in enumerate(rows):
            x0,y0,x1,y1=f['bounds'];scale=max(1,min(5,144//max(1,y1-y0),196//max(1,x1-x0)))
            visible={}
            for p in points:visible.setdefault((p['x'],p['y']),p)
            for (x,y),p in visible.items():
                part=p['part'];i=p['pixel'];n=world_normal(assets.get(part['tile_hash'],{}),part,i)
                art=[int(c*255)for c in rgb(p['color'])]
                lights=[[round(255*(0.12+0.88*max(0,float(n@unit(l)))))]*3 if n is not None else [255,0,255]
                        for l in ((-0.65,0.5,0.75),(0.65,0.5,0.75),(0,0.15,1))]
                for col,color in enumerate([art,COLORS[p['zone']],encode(n)if n is not None else [255,0,255],*lights]):
                    xx=col*212+8+(x-x0)*scale;yy=row*rh+8+(y-y0)*scale
                    for dy in range(scale):
                        for dx in range(scale):
                            at=((yy+dy)*width+xx+dx)*3;data[at:at+3]=bytes(color)
            index.append(dict(page=page,row=row,name=a['name'],context=a['id'],facing=f['facing'],step=f['step']))
        png(output/f'review-{page:02}.png',width,rh*len(rows),data)
    atomic_write(output/'review-index.json',json.dumps(index,indent=2)+'\n')
    atomic_write(output/'surface-colors.json',json.dumps(COLORS,indent=2)+'\n')


def audit(poses,assets,changed):
    """Audit actual exported profile samples, not just analytical model functions."""
    families=defaultdict(lambda:dict(pairs=0,tile_edge_pairs=0,over_45=0,tile_edge_over_45=0,
                                    max_degrees=0,cap_pixels=0,cap_z_dominant=0))
    outliers=[]
    for a,f,points in poses:
        pix={}
        for p in points:pix.setdefault((p['x'],p['y']),p)
        stats=families[a['name']]
        for (x,y),p in pix.items():
            part=p['part'];h=part['tile_hash']
            if h not in changed:continue
            n=world_normal(assets[h],part,p['pixel'])
            if p['zone']=='head' and y<=p['head_top']+1 and abs(x-p['center'])<p['radius']*0.5:
                stats['cap_pixels']+=1;stats['cap_z_dominant']+=int(n[2]>abs(n[0])and n[2]>abs(n[1]))
            for xy in ((x+1,y),(x,y+1)):
                q=pix.get(xy)
                if not q or p['group']!=q['group']:continue
                qa=assets.get(q['part']['tile_hash'],{});qn=world_normal(qa,q['part'],q['pixel'])
                if qn is None:continue
                angle=math.degrees(math.acos(float(np.clip(n@qn,-1,1))))
                edge=part['tile_hash']!=q['part']['tile_hash'] or part['x']!=q['part']['x'] or part['y']!=q['part']['y']
                stats['pairs']+=1;stats['tile_edge_pairs']+=int(edge);stats['over_45']+=int(angle>45)
                stats['tile_edge_over_45']+=int(edge and angle>45);stats['max_degrees']=max(stats['max_degrees'],angle)
                if angle>60 and edge:outliers.append(dict(actor=a['name'],context=a['id'],facing=f['facing'],step=f['step'],xy=[x,y],degrees=round(angle,2),surface=p['zone']))
    return dict(scope='All modeled unique poses; same anatomical group neighbors touching changed normals. Includes authored/generated joins and overlap edges.',
                families=dict(families),tile_edge_over_60=outliers)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,default=Path('build/remaster-geometry/reference-npc-detail-v6/reference-npc-profile.toml'))
    parser.add_argument('--manifest',type=Path,default=Path('build/remaster-geometry/npc-workbench/poses.json'))
    parser.add_argument('--output',type=Path,default=Path('build/remaster-geometry/biped-normals-v1'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    source=args.profile.read_text();main_path=Path('remaster/profiles/ALttP/alttp-profile.toml');main_source=main_path.read_bytes()
    candidate,report,assets,poses=generate(json.loads(args.manifest.read_text()),source)
    target=args.output/'biped-profile.toml';atomic_write(target,candidate)
    subprocess.run([str((args.manifest.parent/'alttp-profile-validate').resolve()),str(target.resolve())],check=True)
    preview(args.output,poses,assets)
    atomic_write(args.output/'coverage-and-seams.json',json.dumps(audit(poses,assets,set(report['tiles'])),indent=2)+'\n')
    atomic_write(args.output/'source-profile.toml',source)
    atomic_write(args.output/'generator-source.py',Path(__file__).read_text())
    atomic_write(args.output/'report.json',json.dumps(report,indent=2)+'\n')
    assert args.profile.read_text()==source and main_path.read_bytes()==main_source
    print(json.dumps({k:v for k,v in report.items()if k not in ('families','tiles','reuse_disagreements','deferred_poses')},indent=2))


if __name__=='__main__':main()
