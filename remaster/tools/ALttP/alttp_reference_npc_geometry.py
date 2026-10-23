#!/usr/bin/env python3
"""Reference-guided second pass for Armos, sword guards, Ganon and Agahnim.

Each family has explicit semantic part masks in assembled artwork coordinates.
Reference pictures supplied in the authoring conversation guide surface shape,
not the photographs' lighting or a brightness-to-depth conversion. Source
profile is never written. Detailed user normals and all existing heights survive.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tomllib

import numpy as np

from alttp_humanoid_geometry import asset_blocks, variant_asset
from alttp_link_geometry import encode, placeholder, png, unit
from alttp_npc_workbench import atomic_write, merge_profile


KEYS = {'armos-knight', 'blue', 'green', 'red', 'ganon', 'agahnim'}
COLORS = {name: color for name, color in zip(
    ['helmet', 'horn', 'visor', 'face', 'shield', 'blade', 'armor', 'arm',
     'hand', 'boot', 'hood', 'robe', 'sleeve', 'trim', 'snout', 'tusk',
     'belly', 'cape', 'eye', 'jewel'],
    [(80, 120, 240), (250, 225, 150), (70, 65, 85), (240, 170, 120),
     (180, 100, 55), (200, 220, 255), (145, 100, 230), (60, 175, 180),
     (245, 165, 70), (130, 90, 60), (60, 190, 65), (30, 120, 60),
     (90, 180, 110), (225, 195, 70), (230, 130, 170), (245, 240, 205),
     (70, 150, 220), (200, 60, 70), (240, 60, 60), (190, 240, 50)])}
COLORS.update(skull=(235,235,215),socket=(65,55,75),necklace=(245,190,45),
              plate=(175,115,50),rim=(240,205,65))


def dome(x, y, cx, cy, rx, ry, top=0.45, strength=0.82):
    """Independent rounded part: X flank, +Y front, +Z crown, slight -Y crest."""
    u = float(np.clip((x-cx)/max(1, rx), -0.97, 0.97))
    v = float(np.clip((y-cy)/max(1, ry), -0.97, 0.97))
    nz = top-v*0.70
    ny = math.sqrt(max(0.04, 1-u*u))*0.88
    # Continuous crown rolloff. The previous if(v < -.70) abruptly replaced
    # +Y with -Y, drawing a green stripe across every rounded abdomen/cap.
    crest = smoothstep(0.58, 0.98, -v) * (1-smoothstep(0.45, 0.90, abs(u)))
    ny = ny*(1-crest) + (-0.10)*crest
    return unit((u*strength, ny, nz))


def smoothstep(a, b, x):
    t=max(0.0,min(1.0,(x-a)/(b-a)))
    return t*t*(3-2*t)


def cloth_normal(x, y, center=0, anchor=0, width=12, length=26, amplitude=1.5):
    """Normals from one continuous draped-depth function across tile/part edges.

    Folds fan from shoulder attachments, deepen toward the hem, and bend with
    the hanging fabric. Derivatives, not painted color or arbitrary normal
    stripes, define alternating ridges/valleys and their vertical undulation.
    """
    def depth(xx, yy):
        t=(yy-anchor)/length
        spread=0.65+0.45*smoothstep(0,1,t)
        u=(xx-center)/(width*spread)
        envelope=0.35+0.65*smoothstep(0,1,t)
        phase=u*math.pi*2.3 + 0.42*math.sin(t*3.1) + 0.18*u*t
        folds=math.cos(phase)+0.23*math.cos(phase*1.9+0.7)
        return amplitude*envelope*folds - 0.018*(xx-center)**2 + 0.55*math.sin(t*4+u)
    e=0.15
    dx=(depth(x+e,y)-depth(x-e,y))/(2*e)
    dy=(depth(x,y+e)-depth(x,y-e))/(2*e)
    return unit((-dx,1,0.12+dy))


def guard_opening(x, y, facing):
    """Artwork-aligned opening, rim and cheek returns; never a head sphere."""
    if facing=='north':
        return None
    # Canonicalize side-facing artwork to east before applying landmarks.
    xx=15-x if facing=='west' else x
    if facing in ('east','west'):
        left,right,top,bottom=5.5,13.8,8.8,12.6
    else:
        left,right,top,bottom=3.4,11.6,8.2,12.0
    if not (left-1<=xx<=right+0.5 and top-0.6<=y<=bottom+1):
        return None
    # Upper lip faces down into the opening; side walls turn toward its center.
    # Skin at the bottom is authored independently by the caller.
    if y<top+0.65:
        n=unit((0,0.64,-0.77));label='visor'
    elif xx<left+0.8:
        n=unit((0.75,0.65,0.10));label='visor'
    elif xx>right-0.65:
        n=unit((-0.75,0.65,0.10));label='visor'
    elif y>bottom-0.5:
        n=unit((0,0.60,0.80));label='visor'
    else:
        n=unit((0.08,0.99,0.02));label='visor'
    if facing=='west':n[0]*=-1
    return label,n


def cylinder(x, cx, radius, z=0.22):
    u = float(np.clip((x-cx)/max(1, radius), -0.95, 0.95))
    return unit((u*0.85, math.sqrt(max(0.08, 1-u*u)), z))


def flat_bevel(x, y, bounds, front=(0, 0.98, 0.20)):
    x0, y0, x1, y1 = bounds
    # Flat faces remain exactly planar. Only the actual outside border bevels.
    if x-x0 < 1.2:
        return unit((-0.65, 0.72, 0.20))
    if x1-x < 1.2:
        return unit((0.65, 0.72, 0.20))
    if y-y0 < 1.2:
        return unit((0, 0.55, 0.84))
    if y1-y < 1.2:
        return unit((0, 0.94, -0.34))
    return unit(front)


def occurrences(parts):
    result, seen = [], set()
    for part in parts:
        sig = (part['tile_hash'], part['x'], part['y'], part['hflip'], part['vflip'])
        if sig in seen:
            continue
        seen.add(sig)
        for i, ink in enumerate(part['indices']):
            if ink:
                result.append(dict(part=part, i=i, ink=ink,
                    x=part['x']+(7-i%8 if part['hflip'] else i%8),
                    y=part['y']+(7-i//8 if part['vflip'] else i//8)))
    return result


def bounds(points):
    return (min(p['x'] for p in points), min(p['y'] for p in points),
            max(p['x'] for p in points), max(p['y'] for p in points))


def armos(pose):
    result = []
    # Jump expansion moves quadrants apart. Pull each quadrant back into the
    # reference assembly for geometry, keeping actual placements for previews.
    for p in occurrences(pose['parts']):
        part = p['part']
        slot = part['slot']
        slot_parts = [q for q in pose['parts'] if q['slot']==slot]
        ox, oy = min(q['x'] for q in slot_parts), min(q['y'] for q in slot_parts)
        x = p['x']-ox+(16 if slot in (0, 2) else 0)
        y = p['y']-oy+(16 if slot in (0, 1) else 0)
        if x >= 19 and y >= 15:
            label = 'shield'
            n = flat_bevel(x, y, (19, 15, 31, 30))
            h = 13-(y-15)*0.65
        elif x <= 6 and 9 <= y <= 21:
            label = 'blade'
            n = unit((-0.38 if x<=2 else 0.38 if x>=5 else 0, 0.60, 0.80))
            h = 19-(y-9)*0.65
        elif y < 9 and (x < 8 or x > 23):
            label = 'horn'
            cx = 6 if x<16 else 25
            n = dome(x, y, cx, 4, 2.0, 6, 0.60)
            h = 27-y*0.32
        elif ((x-16)/9.0)**2+((y-7)/9.5)**2 < 1.15:
            label = 'helmet'
            n = dome(x, y, 16, 6, 7.5, 8, 0.55)
            h = 22+4*math.sqrt(max(0, 1-((x-16)/9)**2))-max(0, y-6)*0.7
            if ((x-16)/3.8)**2+((y-13)/1.8)**2 < 1:
                label = 'visor'
                n = dome(x,y,16,13,4,2,0.18)
                h -= 1
        elif y >= 25 and x < 20:
            label = 'boot'
            cx = 9 if x<13 else 17
            n = dome(x, y, cx, 28, 4, 3, 0.80)
            h = max(0, 4-(y-25)*0.6)
        elif x < 9 or x > 24:
            label = 'hand' if p['ink'] in (3, 4) else 'armor'
            cx = 5 if x<16 else 27
            n = dome(x, y, cx, 13, 4, 5, 0.35)
            h = 17-(y-12)*0.65
        else:
            label = 'armor'
            # No rectangular center patch: the reference cuirass is rounded.
            n = dome(x,y,15,20,8,7,0.30)
            h = 15-(y-15)*0.85
        result.append(dict(p, label=label, normal=n, height=max(0, round(h))))
    return result


def guards(pose):
    result = []
    facing = pose['facing']
    head = [q for q in pose['parts'] if q['slot']==0]
    hx, hy = min(q['x'] for q in head), min(q['y'] for q in head)
    side = facing in ('east', 'west')
    for p in occurrences(pose['parts']):
        part = p['part']
        x, y, slot = p['x']-hx, p['y']-hy, part['slot']
        # Each pose adapter keeps helmet in slot0. Body/weapons are separate
        # slots, with different subdivisions for front/back and side facings.
        if slot==0:
            if y<6 and (x<3 or x>12):
                label = 'helmet'
                n = dome(x, y, 7.5, 5, 7, 6, 0.65)
                h = 22-y*0.25
            elif ((side and 9<=y<=12 and 5<x<14) or
                  (not side and facing=='south' and 8<=y<=11 and 3<x<12)):
                label = 'face' if p['ink'] in (3, 4) else 'visor'
                if label=='face':
                    n = dome(x, y, 8, 11.5, 4.5, 3, 0.30)
                    h = 15-(y-9)*0.55
                else:
                    # Visor is a shallow recess, not a giant rectangular plane
                    # covering the cheek plates and the helmet's rolled rim.
                    n = cylinder(x,8,6,0.08)
                    h = 16-(y-8)*0.60
            else:
                label = 'helmet'
                n = dome(x, y, 7.5, 5.5, 7, 7, 0.48)
                h = 19+3*math.sqrt(max(0, 1-((x-7.5)/8)**2))-max(0, y-5)*0.50
            opening=guard_opening(x,y,facing)
            if opening:
                label,n=opening
                h=16-(y-8)*0.60
                if p['ink'] in (3,4) and y>=10:
                    label='face'
                    n=dome(x,y,11 if facing=='east' else 4 if facing=='west' else 7.5,
                           12.3,2.6,1.8,0.25)
        else:
            blade_slots = (1, 2) if side or facing=='south' else (5, 6)
            shield_slots = (5, 6) if side else (3, 4)
            if slot in blade_slots:
                label = 'blade'
                ps = [q for q in pose['parts'] if q['slot'] in blade_slots]
                pts = occurrences(ps)
                bx0, by0, bx1, by1 = bounds(pts)
                horizontal = bx1-bx0>by1-by0
                cross = p['y']-(by0+by1)/2 if horizontal else p['x']-(bx0+bx1)/2
                n = unit((0 if horizontal else 0.45*math.copysign(1,cross),
                          0.45 if horizontal else 0.60, 0.89 if horizontal else 0.80))
                h = 8 if horizontal else 12-(p['y']-by0)*0.45
            elif slot in shield_slots:
                label = 'shield'
                ps = occurrences([q for q in pose['parts'] if q['slot'] in shield_slots])
                b = bounds(ps)
                n = flat_bevel(p['x'], p['y'], b)
                h = 12-(p['y']-b[1])*0.75
            else:
                # Assemble both mirrored body halves before independent arm,
                # breastplate, skirt and boot segmentation.
                if y >= 20:
                    label = 'boot'
                    cx = 4 if x<8 else 11
                    n = dome(x, y, cx, 21, 3.5, 3.5, 0.85)
                    h = max(0, 4-(y-20)*0.6)
                elif p['ink'] in (3, 4):
                    label = 'hand'
                    cx = 2 if x<8 else 13
                    n = dome(x, y, cx, 15, 3, 3.5, 0.35)
                    h = 11-(y-12)*0.6
                elif x<3 or x>12:
                    label = 'arm'
                    n = cylinder(x, 1 if x<8 else 14, 3.5, 0.4)
                    h = 12-(y-11)*0.75
                else:
                    label = 'armor'
                    n = cylinder(x,7.5,6,0.28)
                    if 6<=x<=9 and 13<=y<=16:
                        n = unit((0,0.96,0.28))
                    h = 13-(y-11)*0.9
        result.append(dict(p, label=label, normal=n, height=max(0, round(h))))
    return result


def agahnim(pose):
    result = []
    ink_points=occurrences(pose['parts'])
    face_visible=any(p['ink'] in (3,4) and abs(p['x']-7.5)<4.5 and 9<=p['y']+8<=13
                     for p in ink_points)
    for p in ink_points:
        x, y = p['x']-7.5, p['y']+8
        ax = abs(x)
        # Sprite states 2–7 reveal the back of the hood, not a different face.
        # Palette skin patches evidence face/hands; embroidery remains flat.
        if y<9 and ax<7:
            label = 'hood'
            n = dome(x, y, 0, 4.5, 7, 5.5, 0.55)
            h = 25+2*math.sqrt(max(0,1-(x/8)**2))-y*0.30
        elif y<16 and ax<6:
            label = 'face' if face_visible and 9<=y<=13 and ax<4.5 else 'hood'
            if label=='face':
                n = dome(x,y,0,11,4.5,3,0.20)
                h = 22-(y-9)*0.65
            else:
                n = cylinder(x,0,6,0.30)
                h = 24-(y-9)*0.7
        elif ax>7 and p['ink'] in (3,4):
            label = 'hand'
            side = -1 if x<0 else 1
            n = dome(x,y,side*11,18 if pose['step']%2==0 else 7,3.5,3,0.55)
            h = 17-(y-15)*0.5
        elif ax>7 and y<24:
            label = 'sleeve'
            side = -1 if x<0 else 1
            # Broad hanging sleeve has a top roll, outer cylinder and a dark
            # recessed opening. Not a whole-robed-body sphere.
            base=dome(x,y,side*10,15,4.5,8,0.35)
            folded=cloth_normal(x,y,side*10,9,5,18,1.05)
            n=unit(base*0.30+folded*0.70)
            h = 21-(y-12)*0.7
        elif y>=29:
            label = 'boot'
            n = dome(x,y,-5 if x<0 else 5,30,3,2.5,0.75)
            h = max(0,3-(y-29))
        else:
            label = 'robe'
            # Center stole is a genuine flat vertical panel. Embroidery doesn't
            # produce lumps. Adjacent heavy robe has authored longitudinal folds.
            folded=cloth_normal(x,y,0,9,11,23,1.65)
            # Stole lies over the drape, with gentler but continuous folds.
            stole=1-smoothstep(2.3,4.8,ax)
            n=unit(folded*(1-0.45*stole)+unit((x*0.025,0.99,0.12))*0.45*stole)
            if ax<3.5:label='trim'
            h = 18-(y-16)*0.95+(0.7 if label=='trim' else 0)
        result.append(dict(p,label=label,normal=n,height=max(0,round(h))))
    return result


def ganon_roles(actor):
    """Stable semantic role from source OAM part provenance, not global body y."""
    roles = {}
    for pose in actor['poses']:
        if pose['step']>7:
            continue
        for part in pose['parts']:
            s = part['slot']
            role = ('head' if s in (6,7) else 'shoulder' if s in (8,9) else
                    'arm' if s in (5,10,11,12) else 'belly' if s in (13,14) else 'boot')
            roles.setdefault(part['tile_hash'],role)
    # Later frames reshuffle slots, so use hash provenance first, then the
    # explicit later-frame slot table only for genuinely new artwork.
    for pose in actor['poses']:
        for part in pose['parts']:
            s = part['slot']
            role = ('shoulder' if s in (5,6) else 'belly' if s in (7,8) else
                    'head' if s in (9,10) else 'boot' if s in (11,12) else 'arm')
            roles.setdefault(part['tile_hash'],role)
    # Ganon_Draw's frame15 reshuffles the full body and duplicates its head.
    # Its head graphics are the same 0x410..0x530 DMA block in both VRAM banks.
    # Hash-first fallback alone misclassified new head hashes as arms and a
    # new torso quadrant as head, introducing vertical component boundaries.
    for pose in actor['poses']:
        for part in pose['parts']:
            if part['address'] & 0x7ff in (0x410,0x420,0x430,0x510,0x520,0x530):
                roles[part['tile_hash']]='head'
            elif pose['step']==15:
                s=part['slot']
                if s in (6,7):roles[part['tile_hash']]='shoulder'
                elif s in (8,9):roles[part['tile_hash']]='belly'
            elif pose['step']==16:
                # Final draw-table assembly is not the steps8–14 layout:
                # slots5/6 form one mirrored head, 7/8 are pauldrons,
                # 9–12 arms, 13/14 torso and 15/16 feet. Treating 5/6 as
                # independent shoulders and 13/14 as arms makes two inward
                # curves at the center of otherwise continuous surfaces.
                s=part['slot']
                roles[part['tile_hash']]=('head' if s in (5,6) else
                    'shoulder' if s in (7,8) else 'arm' if s in (9,10,11,12)
                    else 'belly' if s in (13,14) else 'boot')
    return roles


def top_facing(normal,x,y,cx,top_y,rx,depth,strength=1):
    """World-space upward cap blending continuously into the visible front.

    Depth is an anatomical surface region, not a new cap at every tile row.
    Center of the exposed cap is near +Z; left/right curvature remains lateral.
    """
    t=(y-top_y)/max(1,depth)
    weight=(1-smoothstep(0.08,1,t))*strength
    u=float(np.clip((x-cx)/max(1,rx),-1,1))
    cap=unit((u*0.35,0.06,1))
    return unit(normal*(1-weight)+cap*weight)


def ganon(pose, roles):
    points = occurrences(pose['parts'])
    # Only back/turn cloth assemblies expose a broad cape. Red front-facing
    # shoulder plates and tiny decorative edges are NOT isolated cape patches.
    cape_parts=set()
    if pose['step']>=8:
        by_slot=defaultdict(list)
        for q in points:by_slot[q['part']['slot']].append(q)
        for slot,qs in by_slot.items():
            red_count=0
            for q in qs:
                c=q['part']['colors'][q['i']]
                r,g,b=[(c>>s)&31 for s in (0,5,10)]
                red_count+=r>g*1.55 and r>b*1.55 and r>12
            if red_count>=12 and red_count/len(qs)>0.20:
                cape_parts.add(slot)
    head_points = [p for p in points if roles[p['part']['tile_hash']]=='head']
    hb = bounds(head_points)
    head_cx, head_cy = (hb[0]+hb[2])/2,(hb[1]+hb[3])/2
    # Arm sprites overlap and split the SAME limb between slots. Geometry must
    # use the whole limb, not restart its dome at each 8/16px OAM subdivision.
    arm_bounds={}
    for side in (-1,1):
        limb=[q for q in points if roles[q['part']['tile_hash']]=='arm'
              and (-1 if q['x']<7.5 else 1)==side]
        if limb:arm_bounds[side]=bounds(limb)
    result = []
    # Segment actual material regions on each armor OAM part. Outline pixels
    # inherit their nearest interior, rather than becoming holes or extra beads.
    armor_material={}
    for slot in {p['part']['slot'] for p in points if roles[p['part']['tile_hash']] in ('shoulder','belly')}:
        ps=[q for q in points if q['part']['slot']==slot]
        interior=[q for q in ps if q['ink']!=5]
        for q in ps:
            nearest=min(interior,key=lambda r:(r['x']-q['x'])**2+(r['y']-q['y'])**2) if interior else q
            armor_material[slot,q['x'],q['y']]=nearest['ink']
    for p in points:
        part, x, y = p['part'],p['x'],p['y']
        role = roles[part['tile_hash']]
        color=part['colors'][p['i']]
        red,green,blue=[(color>>s)&31 for s in (0,5,10)]
        if part['slot'] in cape_parts and role not in ('head','boot'):
            # Back/turn frames expose the red cape. Its broad cloth panel must
            # not inherit abdomen or arm geometry from shuffled OAM slots.
            label='cape'
            n=cloth_normal(x,y,7.5,-9,16,33,2.4)
            h=max(0,20-(y+3)*0.8)
        elif role=='head':
            u,v = (x-head_cx)/9,(y-hb[1])/max(1,hb[3]-hb[1])
            label = 'face'
            n = ganon_surface(x,y,head_cx,head_cy-1,9,8,0.50)
            n=top_facing(n,x,y,head_cx,hb[1],9,6)
            h = 28-(y-hb[1])*0.45
            if v>0.48 and abs(u)<0.75:
                label = 'snout'
                snout=ganon_surface(x,y,head_cx,head_cy+3,6,3.7,0.55)
                weight=smoothstep(0.48,0.68,v)*(1-smoothstep(0.52,0.78,abs(u)))
                n=unit(n*(1-weight)+snout*weight)
                # The muzzle has its own top ledge below the forehead.
                n=top_facing(n,x,y,head_cx,head_cy+0.8,5.5,3.3,
                             1-smoothstep(0.55,0.85,abs(u)))
                h += 1.8
            if p['ink']==1 and v>0.35:
                label = 'tusk'
                n = dome(x,y,x,head_cy+1,1.8,4,0.75)
                h += 2
            if v<0.23 and abs(u)>0.65:
                label = 'horn'
                n = dome(x,y,head_cx+math.copysign(7,u),hb[1]+2,2.5,4,0.75)
                h += 2
        elif role=='shoulder':
            label = 'armor'
            ps = [q for q in points if q['part']['slot']==part['slot']]
            b = bounds(ps)
            cx,cy = (b[0]+b[2])/2,(b[1]+b[3])/2
            n = ganon_surface(x,y,cx,cy,7.5,6,0.65)
            n=top_facing(n,x,y,cx,b[1],7.5,6)
            h = 24-(y-b[1])*0.55
            # Gold border is a thin bevel, not separate beads or bumps from
            # bright specular pixels in the reference.
            if y>b[3]-2:
                label = 'trim'
                weight=smoothstep(b[3]-2,b[3],y)*0.65
                n=unit(n*(1-weight)+unit(((x-cx)*0.06,0.90,0.32))*weight)
        elif role=='arm':
            # Use the articulated OAM arm extent, not the complete figure.
            b = arm_bounds[-1 if x<7.5 else 1]
            cx,cy = (b[0]+b[2])/2,(b[1]+b[3])/2
            label = 'hand' if y>cy+1 else 'arm'
            n = ganon_surface(x,y,cx,cy, max(3,(b[2]-b[0]+1)/2),max(4,(b[3]-b[1]+1)/2),0.35)
            n=top_facing(n,x,y,cx,b[1],max(3,(b[2]-b[0]+1)/2),5)
            h = 19-(y-b[1])*0.60
        elif role=='belly':
            label = 'belly'
            # Two half sprites form ONE abdomen, avoiding a center seam.
            n = ganon_surface(x,y,7.5,10,15,9,0.34)
            h = 14-(y-5)*0.60
            # Gold painted details follow the abdomen instead of replacing a
            # whole scanline with constant +Y normals (the second stripe cause).
        else:
            label = 'boot'
            ps = [q for q in points if q['part']['slot']==part['slot']]
            b = bounds(ps)
            cx,cy = (b[0]+b[2])/2,(b[1]+b[3])/2
            n = dome(x,y,cx,cy,max(3,(b[2]-b[0]+1)/2),5,0.85)
            n=top_facing(n,x,y,cx,b[1],max(3,(b[2]-b[0]+1)/2),5)
            h = max(0,5-(y-b[1])*0.6)
        # These chest landmarks are evidenced for front assemblies only. Do
        # not stamp a front-facing skull onto the sideways/back phase artwork.
        if pose['step']<8 and label!='cape' and role in ('shoulder','belly'):
            material=armor_material[part['slot'],x,y]
            label,n=ganon_armor_detail(x,y,role,material,p['ink'],n,
                bounds([q for q in points if q['part']['slot']==part['slot']]))
            if role=='shoulder' and label in ('armor','rim'):
                b=bounds([q for q in points if q['part']['slot']==part['slot']])
                n=top_facing(n,x,y,(b[0]+b[2])/2,b[1],7.5,6)
        result.append(dict(p,label=label,normal=n,height=max(0,round(h))))
    return result


def ganon_armor_detail(x,y,role,material,ink,base,part_bounds):
    """Sprite-evidenced pendant, beads, gold rims and distinct armor planes.

    These normal-only relief features intentionally have edges. Painted plate
    highlights are not separate bumps; the plate remains one physical surface.
    """
    if role=='belly':
        # Pendant spans BOTH mirrored halves: one cranium, recessed paired eye
        # sockets, cheek bevels and a short toothed jaw. Never one skull per tile.
        u,v=(x-7.5)/4.8,(y-6.5)/4.5
        if u*u+v*v<1.12 and 3<=y<=11:
            if ink==5 and 6<=y<=8 and abs(x-7.5)>0.7:
                eye_center=5.5 if x<7.5 else 9.5
                return 'socket',unit((-(x-eye_center)*0.40,0.72,-0.30))
            if y>=9:
                # Jaw's front remains a plane; bottom edge rolls under it.
                return 'skull',unit(((x-7.5)*0.12,0.97,0.18-(y-9)*0.22))
            return 'skull',ganon_surface(x,y,7.5,6,4.7,3.5,0.55)
        if material in (3,4):
            # Separate left/right red inset armor, angled inward around pendant.
            sign=-1 if x<7.5 else 1
            plate=unit((sign*0.28,0.94,0.18))
            return 'plate',unit(base*0.30+plate*0.70)
        if material in (6,7):
            # Gold lower breastplate/tabard: curved edge, shallow central ridge,
            # not the same large cylindrical abdomen as the earlier pass.
            u=(x-7.5)/10
            ridge=0.17*math.tanh((x-7.5)/1.5)
            plate=unit((u*0.25+ridge,0.97,0.28-(y-11)*0.055))
            return 'plate',unit(base*0.35+plate*0.65)
        return 'belly',base
    x0,y0,x1,y1=part_bounds
    cx=(x0+x1)/2
    # Silver necklace runs down each inner shoulder/chest edge in the sprite.
    if material in (1,2):
        # Each 2–3px bead rounds independently, anchored in the artwork part.
        bead_y=y0+round((y-y0)/3)*3
        inner=x1-1 if cx<7.5 else x0+1
        return 'necklace',ganon_surface(x,y,inner,bead_y,1.7,1.6,0.32)
    if material in (6,7):
        # Gold border follows the pauldron's curved lower edge. Its return
        # normal differs from the broad red face, not a row-wide flat band.
        u=(x-cx)/max(3,(x1-x0)/2)
        return 'rim',unit((u*0.50,0.78,0.50))
    if material in (3,4):
        # Shallow armored dome with a broad face and a rolled perimeter.
        return 'armor',ganon_surface(x,y,cx,(y0+y1)/2, max(4,(x1-x0)/2+2),
                                     max(4,(y1-y0)/2+2),0.48)
    return 'armor',base


def ganon_surface(x,y,cx,cy,rx,ry,top):
    """Front-exposed rounded surface, without an invented far-crown roll.

    The generic dome's -Y crest is suitable for a visible top rolling away,
    but on truncated belly/arm tiles it makes a blue stripe at their first row.
    These front surfaces instead vary continuously through their full extent.
    """
    u=float(np.clip((x-cx)/max(1,rx),-0.97,0.97))
    v=float(np.clip((y-cy)/max(1,ry),-0.97,0.97))
    return unit((u*0.82,math.sqrt(max(0.04,1-u*u))*0.88,top-v*0.70))


def ramp(asset):
    """Recognize preserved whole-tile row ramps for reporting only."""
    h = asset.get('height')
    return bool(h and all(h[y*8+x]==h[0]-y for y in range(8) for x in range(8)))


def generate(manifest, source):
    parsed = tomllib.loads(source)
    assets = {a['tile_hash']:a for a in parsed.get('assets',[])}
    actors = [a for a in manifest['actors'] if a['draw_model']['key'] in KEYS]
    hashes = {p['tile_hash'] for a in actors for f in a['poses'] for p in f['parts']}
    owners = defaultdict(set)
    for a in manifest['actors']:
        for f in a['poses']:
            for p in f['parts']:
                owners[p['tile_hash']].add(a['name'])
    names = {a['name'] for a in actors}
    # Guard bodies/helmets/weapons are intentionally reused by other equipped
    # guards. Same canonical artwork may receive the shared reference geometry;
    # list those affected actors explicitly rather than leaving most guards flat.
    compatible = {a['name'] for a in manifest['actors'] if a['draw_model']['key'] in
                  ('soldier','fast','spear','archer','bush-archer','javelin','bush-javelin','bombguard','recruit')}
    blocked = {h:'Shared with excluded actor' for h in hashes if owners[h]-names-compatible}
    blocked.update({h:'Shared background identity' for h in hashes & set(manifest['background_shared_hashes'])})
    blocked.update({h:'Existing authored geometry variants' for h in hashes if variant_asset(assets.get(h,{}))})
    blocked.update({r['tile_hash']:'Existing named context' for r in parsed.get('tile_contexts',[])
                    if r['tile_hash'] in hashes})
    proposals = defaultdict(lambda:defaultdict(list))
    semantic_poses, skipped, seen = [], [], set()
    for a in actors:
        roles = ganon_roles(a) if a['draw_model']['key']=='ganon' else None
        for f in a['poses']:
            if a.get('graphics_inherited'):
                skipped.append(a['id'])
                continue
            sig = (a['name'],f['facing'],tuple((p['tile_hash'],p['x'],p['y'],p['hflip'],p['vflip']) for p in f['parts']))
            if sig in seen:
                continue
            seen.add(sig)
            key = a['draw_model']['key']
            pixels = (armos(f) if key=='armos-knight' else agahnim(f) if key=='agahnim'
                      else ganon(f,roles) if key=='ganon' else guards(f))
            semantic_poses.append((a,f,pixels))
            for p in pixels:
                h = p['part']['tile_hash']
                if h in blocked:
                    continue
                part=p['part']
                n = p['normal'].copy()
                if not assets.get(h,{}).get('normal_ignore_tile_transform'):
                    n[0] *= -1 if part['hflip'] else 1
                    n[1] *= -1 if part['vflip'] else 1
                proposals[h][p['i']].append((n,p['height'],p['label']))
    edits, details, disagreements = {}, {}, []
    for h,data in sorted(proposals.items()):
        original = assets.get(h,{})
        need_n = 'normal_xyz' not in original or placeholder(original)
        # Existing row ramps may have been deliberately authored by the user.
        # Their appearance alone is NOT permission to overwrite those heights.
        need_h = 'height' not in original
        if not (need_n or need_h):
            continue
        ns,hs,labels = [128,128,255]*64,[0]*64,Counter()
        for i,samples in data.items():
            # Semantic mode wins over blending incompatible surface types.
            # Never smear a shield plane into a helmet/torso cylinder.
            dominant = Counter(s[2] for s in samples).most_common(1)[0][0]
            chosen=[s for s in samples if s[2]==dominant]
            vectors=np.array([s[0] for s in chosen])
            n=unit(np.median(vectors,axis=0))
            heights=[s[1] for s in chosen]
            ns[i*3:i*3+3]=encode(n)
            hs[i]=round(float(np.median(heights)))
            labels.update(s[2] for s in samples)
            angle=max(math.degrees(math.acos(float(np.clip(s[0]@n,-1,1)))) for s in samples)
            if len({s[2] for s in samples})>1 or angle>30 or max(s[1] for s in samples)-min(s[1] for s in samples)>2:
                disagreements.append(dict(tile_hash=h,pixel=i,labels=dict(Counter(s[2] for s in samples)),
                    angle=round(angle,2),height_spread=max(s[1] for s in samples)-min(s[1] for s in samples)))
        layers={}
        if need_n: layers['normal_xyz']=ns
        if need_h: layers['height']=hs
        edits[h]=layers
        details[h]=dict(owners=sorted(owners[h]),layers=list(layers),surfaces=dict(labels),
                        preserved_row_ramp=ramp(original),replaced_placeholder=placeholder(original))
    def remove(block,asset):
        h=asset['tile_hash']
        if h in edits:
            import re
            for k in edits[h]:
                block=re.sub(rf'(?m)^{k}\s*=.*\n?', '', block)
        return block
    candidate,_=merge_profile(asset_blocks(source,remove),edits)
    new=tomllib.loads(candidate)
    actual={a['tile_hash']:a for a in new['assets']}
    for h,a in assets.items():
        for k,v in a.items():
            assert actual[h][k]==edits.get(h,{}).get(k,v),(h,k)
    for h in set(actual)-set(edits):
        assert actual[h]==assets[h],h
    report=dict(source_sha256=hashlib.sha256(source.encode()).hexdigest(),
        families=sorted(names),shared_guard_actors=sorted({name for h in edits for name in owners[h]-names}),
        changed_tiles=len(edits),added_normals=sum('normal_xyz'in v and 'normal_xyz'not in assets.get(h,{}) for h,v in edits.items()),
        replaced_placeholders=sum(d['replaced_placeholder'] for d in details.values()),
        authored_height_relief_tiles=sum('height'in v for v in edits.values()),
        preserved_row_ramps=sum(d['preserved_row_ramp'] for d in details.values()),blocked=blocked,
        skipped_inherited_contexts=sorted(set(skipped)),reuse_disagreement_tiles=len({d['tile_hash'] for d in disagreements}),
        reuse_disagreements=disagreements,tiles=details,
        limitations=['Hand-coded semantic masks guided by supplied references; not automatic 3D reconstruction.',
            'Existing detailed normals and all existing heights are preserved, including row ramps.',
            'Guard head/body slot contracts are specific to the current sword-guard adapter.',
            'Ganon later-phase role assignment and occluded pixels require live review.',
            'Shared-hash disagreement remains explicit; dominant surface avoids cross-part normal smearing.',
            'Custom local height placement and live GI/animation are not validated by preview lighting.'])
    return candidate,report,actual,semantic_poses


def preview(output, poses, assets):
    # One representative per family/facing, plus distinct Ganon/Agahnim phases.
    selected,seen=[],set()
    for a,f,pixels in poses:
        key=a['draw_model']['key']
        phase=f['step'] if key in ('agahnim','ganon') and f['step'] in (0,1,2,6,8,15,16) else 0
        sig=(a['name'],f['facing'],phase)
        if sig in seen:continue
        seen.add(sig);selected.append((a,f,pixels))
    width,rh=1280,208
    index=[]
    # Columns: artwork, semantic labels, normals, height, two opposing neutral lights.
    for page,start in enumerate(range(0,len(selected),6),1):
        rows=selected[start:start+6]
        rgb=bytearray([28,30,35]*width*rh*len(rows))
        for row,(a,f,pixels) in enumerate(rows):
            scale=min(5,180//max(1,f['bounds'][3]-f['bounds'][1]),196//max(1,f['bounds'][2]-f['bounds'][0]))
            visible={}
            for p in pixels:visible.setdefault((p['x'],p['y']),p)
            for (x,y),p in visible.items():
                part,i=p['part'],p['i'];asset=assets.get(part['tile_hash'],{})
                n=unit(np.array(asset.get('normal_xyz',[128,128,255]*64)[i*3:i*3+3])/127.5-1)
                if not asset.get('normal_ignore_tile_transform'):
                    n[0]*=-1 if part['hflip'] else 1;n[1]*=-1 if part['vflip'] else 1
                h=asset.get('height',[0]*64)[i];c=part['colors'][i]
                art=[(c>>s&31)*255//31 for s in (0,5,10)]
                hc=[min(255,max(0,round(h/32*255)))]*3
                lights=[[round(255*(0.12+0.88*max(0,float(n@unit(l)))))]*3 for l in ((-0.65,0.5,0.75),(0.65,0.5,0.75))]
                for col,color in enumerate([art,COLORS[p['label']],encode(n),hc,*lights]):
                    xx=col*212+8+(x-f['bounds'][0])*scale;yy=row*rh+8+(y-f['bounds'][1])*scale
                    for dy in range(scale):
                        for dx in range(scale):
                            at=((yy+dy)*width+xx+dx)*3;rgb[at:at+3]=bytes(color)
            index.append(dict(page=page,row=row,name=a['name'],context=a['id'],facing=f['facing'],step=f['step']))
        png(output/f'review-{page:02}.png',width,rh*len(rows),rgb)
    atomic_write(output/'review-index.json',json.dumps(index,indent=2)+'\n')
    atomic_write(output/'surface-colors.json',json.dumps(COLORS,indent=2)+'\n')


def belly_continuity(manifest, assets):
    """Pinned Ganon front belly crop, not a claim all boundaries should smooth.

    A few cropped pixels overlap other OAM surfaces. Report the complete
    denominator, outliers and maximum rather than hiding them in an average.
    """
    angles=[]
    for actor in manifest['actors']:
        if actor['name']!='Ganon':continue
        for pose in actor['poses']:
            if pose['step']>=8:continue
            pixels={}
            for p in occurrences(pose['parts']):
                part=p['part'];a=assets.get(part['tile_hash'],{})
                if 'normal_xyz' not in a:continue
                n=unit(np.array(a['normal_xyz'][p['i']*3:p['i']*3+3])/127.5-1)
                if not a.get('normal_ignore_tile_transform'):
                    n[0]*=-1 if part['hflip'] else 1
                    n[1]*=-1 if part['vflip'] else 1
                pixels.setdefault((p['x'],p['y']),n)
            for (x,y),n in pixels.items():
                if -3<=x<=17 and 5<=y<11 and (x,y+1) in pixels:
                    angles.append(math.degrees(math.acos(float(np.clip(n@pixels[x,y+1],-1,1)))))
    return dict(vertical_neighbor_pairs=len(angles),over_30_degrees=sum(v>30 for v in angles),
                maximum_degrees=max(angles,default=0),p95_degrees=float(np.percentile(angles,95)) if angles else 0,
                scope='Ganon steps0-7, both facings, x[-3,17], y[5,10]; includes overlapping part boundaries')


def upper_belly_continuity(manifest, assets):
    """The reported blue horizontal band's actual generated-profile samples."""
    angles=[]
    for actor in manifest['actors']:
        if actor['name']!='Ganon':continue
        for pose in actor['poses']:
            if pose['step']>7:continue
            pixels={}
            for p in occurrences(pose['parts']):
                part=p['part'];a=assets.get(part['tile_hash'],{})
                if 'normal_xyz' not in a:continue
                n=unit(np.array(a['normal_xyz'][p['i']*3:p['i']*3+3])/127.5-1)
                if not a.get('normal_ignore_tile_transform'):
                    n[0]*=-1 if part['hflip'] else 1
                    n[1]*=-1 if part['vflip'] else 1
                pixels.setdefault((p['x'],p['y']),(n,part['slot']))
            for (x,y),(n,slot) in pixels.items():
                if (3<=x<=12 and y in (3,4) and slot in (13,14) and
                    (x,y+1) in pixels and pixels[x,y+1][1] in (13,14)):
                    angles.append(math.degrees(math.acos(float(np.clip(n@pixels[x,y+1][0],-1,1)))))
    return dict(vertical_neighbor_pairs=len(angles),over_15_degrees=sum(v>15 for v in angles),
                maximum_degrees=max(angles,default=0),
                scope='Ganon steps0-7, both facings, x[3,12], y3->4 and4->5, visible belly slots13/14 only')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,default=Path('remaster/profiles/ALttP/alttp-profile.toml'))
    parser.add_argument('--manifest',type=Path,default=Path('build/remaster-geometry/npc-workbench/poses.json'))
    parser.add_argument('--output',type=Path,default=Path('build/remaster-geometry/reference-npc-geometry'))
    parser.add_argument('--compare-profile',type=Path,help='Previous candidate for the pinned belly continuity audit')
    parser.add_argument('--edited-profile',type=Path,help='User-edited previous candidate to preserve with a three-way normal merge')
    parser.add_argument('--generated-baseline',type=Path,help='Exact unedited generation corresponding to --edited-profile')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    source=args.profile.read_text()
    manifest=json.loads(args.manifest.read_text())
    candidate,report,assets,poses=generate(manifest,source)
    if args.edited_profile:
        if not args.generated_baseline:
            parser.error('--edited-profile requires --generated-baseline')
        edited_text=args.edited_profile.read_text()
        edited={a['tile_hash']:a for a in tomllib.loads(edited_text)['assets']}
        baseline={a['tile_hash']:a for a in tomllib.loads(args.generated_baseline.read_text())['assets']}
        replacements={}
        preserved=[]
        for h,a in assets.items():
            if a.get('normal_xyz')==baseline.get(h,{}).get('normal_xyz'):continue
            if edited.get(h,{}).get('normal_xyz')!=baseline.get(h,{}).get('normal_xyz'):
                preserved.append(h)
                continue
            replacements[h]=a['normal_xyz']
        def replace_normal(block,asset):
            import re
            h=asset['tile_hash']
            if h not in replacements:return block
            return re.sub(r'(?m)^normal_xyz\s*=.*$',
                          lambda _: 'normal_xyz = '+json.dumps(replacements[h]),block)
        candidate=asset_blocks(edited_text,replace_normal)
        assets={a['tile_hash']:a for a in tomllib.loads(candidate)['assets']}
        for h,a in edited.items():
            for k,v in a.items():
                assert assets[h][k]==(replacements[h] if k=='normal_xyz' and h in replacements else v),(h,k)
        report['three_way_merge']=dict(updated_normal_tiles=len(replacements),preserved_edited_normal_tiles=preserved,
            edited_profile=str(args.edited_profile),baseline=str(args.generated_baseline))
    report['belly_continuity']=belly_continuity(manifest,assets)
    report['upper_belly_continuity']=upper_belly_continuity(manifest,assets)
    if args.compare_profile:
        previous={a['tile_hash']:a for a in tomllib.loads(args.compare_profile.read_text())['assets']}
        report['previous_belly_continuity']=belly_continuity(manifest,previous)
        report['previous_upper_belly_continuity']=upper_belly_continuity(manifest,previous)
    target=args.output/'reference-npc-profile.toml';atomic_write(target,candidate)
    subprocess.run([str((args.manifest.parent/'alttp-profile-validate').resolve()),str(target.resolve())],check=True)
    preview(args.output,poses,assets)
    assert args.profile.read_text()==source
    atomic_write(args.output/'source-profile.toml',source)
    atomic_write(args.output/'report.json',json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('tiles','reuse_disagreements')},indent=2))


if __name__=='__main__':
    main()
