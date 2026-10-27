#!/usr/bin/env python3
"""Explicit research transfer: invalidate changed words, retain equal-word writers.

This does NOT recover live writer history. Output uses live graphics/maps and
labels reference-derived surviving ownership. Original replay assertions unchanged.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from alttp_room_geometry import RoomSnapshot,RoomProvenance

def transfer(reference,live,provenance):
    if reference.room!=live.room:raise ValueError('room mismatch')
    owners=[list(v) for v in provenance.owners];changed=[]
    for l in range(2):
        for i,(a,b) in enumerate(zip(reference.cells[l],live.cells[l])):
            if a.word!=b.word:owners[l][i]=65535;changed.append([l,i%64,i//64,a.word,b.word])
    return owners,changed

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('reference','live','output'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();assert not a.output.exists()
    ref=RoomSnapshot.read(a.reference);live=RoomSnapshot.read(a.live);prov=RoomProvenance.read(Path(str(a.reference)+'.prov'),ref)
    owners,changed=transfer(ref,live,prov);original=Path(str(a.reference)+'.prov').read_bytes();prefix=12+10*len(prov.objects)
    data=original[:prefix]+struct.pack('<8192H',*(owners[0]+owners[1]))+struct.pack('<8192H',*(c.word for layer in live.cells for c in layer))
    a.output.write_bytes(a.live.read_bytes());Path(str(a.output)+'.prov').write_bytes(data)
    RoomProvenance.read(Path(str(a.output)+'.prov'),live)
    Path(str(a.output)+'.transfer.json').write_text(json.dumps(dict(approval=False,kind='research_equal_word_reference_provenance_not_live_history',changed_cells=changed,retained_cells=8192-len(changed),inputs={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in (a.reference,a.live,Path(str(a.reference)+'.prov'))}),indent=2)+'\n')
if __name__=='__main__':main()
