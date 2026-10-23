"""Pinned complete inputs and mandatory v2 diagnostics; missing traversal stays explicit."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
for n in ('captures','output','solver','rom','fixtures'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();assert a.output.is_dir()
records=[]
for name in ('frame_room060_085.s9xrmf','frame_room071_001.s9xrmf','frame_room071_007.s9xrmf','frame_room071_008.s9xrmf'):
    capture=a.captures/name;out=a.output/capture.stem;out.mkdir(exist_ok=True)
    subprocess.run([sys.executable,'-B','remaster/tools/ALttP/alttp_replay_live_height_evidence.py',str(capture),'--solver',str(a.solver),'--rom',str(a.rom),'--output',str(out)],check=True)
    # No room071 observed stair trace exists. This deliberately withholds all
    # unmatched edges; never imports another room's traversal or calls it complete.
    state=json.loads(Path(str(capture)+'.dungeon.json').read_text())
    fixture=a.fixtures/f"room-{state['room']:03x}-doorway-graph.json"
    if not fixture.exists():
        fixture=out/'missing-directed-traversal.json'
        fixture.write_text(json.dumps(dict(attribute_sha256=None,graph=dict(stair_edges=[]))))
    subprocess.run([sys.executable,'-B','remaster/tools/ALttP/alttp_capture_complete_gate.py','--capture',str(capture),'--output',str(out),'--rom',str(a.rom),'--fixture',str(fixture)],check=True)
    d=json.loads((out/'gate.json').read_text())
    records.append(dict(capture=name,validator_exit=d['exit'],denominators=d['reachable_denominators'],complete_directed_graph=d['exact_collision'],approval=False))
(a.output/'gates-summary.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(records,indent=2))
