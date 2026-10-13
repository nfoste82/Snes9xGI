"""Summarize complete-own-profile before/after audits, without room approval."""
import argparse
import json
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('before',type=Path);p.add_argument('after',type=Path);p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
def load(path):
    records=json.loads((path/'emitter-source-audit.json').read_text())['results']
    for r in records:
        if r['status']!='audited':continue
        v=json.loads(Path(str(path/Path(r['capture']).name)+'.json').read_text())
        r['structural_conflicts']=sum(x['conflicts'] for x in v['layers'])
        r['unknown_walls']=sum(x['unknown_walls'] for x in v['layers'])
        r['missing_support']=sum(sum(x['missing']) for x in v['layers'])
    return records
b,c=load(a.before),load(a.after)
keys=('emitting','unknown','unknown_walls','structural_conflicts','missing_support')
result={}
for name,records in (('before',b),('after',c)):
    result[name]={k:sum(r.get(k,0) for r in records if r['status']=='audited') for k in keys}
result['changed']=[dict(capture=x['capture'],before={k:x[k] for k in keys},after={k:y[k] for k in keys})
                   for x,y in zip(b,c) if x['status']==y['status']=='audited' and any(x[k]!=y[k] for k in keys)]
result['approval']=False
a.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
