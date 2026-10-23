"""Emit remaining-rule authoring patch and independently verify preservation."""
import argparse,json,hashlib,re,tomllib,difflib
from pathlib import Path
from alttp_diagonal_remaining_rules import remaining_proposals
from alttp_diagonal_sets import proposal_patch

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('inventory','discovery','profile','semantics','output'):p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--verify',action='store_true');a=p.parse_args()
 text=a.profile.read_text();assets={v['tile_hash']:v for v in tomllib.loads(text)['assets']}
 inv=json.loads(a.inventory.read_text());discovery=json.loads(a.discovery.read_text())
 sem=json.loads(a.semantics.read_text())['tiles']
 if a.verify:
  plan=json.loads((a.output/'remaining-plan.json').read_text());updates=plan['updates']
  for key,fields in updates.items():assert all(assets[key][f]==v for f,v in fields.items()),key
  def strip(m):
   key=re.search(r'(?m)^tile_hash\s*=\s*"([^"]+)"',m.group())
   return '' if key and key[1] in updates else m.group()
  assert re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',strip,text)==(a.output/'remaining-before-profile.toml').read_text()
  # Native profile grammar is intentionally line-oriented, unlike tomllib.
  fixed=text
  for key,fields in updates.items():
   block=next(m.group() for m in re.finditer(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)',fixed) if 'tile_hash = "'+key+'"' in m.group())
   new=block
   for field,value in fields.items():
    new=re.sub(r'(?ms)^'+field+r'\s*=\s*\[.*?\]',field+' = ['+', '.join(map(str,value))+']',new)
   fixed=fixed.replace(block,new,1)
  if fixed!=text:
   (a.output/'remaining-native-lines.diff').write_text(''.join(difflib.unified_diff(text.splitlines(True),fixed.splitlines(True),fromfile='a/remaster/profiles/ALttP/alttp-profile.toml',tofile='b/remaster/profiles/ALttP/alttp-profile.toml')))
   print('Native line formatting patch emitted; apply then rerun strict parser')
  print('PASS exact arrays and unrelated-byte preservation',len(updates));return
 updates,proofs=remaining_proposals(inv['entries'],discovery,assets,sem)
 (a.output/'remaining-plan.json').write_text(json.dumps(dict(updates=updates,proofs=proofs,
   profile_sha256=hashlib.sha256(text.encode()).hexdigest()),indent=2)+'\n')
 (a.output/'remaining-before-profile.toml').write_text(text)
 (a.output/'remaining-authoring.patch').write_text('*** Begin Patch\n*** Update File: remaster/profiles/ALttP/alttp-profile.toml\n'+''.join(proposal_patch(text,updates))+'*** End Patch\n')
 print([(k,proofs[k]['rule']) for k in updates])

if __name__=='__main__':main()
