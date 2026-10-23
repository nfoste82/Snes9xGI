"""Run complete production-cache differential inventory; synthetic animation only."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess

p = argparse.ArgumentParser()
for name in ('corpus', 'profile', 'rom', 'probe', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
records = []
for raw in sorted(a.corpus.glob('room-*-entrance-*.raw')):
    state = json.loads(Path(str(raw) + '.state.json').read_text())
    args = [str(raw), str(raw) + '.attr', str(a.profile), str(a.rom),
            *map(str, state['link_xy']), str(state['plane'])]
    record = json.loads(subprocess.check_output([str(a.probe), *args]))
    record['snapshot'] = str(raw)
    records.append(record)
reused = [r for r in records if r['reused']]
report = dict(contexts=len(records), rooms=len({r['room'] for r in records}),
              synthetic_animation=True, complete_equivalence=True,
              reused_contexts=len(reused), fallback_contexts=len(records)-len(reused),
              profile_sha256=hashlib.sha256(a.profile.read_bytes()).hexdigest(),
              rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(), records=records)
if reused:
    report['reused_median_ms'] = statistics.median(r['reuse_ms'] for r in reused)
    report['full_median_ms'] = statistics.median(r['full_ms'] for r in reused)
a.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if k != 'records'}, indent=2))
