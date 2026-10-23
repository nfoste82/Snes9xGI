"""Exhaust exact room061 footprint, ceiling writers and signed contact constraints.

No new height anchors: report whether existing construction/support proves one.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import tomllib
from alttp_room_geometry import RoomSnapshot, RoomProvenance
from alttp_diagonal_sets import inspect


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','raw','atlas','rom','runtime','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();snapshot=RoomSnapshot.read(a.raw)
    provenance=RoomProvenance.read(Path(str(a.raw)+'.prov'),snapshot)
    blob=Path(str(a.capture)+'.dungeon-input.bin').read_bytes()
    state=json.loads(Path(str(a.capture)+'.dungeon.json').read_text())
    assets={v['tile_hash']:v for v in tomllib.loads(Path(str(a.capture)+'.profile.toml').read_text())['assets']}
    runtime=json.loads(a.runtime.read_text())
    cells={(r['layer'],r['plane'],r['x'],r['y']):r for r in runtime['directional_geometry']}
    parity=[]
    for layer in range(2):
        for cell in snapshot.cells[layer]:
            live=struct.unpack_from('<H',blob,8+(8192 if layer==0 else 0)+2*(cell.y*64+cell.x))[0]
            if live!=cell.word:parity.append([layer,cell.x,cell.y])
    if parity:raise ValueError('reference final writer cannot attest changed live maps')
    with sqlite3.connect(a.atlas) as db:
        objects=[json.loads(r[0]) for r in db.execute("SELECT data_json FROM world_record WHERE kind='dungeon_objects'")]
    objects={(o['layer'],o['ordinal']):o for o in objects if o['room_id']==snapshot.room}
    template=a.rom.read_bytes()[0x1b52:0x1b52+12876]
    assemblies,claims=inspect(snapshot,provenance,objects,template)
    def placed(layer,x,y):
        cell=snapshot.cell(layer,x,y);owner=provenance.owner(layer,x,y)
        source=provenance.objects[owner-1] if 0<owner<=len(provenance.objects) else None
        asset=assets.get(cell.tile_hash,{})
        return dict(xy=[x,y],word=cell.word,hash=cell.tile_hash,writer=owner,writer_record=source,
                    writer_object=objects.get((source['phase'],source['ordinal'])) if source else None,
                    height=asset.get('height'),normals=asset.get('normal_xyz'),
                    planes=[cells[layer,p,x,y] for p in range(2)])
    sections=[]
    # Both ends, both layers, every seven-identity interior and every ceiling
    # contact. No viewport-only denominator and no selected favorable seam.
    ceiling=[]
    for layer in range(2):
        for x in (8,9,10,11,52,53,54,55):
            y0=x+30 if x<32 else 93-x
            sections.append(dict(layer=layer,x=x,placements=[placed(layer,x,y) for y in range(y0-1,y0+8)]))
        for cell in snapshot.cells[layer]:
            if cell.word!=0x3c15:continue
            neighbors=[]
            for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                x,y=cell.x+dx,cell.y+dy
                if 0<=x<64 and 0<=y<64 and (layer,x,y) in claims:
                    neighbors.append(placed(layer,x,y))
            if neighbors:ceiling.append(dict(layer=layer,ceiling=placed(layer,cell.x,cell.y),neighbors=neighbors))
    report=dict(approval=False,verdict='blocked by missing evidence for arbitrary extension',
                live_reference_word_mismatches=parity,state=state,
                profile_sha256=hashlib.sha256(Path(str(a.capture)+'.profile.toml').read_bytes()).hexdigest(),
                sections=sections,ceiling_contacts=ceiling,dispatches=assemblies,
                ceiling_base_counts=dict(Counter(str(p['base']) for c in ceiling for p in c['ceiling']['planes'])),
                conclusion=['all ceiling contacts have unsolved placement base; local60 is not an effective top anchor',
                'actual diagonal handlers emit five source rows per stepped column; size extends tangent, not cross section',
                'seven final identities have attested HIGH/LOW writers; no additional repeated face strip dispatched here',
                'using60 as top would contradict independently authored cardinal shell48 at zero base'])
    a.output.write_text(json.dumps(report,indent=2)+'\n')
    print('8192 live/reference final words match; sections',len(sections),'ceiling contacts',len(ceiling),
          'ceiling plane bases',report['ceiling_base_counts'])
    print('ceiling writer classes',Counter((c['ceiling']['writer_object']['subtype'],c['ceiling']['writer_object']['object_id'])
         if c['ceiling']['writer_object'] else ('default',c['ceiling']['writer_record']['phase'] if c['ceiling']['writer_record'] else None) for c in ceiling))
    for r in sections[8:12]:
        print('section',r['x'],[(v['xy'],hex(v['word']),v['writer'],[(p['base'],p.get('preview_maximum')) for p in v['planes']]) for v in r['placements']])


if __name__=='__main__':main()
