#!/usr/bin/env python3
"""Pin and audit immutable diagonal replay, including complete-room v2.

Requires an existing output directory and replayed live-runtime.json/raw.
The graph is independently reconstructed from the observed Link seed; special
stair playback remains unavailable for this exact live state (not approval).
"""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
from PIL import Image
from alttp_diagonal_preview import diagonal_previews
from alttp_room_geometry import RoomSnapshot
import tomllib

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--rom',type=Path,required=True)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--companion-preview',type=Path,action='append',default=[],
                   help='additional before/after probe output prefix; images only, not approval')
    p.add_argument('--complete-graph',type=Path,required=True,
                   help='complete doorway/stair graph; collision SHA must exactly match live arrays')
    a=p.parse_args();out=a.output
    if not out.is_dir():p.error('output must exist')
    state=json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    raw=out/'live-solver-input.raw';attr=Path(str(raw)+'.attr')
    pin=dict(room_id=state['room'],plane=state['plane'],collision_mode=0,
             link_xy=state['link_xy'],live_capture=str(a.capture),
             missing_transition_playback=True)
    graph=json.loads(a.complete_graph.read_text())
    if graph.get('attribute_sha256')!=hashlib.sha256(attr.read_bytes()).hexdigest():
        p.error('complete graph collision pin differs; refuse reuse')
    graph['source_graph_state']=graph.get('state')
    graph['live_repin_evidence']='identical complete collision arrays; transition gameplay parity still unproved'
    graph['state']=pin;graph['elevation']={}
    (out/'live-graph.json').write_text(json.dumps(graph,indent=2)+'\n')
    (out/'live-state.json').write_text(json.dumps(pin,indent=2)+'\n')
    cmd=[sys.executable,'-B',str(Path(__file__).with_name('alttp_validate_room_heights.py')),
         '--graph',str(out/'live-graph.json'),'--snapshot',str(raw),'--attributes',str(attr),
         '--state',str(out/'live-state.json'),'--profile',str(out/'replay-input-profile.toml'),
         '--runtime-solution',str(out/'live-runtime.json'),'--rom',str(a.rom),
         '--output',str(out/'validation-v2.json'),'--diagnostic-svg',str(out/'validation-v2.svg')]
    result=subprocess.run(cmd,capture_output=True,text=True)
    before=json.loads(a.before.with_suffix('.json').read_text());after=json.loads(a.after.with_suffix('.json').read_text())
    with (out/'replay-input-profile.toml').open('rb') as f:profile=tomllib.load(f)
    proofs=diagonal_previews(RoomSnapshot.read(raw),profile,a.rom.read_bytes()[0x1b52:0x1b52+12876])
    samples=[]
    for xy,proof in sorted(proofs.items()):
        i=xy[1]*64+xy[0];b=before['layers'][1]['cells'][i];c=after['layers'][1]['cells'][i]
        samples.append(dict(xy=xy,hash=c['hash'],flips=c['flips'],before_base=b['base'],after_base=c['base'],
                            before_local=before['layers'][1]['local_samples'][i]['height'],
                            after_local=proof['height'],row=proof['row'],foot=proof['foot']))
    # Palette-index artwork: exact canonical pixels, not a guessed tile identity.
    blob=Path(str(a.capture)+'.dungeon-input.bin').read_bytes();vram=blob[24584:]
    art=Image.new('RGB',(4*96,96),(32,32,32))
    for col,row in enumerate(range(4)):
        word=int.from_bytes(blob[8+2*((4+row)*64+11):10+2*((4+row)*64+11)],'little')
        tile=Image.new('RGB',(8,8));base=state['graphics_base']+(word&1023)*32
        for y in range(8):
            for x in range(8):
                index=sum(((vram[(base+(b//2)*16+y*2+b%2)&65535]>>(7-x))&1)<<b for b in range(4))
                tile.putpixel((x,y),(index*17,)*3)
        art.paste(tile.resize((96,96),Image.Resampling.NEAREST),(col*96,0))
    art.save(out/'canonical-cap-upper-middle-foot-artwork.png')
    for prefix in (a.before,a.after,*a.companion_preview):
        for suffix in ('-captured-composed-height','-recomputed-composed-height'):
            f=Path(str(prefix)+suffix+'.ppm')
            if f.exists():Image.open(f).save(out/(prefix.name+suffix+'.png'))
    report=dict(verdict='reviewable but incomplete',capture=str(a.capture),
                pins={s:hashlib.sha256(Path(str(a.capture)+s).read_bytes()).hexdigest()
                      for s in ('','.profile.toml','.dungeon.json','.dungeon-input.bin','.dungeon-templates.bin')},
                validator_command=cmd,validator_exit=result.returncode,validator_stdout=result.stdout,
                validator_stderr=result.stderr,independent_preview_denominator=len(proofs),
                samples=samples,before_counts=[{k:v for k,v in l.items() if k not in ('cells','local_samples','relations','doorframes')} for l in before['layers']],
                after_counts=[{k:v for k,v in l.items() if k not in ('cells','local_samples','relations','doorframes')} for l in after['layers']],
                missing_evidence=['exact-state directed staircase playback','fresh gameplay comparison','complete corpus states'],approval=False)
    (out/'diagonal-review.json').write_text(json.dumps(report,indent=2)+'\n')
    print('independent previews',len(proofs),'validator exit',result.returncode,
          'see validation-v2.json and diagonal-review.json',result.stderr)

if __name__=='__main__':main()
