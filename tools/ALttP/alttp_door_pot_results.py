import json
import sys
from pathlib import Path

before, after = (json.loads(Path(p).read_text()) for p in sys.argv[1:3])
for b, a in zip(before['layers'], after['layers']):
    changes = [(old, new) for old, new in zip(b['cells'], a['cells']) if old != new]
    print('BG', a['bg'], 'changes', len(changes), 'required', a['required_planes'], 'frames', a['doorframes'])
    for old, new in changes:
        print(old['xy'], 'base', old['base'], '->', new['base'], 'support', old['support'], '->', new['support'], 'role', old['role'], '->', new['role'])
    for old,new in zip(b['local_samples'],a['local_samples']):
        assert old['height']==new['height'], 'authored local changed'
