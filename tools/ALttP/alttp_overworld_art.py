#!/usr/bin/env python3
"""ROM-derived index-color overview and labelled metatile contact sheet.
Palette here is diagnostic index color, NOT game RGB or classification input.
"""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw
from alttp_atlas import Rom
from alttp_atlas_graphics import extract_graphics
from alttp_atlas_overworld import extract_overworld
from alttp_atlas_overworld_gfx import BaseOverworldGraphics
from alttp_atlas_world import catalog_world

def generate(rom, area, prefix, candidate=None):
    qs = extract_overworld(rom)
    graphics = extract_graphics(rom)
    resolver = BaseOverworldGraphics(graphics, catalog_world(rom)['overworld_areas'], qs)
    by_hash = {g.content_id: g.indices for g in graphics}
    ids = resolver.area_quadrants(area)
    minx = min(i & 7 for i in ids)*512; miny = min((i & 63)//8 for i in ids)*512
    image = Image.new('RGB', (1024,1024), '#ff00ff')
    metatiles = {}
    for qi in ids:
        for t in qs[qi].iter_tiles():
            g = resolver.resolve_tile(area,t)
            indices = by_hash.get(g.content_id, bytes(64))
            # Each palette gets a distinct diagnostic hue; brightness is index.
            colors = [(int(v*17),int(v*17),int(v*17)) for v in range(16)]
            tile = Image.new('RGB',(8,8)); tile.putdata([colors[v] for v in indices])
            if t.flip_x: tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if t.flip_y: tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            image.paste(tile,((qi & 7)*512-minx+t.x*8,((qi & 63)//8)*512-miny+t.y*8))
            if t.map16_id not in metatiles: metatiles[t.map16_id] = Image.new('RGB',(16,16))
            metatiles[t.map16_id].paste(tile,((t.x&1)*8,(t.y&1)*8))
    image.save(str(prefix)+'-overview.png')
    if candidate:
        c=json.loads(candidate.read_text()); overlay=image.copy(); heights=image.copy()
        d=ImageDraw.Draw(overlay); hd=ImageDraw.Draw(heights)
        colors={'wall':'#388bff','cap':'#ffbc40','floor':'#4ab060','obstacle':'#d75fe0'}
        for p in c['placements']:
            x=p['x']*8-minx; y=p['y']*8-miny
            if p['role']!='unknown':
                color=colors.get(p['role'],'white') if p['status']=='solved' else '#e34949'
                d.rectangle((x,y,x+7,y+7),fill=color)
            if p['status']=='solved':
                for py in range(8):
                    for px in range(8):
                        v=max(0,min(255,32+3*(p['base']+p['local'][py*8+px])))
                        hd.point((x+px,y+py),fill=(v,v,v))
        Image.blend(image,overlay,.6).save(str(prefix)+'-roles.png')
        heights.save(str(prefix)+'-heights.png')
    # Labelled structural inspection in four readable quadrant sheets.
    for qi in ids:
        ox=(qi&7)*512-minx; oy=((qi&63)//8)*512-miny
        annotated=image.crop((ox,oy,ox+512,oy+512)).resize((1536,1536),Image.Resampling.NEAREST)
        d=ImageDraw.Draw(annotated)
        for yy in range(32):
            for xx in range(32):
                mid=qs[qi].map16_ids[yy*32+xx]
                d.text((xx*48,yy*48),str(mid),fill='#ff5050',stroke_width=1,stroke_fill='black')
        annotated.save(str(prefix)+f'-q{qi:02x}-ids.png')
    mids = sorted(metatiles)
    sheet = Image.new('RGB',(16*72,((len(mids)+15)//16)*90),'#303030')
    draw = ImageDraw.Draw(sheet)
    for n,mid in enumerate(mids):
        x,y = (n%16)*72,(n//16)*90
        sheet.paste(metatiles[mid].resize((64,64),Image.Resampling.NEAREST),(x,y))
        attrs = [rom.get_byte(0x8e9459+(rom.get_word(0x8f8000+mid*8+i*2)&511)) for i in range(4)]
        draw.text((x,y+64),str(mid),fill='white')
        draw.text((x,y+76),''.join(f'{a:02x}' for a in attrs),fill='white')
    sheet.save(str(prefix)+'-metatiles.png')

if __name__ == '__main__':
    p=argparse.ArgumentParser(); p.add_argument('--rom',type=Path,default=Path.home()/'Downloads/Zelda_ALTTP.zip')
    p.add_argument('--area',type=lambda s:int(s,0),default=27); p.add_argument('--prefix',type=Path,required=True)
    p.add_argument('--candidate',type=Path)
    a=p.parse_args(); generate(Rom(a.rom),a.area,a.prefix,a.candidate)
