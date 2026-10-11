"""Exact contact inventory for own050; never exports speculative heights."""
import json
import collections
from pathlib import Path
import argparse
p=argparse.ArgumentParser()
p.add_argument('prefix',type=Path)
a=p.parse_args()
tiles=json.loads(Path(str(a.prefix)+'-placements.json').read_text())
counter=collections.Counter()
contacts=[]
for t in tiles:
    if t['surface']!=2 or t['base']!=255: continue
    i=t['cell']; x=i%64; y=i//64
    for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
        if not(0<=x+dx<64 and 0<=y+dy<64):continue
        n=tiles[i+dx+64*dy]
        if n['surface']==2:continue
        def h(t,x,y):return t['samples'][(7-y if t['vflip'] else y)*8+(7-x if t['hflip'] else x)]
        downhill=sum(h(t,7 if dx>0 else 0 if dx<0 else k,7 if dy>0 else 0 if dy<0 else k) for k in range(8))<sum(h(t,0 if dx>0 else 7 if dx<0 else k,0 if dy>0 else 7 if dy<0 else k) for k in range(8))
        key=(n['surface'],n['attrs'][0],n['attrs'][1],tuple(n['span']),n['structural_floor_mask'],downhill)
        counter[key]+=1
        contacts.append(dict(wall=i,neighbor=n['cell'],downhill=downhill,neighbor_surface=n['surface'],neighbor_attributes=n['attrs'],neighbor_span=n['span'],neighbor_base=n['base'],word=n['word'],distinct=n['distinct']))
report=dict(classes=[dict(key=k,count=v) for k,v in counter.most_common()],contacts=contacts)
Path(str(a.prefix)+'-contacts.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['classes'],indent=2))
