"""Compile canonical SE classification only; preserve every authored array.

Output is a separate explicitly selected profile, never an immutable capture.
"""
import argparse
import json
from pathlib import Path
import re
import tomllib


def activate(text, data):
    source=tomllib.loads(text)
    from alttp_authored_obstacles import validate_groups
    _, grouped = validate_groups(source)
    if data['rom_sha256']!=source['game']['rom_sha256']:raise ValueError('ROM mismatch')
    annotations=data['tiles'];count=0
    def block(match):
        nonlocal count
        old=match.group();key=re.search(r'tile_hash\s*=\s*"([^"]+)"',old)[1]
        if key in grouped:return old # explicit footprint never reauthors intrinsic metadata
        annotation=annotations.get(key,{})
        if annotation.get('role')!='wall' or annotation.get('normals')!=['south_east'] or annotation.get('context_dependent'):return old
        parsed=tomllib.loads(old)['assets'][0]
        if parsed.get('height')==[0]*64:return old
        if parsed.get('tile_semantics_authored') and parsed.get('tile_semantic_normals')!=['south_east']:
            raise ValueError('conflicting approved profile semantics: '+key)
        if parsed.get('tile_role','wall') not in ('wall','unknown'):
            raise ValueError('conflicting explicit role: '+key)
        if 'height = [0, 0, 0, 0' in old:
            if parsed.get('height')==[0]*64:return old
        for field in ('tile_role','tile_semantics_authored','tile_semantic_normals','tile_semantic_context_dependent'):
            old=re.sub(r'(?m)^'+field+r'\s*=.*\n','',old)
        count+=1
        return old.rstrip()+'\ntile_role = "wall"\ntile_semantics_authored = true\ntile_semantic_normals = ["south_east"]\ntile_semantic_context_dependent = false\n\n'
    return re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',block,text),count

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--semantics',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--header',action='store_true')
    p.add_argument('--patch',action='store_true')
    a=p.parse_args();text=a.profile.read_text();source=tomllib.loads(text)
    data=json.loads(a.semantics.read_text())
    if a.patch:
        print('*** Begin Patch\n*** Update File: '+str(a.profile))
        for asset in source['assets']:
            key=asset['tile_hash'];annotation=data['tiles'].get(key,{})
            if annotation.get('role')!='wall' or annotation.get('normals')!=['south_east'] or annotation.get('context_dependent') or asset.get('height')==[0]*64 or asset.get('tile_semantics_authored'):continue
            print('@@\n tile_hash = "'+key+'"\n+tile_semantics_authored = true\n+tile_semantic_normals = ["south_east"]\n+tile_semantic_context_dependent = false')
            if asset.get('tile_role') is None:print('+tile_role = "wall"')
        print('*** End Patch')
        return
    if a.header:
        ids=sorted(k for k,v in data['tiles'].items() if v.get('role')=='wall' and
                   v.get('normals')==['south_east'] and not v.get('context_dependent'))
        print('\n'.join('UINT64_C(0x'+k.split(':')[-1]+'),' for k in ids))
        return
    if data['rom_sha256']!=source['game']['rom_sha256']:raise ValueError('ROM mismatch')
    text,count=activate(text,data)
    a.output.write_text(text)
    print('activated existing SE assets',count,'arrays unchanged; explicit research profile',a.output)


if __name__=='__main__':main()
