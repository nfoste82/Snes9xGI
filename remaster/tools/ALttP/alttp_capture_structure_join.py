#!/usr/bin/env python3
"""Detection only: join actual packet pixels to independent placed contracts.

Requires exact complete-map identity, checks all observed canonical pixel/flips,
and retains main/sub/scene ownership separately. Never supplies geometry.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
from alttp_room_geometry import RoomSnapshot


def compare_pixel(pixel, contract, scroll):
    rx=(pixel['sx']+scroll[0])&511; ry=(pixel['sy']+scroll[1])&511
    k=(ry%8)*8+rx%8
    expected=contract.get('expected')
    return dict(pixel,rx=rx,ry=ry,cx=rx//8,cy=ry//8,writer=contract.get('writer',contract.get('source',{}).get('writer')),
                expected=None if expected is None else expected[k],
                contract_status=contract['status'],
                mismatch=expected is not None and (not pixel['has_height'] or pixel['actual']!=expected[k]))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('capture','pixels','snapshot','contracts','output'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--all-contracts',action='store_true',help='Join doorway and stair contracts too; retain buffer ownership, never treat hidden art as visible defects')
    a=p.parse_args();state=json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    blob=Path(str(a.capture)+'.dungeon-input.bin').read_bytes()
    if len(blob)!=90120 or blob[:8]!=b'ALTPDI1\0':p.error('requires complete capture inputs')
    s=RoomSnapshot.read(a.snapshot)
    if s.room!=state['room']:p.error('room mismatch')
    for l in range(2):
        for y in range(64):
            for x in range(64):
                w=struct.unpack_from('<H',blob,8+(8192 if l==0 else 0)+2*(y*64+x))[0]
                if s.cell(l,x,y).word!=w:p.error('complete maps differ; do not borrow contracts')
    cs=json.loads(a.contracts.read_text())
    contracts={(r['layer']+1,r['x'],r['y']):r for r in cs['placements'] if a.all_contracts or r['rule']=='diagonal_partial_pixel_geometry'}
    rows=[];count=0
    for line in a.pixels.open():
        r=json.loads(line);rx=(r['sx']+state['scroll'][0])&511;ry=(r['sy']+state['scroll'][1])&511
        c=s.cell(r['bg']-1,rx//8,ry//8)
        k=(7-ry%8 if c.vflip else ry%8)*8+(7-rx%8 if c.hflip else rx%8)
        if int(r['hash'],16)!=int(c.tile_hash.split(':')[-1],16) or r['k']!=k or bool(r['hf'])!=c.hflip or bool(r['vf'])!=c.vflip:
            p.error('packet/complete-map/camera mismatch')
        count+=1
        contract=contracts.get((r['bg'],rx//8,ry//8))
        if contract:rows.append(compare_pixel(r,contract,state['scroll']))
    summary=dict(verified_packet_bg_samples=count,complete_maps_equal=True,
                  contract_scope='all' if a.all_contracts else 'diagonal',
                  observed_contract_samples=len(rows),observed_diagonal_samples=len(rows) if not a.all_contracts else None,
                  failures=sum(r['mismatch'] for r in rows),
                 by_buffer_writer={str(k):v for k,v in Counter((r['buffer'],r['writer'],r['contract_status'],r['mismatch']) for r in rows).items()},
                 limitations=['Contract datums are conditional, not absolute anchors.',
                              'Only observed packet pixels checked; main/sub/scene are not independent physical surfaces.',
                              'Matching words do not imply identical live/reference state or profile.'])
    a.output.write_text(json.dumps(dict(summary=summary,pixels=rows),indent=2)+'\n')
    print(json.dumps(summary,indent=2))
    return int(summary['failures']>0)


if __name__=='__main__':raise SystemExit(main())
