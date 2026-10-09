"""Own-input 060/061 production packets: cache enabled versus forced-full."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
for name in ('captures', 'enabled', 'full', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
records = []
suffixes = ('.dungeon-input.bin', '.dungeon-templates.bin', '.dungeon.json', '.profile.toml')
for capture in sorted(a.captures.glob('frame_room06[01]*.s9xrmf')):
    missing = [s for s in suffixes if not Path(str(capture)+s).is_file()]
    record = dict(capture=str(capture), missing=missing)
    records.append(record)
    if missing:
        record['status'] = 'incomplete-input'
        continue
    record['pins'] = {s or 'packet': hashlib.sha256(Path(str(capture)+s).read_bytes()).hexdigest()
                      for s in ('', *suffixes)}
    terrain = Path(str(capture)+'.terrain-input.bin')
    if terrain.exists():
        record['pins']['.terrain-input.bin'] = hashlib.sha256(terrain.read_bytes()).hexdigest()
    prefixes = []
    for mode, binary in (('enabled', a.enabled), ('full', a.full)):
        prefix = a.output / (capture.stem+'-'+mode)
        prefixes.append(prefix)
        result = subprocess.run([str(binary), str(capture), str(prefix)], capture_output=True, text=True)
        record[mode] = dict(exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
    if any(record[mode]['exit'] for mode in ('enabled', 'full')):
        record['status'] = 'rejected-evidence'
        continue
    for suffix in ('.s9xrmf', '.inventory', '.json'):
        before, after = [Path(str(prefix)+suffix).read_bytes() for prefix in prefixes]
        assert before == after, (capture, suffix)
        record[suffix+'_equal'] = True
    record['status'] = 'exact-equivalent'
    record['result'] = json.loads(Path(str(prefixes[0])+'.json').read_text())
report = dict(records=records, total=len(records),
              equivalent=sum(r['status']=='exact-equivalent' for r in records),
              incomplete=sum(r['status']=='incomplete-input' for r in records),
              rejected=sum(r['status']=='rejected-evidence' for r in records))
(a.output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({k: v for k, v in report.items() if k!='records'}, indent=2))
