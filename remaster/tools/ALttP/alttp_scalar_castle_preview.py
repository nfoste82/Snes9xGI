"""Offline castle scalar-origin proposal; no canonical writes, no local edits.

Uses fresh profile bytes and ROM assembly. V2 is semantic context only, not a
per-pixel target to average. Mixed origins are explicit authoring gaps.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tomllib
import numpy as np
from PIL import Image
from alttp_atlas import Rom
from alttp_atlas_graphics import extract_graphics
from alttp_atlas_overworld import extract_overworld
from alttp_atlas_overworld_gfx import BaseOverworldGraphics
from alttp_visual_context import rom_palette, rgb555


def generate(out):
    snapshot = out / 'current-profile.toml'
    profile = tomllib.loads(snapshot.read_text())
    assets = {a['tile_hash']: a for a in profile['assets']}
    contexts = {}
    for c in profile.get('tile_contexts', []):
        contexts.setdefault(c['tile_hash'], {})[c['name']] = c
    bindings = {(b['area'], b['background'], b['x'], b['y']): b for b in profile.get('context_bindings', [])}
    rom = Rom(Path.home() / 'Downloads/Zelda_ALTTP.zip')
    qs = extract_overworld(rom); gs = extract_graphics(rom)
    resolver = BaseOverworldGraphics(gs, [{'area_id': 27, 'is_small': False,
                                          'gfx_index': rom.get_byte(0x80fc9c + 27)}], qs)
    pixels = {g.content_id: g.indices for g in gs}
    pal, _ = rom_palette(rom, 27, rom.get_byte(0x80fd1c + 27))
    local = np.full((1024, 1024), np.nan, np.float32)
    art = np.zeros((1024, 1024, 3), np.uint8)
    bases = np.zeros((128, 128), np.int16)
    records = []; gaps = []
    for qi in resolver.area_quadrants(27):
        for t in qs[qi].iter_tiles():
            g = resolver.resolve_tile(27, t)
            if g.status == 'animated_vram_slot': g = resolver.resolve_animated_phase(g, 0)
            x = ((qi & 7) - 3) * 512 + t.x * 8
            y = ((qi // 8) - 3) * 512 + t.y * 8
            a = assets.get(g.content_id, {})
            h = a.get('height')
            flip = ['normal', 'horz', 'vert', 'both'][int(t.flip_x) + 2*int(t.flip_y)]
            if a.get('geometry_by_flip'): h = a.get('flip_'+flip+'_height', h)
            cs = contexts.get(g.content_id, {})
            selected = next(iter(cs.values())) if len(cs) == 1 else None
            b = bindings.get((320, 1, (1536+x)//8, (1536+y)//8))
            if b and b['tile_hash'] == g.content_id: selected = cs.get(b['context'])
            if selected: h = selected.get('height', h)
            ix = np.array(list(pixels[g.content_id]), np.uint8).reshape(8, 8)
            hv = np.array(h, np.float32).reshape(8, 8) if h is not None else np.full((8,8), np.nan)
            if t.flip_x: ix = ix[:, ::-1]; hv = hv[:, ::-1]
            if t.flip_y: ix = ix[::-1]; hv = hv[::-1]
            local[y:y+8, x:x+8] = hv
            art[y:y+8, x:x+8] = np.array([rgb555(pal[t.palette*16+int(v)]) for v in ix.ravel()], np.uint8).reshape(8,8,3)
            # Classify the whole physical tile's assembly origin. Never select
            # grass/soil pixels independently and never average a V2 base array.
            base = 0; reason = 'ground/object origin'; gap = None
            architecture = 1100 <= t.map16_id <= 1265
            if architecture and t.palette == 7 and y < 128 and 208 <= x < 816:
                base = 40; reason = 'upper keep support'
                if y < 64 and 408 <= x < 616: base = 80
                if y < 16 and 432 <= x < 584: base = 120
            terrace = architecture and ((t.palette == 6 and np.isin(ix, [9,11]).any()) or (t.map16_id in [1235,1236,1237,1241] and y < 192))
            terrace &= 200 <= x < 824 and y < 672 and not (264 <= y < 304 and 480 <= x < 544)
            if terrace:
                base = 16 if h is not None and np.min(hv) == np.max(hv) == 24 else 40
                reason = 'terrace support, local-origin-aware'
                if t.palette == 6 and not np.isin(ix, [0,9,11]).all(): gap = 'mixed terrace material; one scalar cannot isolate deck'
            water = t.palette in (2,3) and np.isin(ix, [5,6,8,10,12,13,14,15]).all()
            colors = art[y:y+8,x:x+8]
            water_colors = np.array([[57,98,172],[49,74,123],[115,180,246],[90,131,197]])
            water = ((colors[:,:,None,:] == water_colors).all(axis=3).any(axis=2))
            if water.all() and not architecture: base = -40; reason = 'whole water tile'
            shore = x < 176 or x >= 848 or y >= 736
            if shore and t.palette == 2 and np.isin(ix,[1,2,3,4,7]).any() and h is not None:
                ground_relative = np.min(hv) < 0 or np.max(hv) <= 8
                if ground_relative: base = 0; reason = 'ground-relative bank relief'
                elif np.isin(ix,[0,1,2,3,4,7]).all(): base = -40; reason = 'whole water-relative bank tile'
                else: base = 0; gap = 'mixed water-relative bank/ground; retained ground origin'
            if architecture and t.palette == 7 and h is not None and np.min(hv)==np.max(hv)==0:
                gap = 'featureless masonry: missing local face gradient; no placement ramp invented'
            if water.any() and not water.all() and not architecture:
                gap = gap or 'mixed water/other surface; scalar cannot isolate water'
            bases[y//8,x//8] = base
            record = dict(world='light',background=1,x=(1536+x)//8,y=(1536+y)//8,offset=base,
                          expected_word=t.word,hash=g.content_id,reason=reason,has_height=h is not None)
            records.append(record)
            if gap: gaps.append(dict(x=record['x'], y=record['y'], issue=gap))
    expanded = np.repeat(np.repeat(bases,8,0),8,1)
    final = local + expanded
    np.savez_compressed(out/'proposal.npz',local=local,offset=bases,final=final)
    def image(name, field):
        gray = np.uint8(np.nan_to_num(np.clip((field+40)/200,0,1))*255)
        Image.fromarray(gray).save(out/name)
    image('baseline-height.png',local); image('candidate-height.png',final)
    image('placement-offset.png',expanded); Image.fromarray(art).save(out/'artwork.png')
    # Copy source text exactly, except schema promotion and appended sections.
    text = snapshot.read_text(); import re
    text = re.sub(r'^schema_version\s*=\s*\d+', 'schema_version = 37', text, count=1)
    if 'overworld_offsets' in profile: raise ValueError('fresh source already contains offsets; explicit merge required')
    text += '\n[overworld_placement]\nmode = "authored_overworld_offsets"\n'
    for r in records:
        text += '\n[[overworld_offsets]]\nworld = "light"\nbackground = 1\nx = %d\ny = %d\noffset = %d\n' % (r['x'],r['y'],r['offset'])
    candidate = out/'alttp-scalar-preview.toml'; candidate.write_text(text)
    parsed = tomllib.loads(text)
    for k,v in profile.items():
        if k != 'schema_version': assert parsed[k] == v, k
    (out/'placement-offsets.json').write_text(json.dumps(dict(schema='scalar-overworld-offsets-v1', light=records,dark=[],default=0,coverage=16384,guards='metadata-only; not runtime guards'),indent=2))
    (out/'authoring-gaps.json').write_text(json.dumps(gaps,indent=2))
    values = json.dumps(dict(local=np.nan_to_num(local).astype(int).tolist(),offset=bases.tolist(),presence=np.isfinite(local).astype(int).tolist()))
    html = '''<!doctype html><meta charset="utf-8"><title>Scalar castle preview</title><style>body{background:#222;color:white}canvas{image-rendering:pixelated;cursor:crosshair}pre{position:sticky;top:0;background:#222}</style><h1>Scalar castle proposal — fixed -40..160</h1><p>Offline base-ROM context; not exact-state/geometry approval. Missing local samples are not authored zero. Dark World empty/default zero.</p><select id="view"><option>candidate-height.png</option><option>baseline-height.png</option><option>placement-offset.png</option><option>artwork.png</option></select> Zoom <input id="zoom" type="range" min="1" max="8" value="1"><pre id="sample">Hover for current local + one tile scalar = final.</pre><canvas id="c" width="1024" height="1024"></canvas><script>const data=DATA;const c=document.querySelector('#c'),ctx=c.getContext('2d');function load(){const i=new Image();i.onload=()=>ctx.drawImage(i,0,0);i.src=view.value;}view.onchange=load;zoom.oninput=()=>{c.style.width=1024*zoom.value+'px';c.style.height=1024*zoom.value+'px';};c.onmousemove=e=>{const r=c.getBoundingClientRect(),x=Math.floor((e.clientX-r.left)*1024/r.width),y=Math.floor((e.clientY-r.top)*1024/r.height),b=data.offset[y>>3][x>>3],l=data.local[y][x];sample.textContent=`Light BG2 world tile (${192+(x>>3)},${192+(y>>3)}) pixel (${x%8},${y%8}): local ${data.presence[y][x]?l:'MISSING'} + scalar ${b} = ${l+b}`;};load();</script>'''.replace('DATA', values)
    (out/'preview.html').write_text(html)
    print(json.dumps(dict(placements=len(records),gaps=len(gaps),local_missing=int((~np.isfinite(local)).sum()),candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest())))


if __name__ == '__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True)
    generate(ap.parse_args().output)
