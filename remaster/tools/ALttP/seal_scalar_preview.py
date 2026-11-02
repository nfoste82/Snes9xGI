"""Verify preservation and pin the completed isolated preview (never builds)."""
import json
from pathlib import Path
import subprocess
import sys
from pin_overworld_inputs import digest, tree

out = Path(sys.argv[1])
startup = json.loads((out/'startup-pins.json').read_text())
canonical = Path('remaster/profiles/ALttP/alttp-profile.toml')
ordinary = tree(Path('build/release/Snes9x.app'))
assert startup['canonical_sha256'] == digest(canonical), 'Canonical changed; report without overwriting authoring'
assert ordinary == startup['ordinary_release'], 'Ordinary Release bundle changed'
paths = subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard'],text=True).splitlines()
sources = {p:digest(Path(p)) for p in paths if Path(p).is_file()}
product = tree(Path('build/overworld-height-preview/Snes9x.app'))
capture = Path.home()/'Library/Application Support/Snes9x/Remaster/frame_overworld_x08b0_y06cc.s9xrmf'
pins = dict(head=startup['head'], canonical_sha256=digest(canonical),ordinary_release_unchanged=True,
            ordinary_release_entries=len(ordinary),sources=sources,preview_bundle=product,
            candidate_sha256=digest(out/'alttp-scalar-preview.toml'),
            inputs={str(p):digest(p) for p in [capture,Path(str(capture)+'.overworld-input.bin'),Path(str(capture)+'.overworld.json')]},
            artifacts={str(p.relative_to(out)):digest(p) for p in out.iterdir() if p.is_file() and p.name!='final-pins.json'})
(out/'final-pins.json').write_text(json.dumps(pins,indent=2)+'\n')
(out/'implementation.diff').write_bytes(subprocess.check_output(['git','diff']))
print('SEALED canonical and all ordinary Release bundle entries unchanged; sources/product/artifacts pinned')
