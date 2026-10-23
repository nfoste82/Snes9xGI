"""Summarize immutable before/after seven-identity review and full gate blockers."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,required=True)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    before=json.loads(a.before.with_suffix('.json').read_text());after=json.loads(a.after.with_suffix('.json').read_text())
    gate=json.loads((a.output/'validation-v2.json').read_text());samples=[]
    for y in range(41,48):
        i=y*64+11;b=before['layers'][1]['cells'][i];c=after['layers'][1]['cells'][i]
        bh=before['layers'][1]['local_samples'][i]['height'];ch=after['layers'][1]['local_samples'][i]['height']
        samples.append(dict(xy=[11,y],hash=c['hash'],flips=c['flips'],before_base=b['base'],after_base=c['base'],
                            before_canonical=bh,after_canonical=ch,
                            after_effective=[v+c['base'] for v in ch]))
    for prefix in (a.before,a.after):
        f=Path(str(prefix)+'-recomputed-composed-height.ppm')
        Image.open(f).resize((768,672),Image.Resampling.NEAREST).save(a.output/(prefix.name+'.png'))
    report=dict(verdict='reviewable but incomplete',samples=samples,
                pins={suffix:hashlib.sha256(Path(str(a.capture)+suffix).read_bytes()).hexdigest()
                      for suffix in ('','.profile.toml','.dungeon-input.bin','.dungeon-templates.bin','.dungeon.json')},
                coverage={k:gate[k] for k in ('valid_walkable_placements','required_walkable_placements',
                'per_plane_walkable_denominator','solved_wall_cells','unknown_wall_cells')},
                missing_placements=len(gate['missing_placements']),
                conflicts=len(gate['conflicting_components']),
                residuals=['BG1 unknown walls6','ceiling artwork60 versus cardinal shell48',
                           'arbitrary extents beyond emitted contiguous dispatches unsupported',
                           'exact-state movement parity and fresh gameplay comparison missing'],approval=False)
    (a.output/'variable-review.json').write_text(json.dumps(report,indent=2)+'\n')
    print('samples',[(r['xy'],r['hash'],r['before_base'],r['after_base'],min(r['after_effective']),max(r['after_effective'])) for r in samples])
    print('coverage',report['coverage'],'missing',report['missing_placements'])


if __name__=='__main__':main()
