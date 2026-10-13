"""Print bounded complete-room structural inventory for a pinned replay."""
import argparse
import json

p=argparse.ArgumentParser()
p.add_argument('runtime')
p.add_argument('--x',type=int,default=48)
p.add_argument('--y',type=int,default=0)
p.add_argument('--width',type=int,default=14)
p.add_argument('--height',type=int,default=12)
a=p.parse_args()
r=json.load(open(a.runtime))
unresolved={tuple(v['xy']):v for v in r['unresolved_wall_placements']}
for v in r['directional_geometry']:
    if v['layer']!=1 or not(a.x<=v['x']<a.x+a.width and a.y<=v['y']<a.y+a.height):continue
    u=unresolved.get((v['x'],v['y']),{})
    print(v['plane'],v['x'],v['y'],'role',v['role'],'base',v['base'],'local',v['has_local'],
          'unresolved', {k:value for k,value in u.items() if k!='neighbors'})
