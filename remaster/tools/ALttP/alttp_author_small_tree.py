"""Review proposal for the explicit 4x4_tree_light_world group, peak local10.

Inputs are pinned profile, capture-extracted indices and placements. Palette
indices segment this inspected sprite's parts, never map brightness to height.
No coordinate terrain channels or unrelated asset data are modified.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import tomllib
import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import gaussian_filter


def run(source, assets_path, placements_path, artwork, output):
    raw=source.read_text();p=tomllib.loads(raw)
    group=next(g for g in p['obstacle_groups'] if g['name']=='4x4_tree_light_world')
    members=group['members'];keys={m['tile_hash'] for m in members}
    assets={a['hash']:np.array(a['indices']).reshape(8,8) for a in json.loads(assets_path.read_text())}
    indices=np.zeros((32,32),dtype=np.uint8)
    for m in members:
        a=assets[m['tile_hash']]
        if m['hflip']:a=a[:,::-1]
        if m['vflip']:a=a[::-1]
        indices[m['y']*8:m['y']*8+8,m['x']*8:m['x']*8+8]=a
    y,x=np.mgrid[:32,:32];crown=np.zeros((32,32),bool)
    # Traced crown outline: include its dark rim, exclude brown roots/shadow.
    for row in range(24):
        ink=np.flatnonzero(np.isin(indices[row],[14,15]))
        if len(ink):crown[row,max(0,ink[0]-1):min(32,ink[-1]+2)]=True
    shadow=indices==13
    root=(indices!=0)&~shadow&~crown
    u=(x-15.5)/13.5;v=(y-11.5)/12.5
    radius=u*u+v*v;dome=np.sqrt(np.maximum(0,1-radius))
    heights=np.zeros((32,32),float)
    heights[crown]=3+7*dome[crown]
    # Separate exposed trunk/root relief, tapering to ground at the bottom.
    root_center=np.where(x<10,6.,np.where(x>21,25.,15.5))
    root_radius=np.where((x<10)|(x>21),4.,6.)
    rr=np.clip((x-root_center)/root_radius,-1,1)
    root_height=3*np.sqrt(np.maximum(0,1-rr*rr))*np.clip((31-y)/10,0,1)
    heights[root]=root_height[root]
    h=np.rint(heights).astype('int16');n=np.zeros((32,32,3),float);n[...,2]=1
    # Smooth analytic ellipsoid normals, independent of quantized height bands.
    # No global +Z clamp: crown rim and trunk sides can be near vertical.
    z=np.sqrt(np.maximum(0,1-radius))
    cn=np.stack((u/13.5,v/12.5,z/7),axis=-1)
    cn/=np.maximum(np.linalg.norm(cn,axis=-1,keepdims=True),1e-12)
    n[crown]=cn[crown]
    # Roots spread across the ground as low tapered buttresses, not parallel
    # upright cylinders. Differentiate the continuous (unquantized) low relief
    # within its own part mask, matching the approved large-tree root treatment.
    weight=gaussian_filter(root.astype(float),1.1)
    smooth=gaussian_filter(root_height*root,1.1)/np.maximum(weight,1e-9)
    gy,gx=np.gradient(smooth)
    # The exposed outer tips flatten into the supporting ground. Attenuate
    # gradients by distance along each root; do not clamp all object normals.
    progress=np.clip((y-19)/12,0,1)
    flatten=1-progress*progress*(3-2*progress)
    rn=np.stack((-gx*flatten,-gy*flatten,np.ones_like(rr)),axis=-1)
    rn/=np.linalg.norm(rn,axis=-1,keepdims=True);n[root]=rn[root]
    additions={}
    for m in members:
        sl=np.s_[m['y']*8:m['y']*8+8,m['x']*8:m['x']*8+8]
        a=h[sl].copy();b=n[sl].copy()
        if m['vflip']:a=a[::-1];b=b[::-1];b[...,1]*=-1
        if m['hflip']:a=a[:,::-1];b=b[:,::-1];b[...,0]*=-1
        b=np.rint((b+1)*127.5).clip(0,255).astype('uint8')
        value={'height':a.ravel().tolist(),'height_sampling':'linear','normal_xyz':b.ravel().tolist()}
        if m['tile_hash'] in additions:assert additions[m['tile_hash']]==value,'Mirrored hash conflict'
        additions[m['tile_hash']]=value
    def edit(match):
        block=match[0];a=tomllib.loads(block)['assets'][0];k=a['tile_hash']
        if k not in additions:return block
        if a.get('geometry_by_flip') or a.get('geometry_by_facing') or a.get('normal_ignore_tile_transform'):raise ValueError('Unsupported variant/transform')
        block=re.sub(r'^(height|height_sampling|normal_xyz)\s*=.*\n?','',block,flags=re.M)
        return block.rstrip()+'\n'+''.join(k+' = '+json.dumps(v,separators=(',',':'))+'\n' for k,v in additions[k].items())+'\n'
    text=re.sub(r'^\[\[assets\]\][\s\S]*?(?=^\[|\Z)',edit,raw,flags=re.M)
    result=tomllib.loads(text)
    for k in p.keys()|result.keys():
        if k!='assets':assert p.get(k)==result.get(k),k
    for old,new in zip(p['assets'],result['assets']):
        if old['tile_hash'] not in keys:assert old==new
        else:assert {k:v for k,v in old.items() if k not in ('height','height_sampling','normal_xyz')}=={k:v for k,v in new.items() if k not in ('height','height_sampling','normal_xyz')}
    assert h.max()==10 and np.all(h[~(root|crown)]==0)
    # Reconstruct serialized canonical arrays through actual member flips.
    decoded=np.zeros_like(n)
    for m in members:
        a=np.array(additions[m['tile_hash']]['normal_xyz']).reshape(8,8,3)/127.5-1
        if m['hflip']:a=a[:,::-1];a[...,0]*=-1
        if m['vflip']:a=a[::-1];a[...,1]*=-1
        a/=np.linalg.norm(a,axis=-1,keepdims=True)
        decoded[m['y']*8:m['y']*8+8,m['x']*8:m['x']*8+8]=a
    placements=json.loads(placements_path.read_text());origins=set()
    for r in placements:
        for m in members:
            if r['hash']==m['tile_hash'] and bool(r['hflip'])==m['hflip'] and bool(r['vflip'])==m['vflip']:
                ox,oy=r['x']-8*m['x'],r['y']-8*m['y']
                if all(any(t['hash']==q['tile_hash'] and t['x']==ox+q['x']*8 and t['y']==oy+q['y']*8 and bool(t['hflip'])==q['hflip'] and bool(t['vflip'])==q['vflip'] for t in placements) for q in members):origins.add((ox,oy))
    assert len(origins)==4,origins
    output.mkdir(parents=True,exist_ok=False);(output/'source.toml').write_text(raw)
    dest=output/'alttp-small-tree-proposed.toml';dest.write_text(text)
    art=Image.open(artwork).convert('RGB');art=Image.fromarray(np.array(art)[...,::-1]) # PPM exporter emitted BGR
    origin=next(o for o in sorted(origins) if o[0]>=0);crop=art.crop((*origin,origin[0]+32,origin[1]+32))
    panels=[crop,Image.fromarray(np.repeat((h*25).astype('uint8')[...,None],3,axis=2)),Image.fromarray(np.rint((decoded+1)*127.5).clip(0,255).astype('uint8'))]
    review=Image.new('RGB',(768,300),'#181d23');draw=ImageDraw.Draw(review)
    for j,(label,im) in enumerate(zip(('Artwork','Local height 0..10','Normals (+X right, +Y down, +Z viewer)'),panels)):
        review.paste(im.resize((256,256),Image.Resampling.NEAREST),(j*256,32));draw.text((j*256+4,8),label,fill='white')
    review.save(output/'review.png')
    lighting=Image.new('RGB',(768,544),'#181d23');draw=ImageDraw.Draw(lighting)
    for j,direction in enumerate(((-.8,-.5,1),(.8,-.5,1),(0,.8,.25))):
        light=np.array(direction);light/=np.linalg.norm(light);shade=.12+.88*np.maximum(0,decoded@light)
        for row,albedo in enumerate((np.full((32,32,3),[180,190,180]),np.array(crop))):
            rgb=np.rint(shade[...,None]*albedo).astype('uint8');rgb[~(root|crown)]=[28,32,28]
            lighting.paste(Image.fromarray(rgb).resize((256,256),Image.Resampling.NEAREST),(j*256,32+row*256))
        draw.text((j*256+4,8),('Left','Right','Low front')[j],fill='white')
    lighting.save(output/'lighting.png');art.resize((768,672),Image.Resampling.NEAREST).save(output/'capture.png')
    np.savez_compressed(output/'fields.npz',height=h,normal=decoded,crown=crown,roots=root,shadow=shadow,indices=indices)
    report=dict(group=group['name'],canonical_tiles=len(additions),members=len(members),capture_instances=sorted(origins),peak_local_height=int(h.max()),source_sha256=hashlib.sha256(raw.encode()).hexdigest(),proposal_sha256=hashlib.sha256(text.encode()).hexdigest(),preserved='All other assets, metadata, groups and placement/terrain offsets',references='Approved large-tree smooth normals; corrected column surface separation; no numerical arrays copied',assumptions=['Compact ellipsoidal crown peaks at10 above its support; rim at3','Brown exposed roots/trunk use separate low rounded relief; index13 painted shadow remains flat','No brightness-to-height or terrain normal offsets'],limitations=['Analytic crown is a shape interpretation, not leaf-by-leaf geometry','No live GPU shadows validated','Canonical unchanged; proposal only'])
    (output/'REPORT.json').write_text(json.dumps(report,indent=2))
    (output/'review.html').write_text('<!doctype html><meta charset="utf-8"><style>body{background:#181d23;color:white;font:17px sans-serif}img{max-width:100%;image-rendering:pixelated}</style><h1>Small Light World tree — local peak 10</h1><img src="review.png"><h2>Neutral / artwork lighting: left, right, low-front</h2><img src="lighting.png"><h2>Four matched examples</h2><img src="capture.png"><p>Local geometry only; placement offsets preserved. Canonical unchanged. See REPORT.json for assumptions.</p>')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--assets',type=Path,required=True);p.add_argument('--placements',type=Path,required=True);p.add_argument('--artwork',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(a.source,a.assets,a.placements,a.artwork,a.output)
