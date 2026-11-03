"""Review-only terrain deformation from pinned revised support contacts.

Uses the existing ROM ring reconstruction, not brightness-to-height. Solve only
ground placements; retain local relief and the user's wall scalar edits. Does
not publish canonical data or infer a world-wide monotonic elevation rule.
"""
from pathlib import Path
import argparse,json,tomllib,re,hashlib
import numpy as np
from scipy.ndimage import gaussian_filter, distance_transform_edt
from PIL import Image,ImageDraw

def run(bundle,source,output):
 output.mkdir(exist_ok=False,parents=True)
 raw=source.read_text();profile=tomllib.loads(raw)
 (output/'source.toml').write_text(raw)
 a=np.load(bundle/'assembly.npz');placements=json.loads((bundle/'placements.json').read_text())
 # Localized full assembly review including both ledges and connecting paths.
 x0,y0,w,h=0,1536,512,352;tx0,ty0=x0//8,y0//8;tw,th=w//8,h//8
 local=a['local'][y0-512:y0-512+h,x0:x0+w];art=a['art'][y0-512:y0-512+h,x0:x0+w]
 by={(r['x'],r['y']):r for r in placements};scalars={(r['x'],r['y']):r['offset'] for r in profile['overworld_offsets'] if r['world']=='light' and r['background']==1}
 base=np.array([[scalars.get((x+tx0,y+ty0),0) for x in range(tw)] for y in range(th)],float)
 ground=np.zeros((th,tw),bool)
 for y in range(th):
  for x in range(tw):
   r=by[x+tx0,y+ty0]
   ground[y,x]=r['role']=='floor' and (not r['height_present'] or r['local_max']<=4)
 # Preserve tree / shrub / roof / wall assemblies; metadata identity alone is
 # insufficient at mixed boundaries. Selected floor placements retain detail.
 mask=ground.repeat(8,0).repeat(8,1);contacts=0
 expanded=base.repeat(8,0).repeat(8,1);target=expanded.copy();fixed_contacts=[]
 # Carry the user's stepped ground support north BEFORE smoothing; do not
 # regularize the upper side of a ramp back down to the former zero datum.
 support_goal=np.maximum.accumulate(np.where(mask,np.clip(expanded,0,40),0)[::-1],axis=0)[::-1]
 support_goal[:1680-y0]=40
 # Assemble a continuous support proposal BEFORE differentiating. Flat authored
 # tile arrays are relief defaults, not constraints that terrain must be flat.
 support_goal=gaussian_filter(support_goal,12,mode='nearest')
 target[:]=support_goal
 # Blend continuously to the plateau constraints, with zero derivative at the
 # join. Assigning 40 only inside tile masks after filtering makes tile-edge
 # cliffs; the transition must be constructed in assembled pixel space.
 plateau=mask & ((np.indices(mask.shape)[0] < 1680-y0) | (expanded==40))
 # Smooth the distance to the terrace edge, not each tile separately. Expand
 # the constant region far enough to absorb raster stair-steps in the boundary.
 distance=np.maximum(0,gaussian_filter(distance_transform_edt(~plateau),8,mode='nearest')-12)
 blend=np.clip(distance/24.,0,1);blend=blend*blend*(3-2*blend)
 target=40-(40-target)*blend
 target[plateau]=40.
 target[mask]=np.clip(target[mask],0,40)
 assert np.all(target[plateau]==40), 'Northern plateau must remain exactly 40'
 # Lift complete non-wall object placements onto the continuing northern datum
 # without tilting their roofs/crowns or changing any local geometry arrays.
 scalar_edits={}
 for y in range(th):
  for x in range(tw):
   r=by[x+tx0,y+ty0]
   if ground[y,x] or r['role']=='wall':continue
   if y*8+y0<1680 or (320<=x*8<408 and 1656<=y*8+y0<1744):
    scalar_edits[x+tx0,y+ty0]=40;base[y,x]=40
 expanded=base.repeat(8,0).repeat(8,1)
 # Differentiate the ASSEMBLED continuous field, not 64 isolated 8×8 patches.
 # Missing ground neighbors use one-sided differences; cliff faces never
 # contaminate ground normals. This avoids tile-sized lighting blocks.
 gx=np.zeros_like(target);gy=np.zeros_like(target)
 for axis,out in ((1,gx),(0,gy)):
  plus=np.roll(target,-1,axis=axis);minus=np.roll(target,1,axis=axis)
  plus_ok=mask & np.roll(mask,-1,axis=axis);minus_ok=mask & np.roll(mask,1,axis=axis)
  if axis==1:plus_ok[:,-1]=False;minus_ok[:,0]=False
  else:plus_ok[-1]=False;minus_ok[0]=False
  both=plus_ok&minus_ok;out[both]=(plus[both]-minus[both])*.5
  forward=plus_ok&~minus_ok;out[forward]=plus[forward]-target[forward]
  back=minus_ok&~plus_ok;out[back]=target[back]-minus[back]
 support_normals=np.stack((-gx,-gy,np.ones_like(gx)),axis=2)
 support_normals/=np.linalg.norm(support_normals,axis=2)[:,:,None]
 # Record wall disagreements as diagnostics, not false zero residuals after
 # replacing the old wall-voted solve with the user-corrected terrace model.
 for y,x in zip(*np.where(mask)):
  for dy,dx in ((0,1),(0,-1),(1,0),(-1,0)):
   ny,nx=y+dy,x+dx
   if not(0<=ny<h and 0<=nx<w) or mask[ny,nx]:continue
   r=by[nx//8+tx0,ny//8+ty0]
   if r['role']=='wall' and r['palette']==2 and np.isfinite(local[ny,nx]):
    v=expanded[ny,nx]+local[ny,nx]-(local[y,x] if np.isfinite(local[y,x]) else 0)
    fixed_contacts.append((x,y,float(v)));contacts+=1
 records=[];normal_field=np.zeros((h,w,3),float);normal_field[:,:,2]=1;delta=np.zeros((h,w),float)
 for y in range(th):
  for x in range(tw):
   if not ground[y,x]:continue
   sl=np.s_[y*8:y*8+8,x*8:x*8+8];z=target[sl];adjust=z-base[y,x]
   n=support_normals[sl]
   if abs(adjust).max()<1e-10 and abs(n[:,:,:2]).max()<1e-10:continue
   records.append(dict(world='light',background=1,x=x+tx0,y=y+ty0,height_offset=np.round(adjust,6).ravel().tolist(),normal_offsets=np.round(n,9).ravel().tolist()))
   normal_field[sl]=n;delta[sl]=adjust
 # Preserve every non-offset authored field and every nonzero scalar. Eliminate
 # zero identity entries globally; they have no semantic effect.
 text=re.sub(r'^schema_version\s*=\s*\d+','schema_version = 39',raw,count=1)
 # Replace proposal terrain in this crop rather than appending duplicates or
 # adding a second deformation on top of the user's edited scalar reference.
 def keep_terrain(m):
  r=tomllib.loads(m[0])['overworld_terrain'][0]
  return '' if r['world']=='light' and r['background']==1 and tx0<=r['x']<tx0+tw and ty0<=r['y']<ty0+th else m[0]
 text=re.sub(r'^\[\[overworld_terrain\]\][\s\S]*?(?=^\[|\Z)',keep_terrain,text,flags=re.M)
 def keep(m):
  r=tomllib.loads(m[0])['overworld_offsets'][0]
  if r['world']=='light' and r['background']==1 and (r['x'],r['y']) in scalar_edits:
   return re.sub(r'^offset\s*=.*$',f"offset = {scalar_edits[r['x'],r['y']]}",m[0],flags=re.M)
  return m[0] if r['offset'] else ''
 text=re.sub(r'^\[\[overworld_offsets\]\][\s\S]*?(?=^\[|\Z)',keep,text,flags=re.M)
 for r in records:
  text+='\n[[overworld_terrain]]\n'+''.join(k+' = '+json.dumps(v,separators=(',',':'))+'\n' for k,v in r.items())
 dest=output/'alttp-kakariko-slope-proposed.toml';dest.write_text(text)
 final=local+expanded+delta;before=local+expanded
 def height(z):
  rgb=np.repeat(np.rint(np.clip((np.nan_to_num(z)+48)/128,0,1)*255).astype(np.uint8)[:,:,None],3,2);rgb[~np.isfinite(z)]=(220,0,220);return rgb
 panels=[('Artwork',art),('Revised scalar + local',height(before)),('Smooth support + local',height(final)),('Normal offset (+Z default)',np.rint((normal_field+1)*127.5).astype(np.uint8))]
 im=Image.new('RGB',(w*4,h+28),'#222');d=ImageDraw.Draw(im)
 for j,(label,pixels) in enumerate(panels):im.paste(Image.fromarray(pixels),(j*w,28));d.text((j*w+4,8),label,fill='white')
 im.save(output/'review.png')
 lights=[]
 for v in ((-.8,-.5,1),(.8,-.5,1),(0,.8,.25)):
  v=np.array(v);v/=np.linalg.norm(v);shade=.12+.88*np.maximum(0,(normal_field*v).sum(2));lights.append(np.rint(shade[:,:,None]*np.array([190,195,200])).astype(np.uint8))
 Image.fromarray(np.concatenate(lights,axis=1)).save(output/'neutral-support-lighting.png')
 # Verify exported (rounded) values, not merely the pre-export solver field.
 reconstructed=expanded.copy();exported_normals=np.zeros_like(normal_field);exported_normals[:,:,2]=1
 for r in records:
  xx=(r['x']-tx0)*8;yy=(r['y']-ty0)*8;sl=np.s_[yy:yy+8,xx:xx+8]
  reconstructed[sl]+=np.array(r['height_offset']).reshape(8,8)
  exported_normals[sl]=np.array(r['normal_offsets']).reshape(8,8,3)
 assert np.max(np.abs(reconstructed[plateau]-40))<1e-6
 assert np.max(np.abs(exported_normals[mask]-support_normals[mask]))<1e-8
 np.savez_compressed(output/'proposal.npz',local=local,scalar=base,adjustment=delta,normal=normal_field,ground_mask=mask,plateau_mask=plateau,support=reconstructed,final=final)
 (output/'plateau-validation.json').write_text(json.dumps(dict(plateau_pixels=int(plateau.sum()),minimum=float(reconstructed[plateau].min()),maximum=float(reconstructed[plateau].max()),normal_max_component_error=float(np.max(np.abs(exported_normals[mask]-support_normals[mask]))),normal_convention='normalize(-dH/dx,-dH/dy,1), assembled world-oriented support; local detail rotates separately'),indent=2))
 residual=[abs(float(target[y,x])-v) for x,y,v in fixed_contacts]
 report=dict(source=str(source),source_sha256=hashlib.sha256(raw.encode()).hexdigest(),proposal_sha256=hashlib.sha256(text.encode()).hexdigest(),world_rect=[x0,y0,w,h],terrain_placements=len(records),ground_pixels=int(mask.sum()),wall_contact_samples=contacts,max_contact_residual=max(residual,default=0),median_contact_residual=float(np.median(residual)) if residual else 0,scope='northwestern village terrace review; +40 plateau continues to north review boundary',normal_scope='per-pixel shortest-arc orientations from assembled support gradients',lifted_northern_object_placements=len(scalar_edits),limitations=['Missing local wall/ground layers remain missing','Northern terrace propagation ends at review boundary y1536, not a solved full-world continuation','Mixed object/ground tile supports require additional contact review','Neutral preview shows support only; detailed production normals are validated separately'])
 (output/'REPORT.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--bundle',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(a.bundle,a.source,a.output)
