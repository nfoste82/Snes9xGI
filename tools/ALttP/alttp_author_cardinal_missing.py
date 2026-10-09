#!/usr/bin/env python3
"""User-approved missing-only EAST/SOUTH intrinsic walls; never rewrite arrays.

Appends absent canonical assets only. Ambiguous/multifacing/context-dependent
usages stay explicit. This authoring operation is not placement height solving.
"""
import argparse,hashlib,json,tomllib,re
from pathlib import Path
from collections import Counter

def proposal(profile,semantics):
 assets={a['tile_hash']:a for a in profile.get('assets',[])};rows=[];add=[]
 for h,s in sorted(semantics['tiles'].items()):
  faces=s.get('normals',[])
  if s.get('role') not in ('wall','wall_corner') or not set(faces)&{'east','south'}:continue
  a=assets.get(h);status='preserved_existing_height' if a and 'height' in a else 'missing'
  if status=='missing':
   if s.get('context_dependent') or s['role']!='wall' or faces not in (['east'],['south']):status='unresolved_multiple_facings_or_context'
   elif a:status='existing_asset_missing_requires_surgical_field_patch'
   else:
    status='author_missing_cardinal';face=faces[0]
    add.append(dict(tile_hash=h,height=[7-x if face=='east' else 7-y for y in range(8) for x in range(8)],
                    tile_role='wall',tile_semantics_authored=True,tile_semantic_normals=[face],tile_semantic_context_dependent=False))
  rows.append(dict(tile_hash=h,role=s['role'],facings=faces,status=status))
 return rows,add

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',type=Path,required=True)
 p.add_argument('--semantics',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--apply',action='store_true')
 a=p.parse_args();before=a.profile.read_bytes();profile=tomllib.loads(before.decode());sem=json.loads(a.semantics.read_text())
 rows,add=proposal(profile,sem);text=before.decode()
 activated=[];conflicts=[]
 def activate(match):
  old=match.group();parsed=tomllib.loads(old)['assets'][0];h=parsed['tile_hash'];s=sem['tiles'].get(h,{})
  faces=s.get('normals',[])
  if s.get('context_dependent') or s.get('role') not in ('wall','wall_corner') or not set(faces)&{'east','south'}:return old
  if s['role']=='wall' and faces not in (['east'],['south']):return old
  if s['role']=='wall_corner' and (set(faces)!={'east','south'} or s.get('corner_type') not in ('inside','outside')):return old
  if parsed.get('tile_role',s['role']) not in (s['role'],'unknown') or (parsed.get('tile_semantics_authored') and parsed.get('tile_semantic_normals')!=faces):
   conflicts.append(h);return old
  fields=dict(tile_role=s['role'],tile_semantics_authored=True,tile_semantic_normals=faces,tile_semantic_context_dependent=False)
  if s['role']=='wall_corner':fields['tile_semantic_corner']=s['corner_type']
  missing={k:v for k,v in fields.items() if k not in parsed}
  if missing:
   activated.append(h)
   insert=''.join(k+' = '+json.dumps(v)+'\n' for k,v in missing.items())
   old=re.sub(r'(?m)^(tile_hash\s*=.*\n)',lambda m:m.group()+insert,old,count=1)
  return old
 text=re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',activate,text)
 for asset in add:
  text+='\n[[assets]]\ntile_hash = '+json.dumps(asset['tile_hash'])+'\nheight = '+json.dumps(asset['height'])+'\n'
  text+='tile_role = "wall"\ntile_semantics_authored = true\ntile_semantic_normals = '+json.dumps(asset['tile_semantic_normals'])+'\ntile_semantic_context_dependent = false\n'
 after=tomllib.loads(text);old={x['tile_hash']:x for x in profile['assets']};new={x['tile_hash']:x for x in after['assets']}
 activation_fields={'tile_role','tile_semantics_authored','tile_semantic_normals','tile_semantic_context_dependent','tile_semantic_corner'}
 assert all(all(new[h][k]==value for k,value in v.items()) for h,v in old.items())
 assert all(set(new[h])-set(v)<=activation_fields for h,v in old.items())
 assert all(after[k]==v for k,v in profile.items() if k!='assets')
 assert a.profile.read_bytes()==before,'shared profile changed during authoring; refuse overwrite'
 if a.apply:a.profile.write_text(text)
 finalrows,_=proposal(after if a.apply else profile,sem)
 a.report.write_text(json.dumps(dict(rule='east7-x/south7-y; missing-only unambiguous walls',apply=a.apply,
      before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=hashlib.sha256(text.encode()).hexdigest(),
      denominator=len(rows),before_counts=dict(Counter(r['status'] for r in rows)),after_counts=dict(Counter(r['status'] for r in finalrows)),
      added=add,activated=activated,activation_conflicts=conflicts,rows=finalrows,preserved_asset_denominator=len(old),existing_arrays_changed=0,approval=False),indent=2)+'\n')
 print('denominator',len(rows),'added',len(add),'counts',dict(Counter(r['status'] for r in finalrows)))
if __name__=='__main__':main()
