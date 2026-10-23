"""Short sequential ABBA production EndFrame stage probe. No parallel subprocesses."""
import argparse
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--captures', type=Path, required=True)
p.add_argument('--mode', type=int, choices=(0, 1, 2), required=True)
a = p.parse_args()
env = dict(os.environ, ALTTP_CACHE_BENCHMARK='30', ALTTP_CACHE_ONLY_MODE=str(a.mode),
           ALTTP_CACHE_GRAPHICS_SAMPLES=';'.join(str(a.captures / ('frame_room061_'+n+'.s9xrmf'))
                                                for n in ('018', '025', '026', '032', '041')))
records = []
for order, variant in enumerate(('pre', 'final', 'final', 'pre', 'stage1', 'final', 'final', 'stage1')):
    cmd = [str(a.output / ('cache-'+variant+'061')),
           str(a.captures / 'frame_room061_041.s9xrmf'), str(a.output / ('ab-'+variant))]
    text = subprocess.check_output(cmd, env=env, text=True)
    records.append(dict(order=order, variant=variant, command=cmd, stdout=text))
    print(variant, text, flush=True)
report = dict(environment={k: v for k, v in env.items() if k.startswith('ALTTP_CACHE')}, records=records)
if a.mode:
    report['complete_state_equality'] = all(
        (a.output / ('ab-pre-mode'+str(a.mode)+'-state'+str(i)+'.inventory')).read_bytes() ==
        (a.output / ('ab-final-mode'+str(a.mode)+'-state'+str(i)+'.inventory')).read_bytes() ==
        (a.output / ('ab-stage1-mode'+str(a.mode)+'-state'+str(i)+'.inventory')).read_bytes()
        for i in range(5))
    assert report['complete_state_equality']
(a.output / ('cache061-alternating-mode'+str(a.mode)+'.json')).write_text(json.dumps(report, indent=2)+'\n')
