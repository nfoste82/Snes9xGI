import argparse
import json
import collections
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('route',type=Path);a=p.parse_args()
r=[json.loads(v) for v in a.route.read_text().splitlines()]
trans=[v for v in r if v.get('submodule')==2]
print('rooms',collections.Counter(v.get('room') for v in r),'transition frames',len(trans))
for key in sorted({(v['room'],v['previous_room'],v['direction']) for v in trans}):
    s=[v for v in trans if (v['room'],v['previous_room'],v['direction'])==key]
    print(key,'frames',len(s),'start',s[0],'end',s[-1], 'origins',collections.Counter(tuple(v['destination_origin']) for v in s if v['phase']>=2))
