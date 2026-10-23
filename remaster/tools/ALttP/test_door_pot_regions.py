import unittest
from types import SimpleNamespace
from alttp_validate_room_heights import audit_doorframes

class Regions(unittest.TestCase):
    def test_placement_owned_region_without_asset_role(self):
        off=0x2716
        data=bytearray(12876)
        words=[0x100+n for n in range(12)]
        for n,w in enumerate(words):data[off+2*n:off+2*n+2]=w.to_bytes(2,'little')
        cells={}
        for x in range(4):
            for y in range(3):cells[1,10+x,10+y]=SimpleNamespace(layer=1,x=10+x,y=10+y,word=words[x*3+y],tile_hash='test')
        class Snapshot:
            def cell(self,l,x,y):return cells.get((l,x,y),SimpleNamespace(layer=l,x=x,y=y,word=0xffff,tile_hash='empty'))
        local=[23-k//8 if k%8<6 else 0 for k in range(64)]
        occ=[255 if k%8<6 else 0 for k in range(64)]
        mask=sum(1<<k for k in range(64) if occ[k])
        asset=dict(tile_hash='test',height=local,occlusion=occ)
        record=dict(layer=1,x=10,y=10,plane=0,visible=True,frame_base=24,template_offset=off,
                    mask=f'{mask:016x}',effective_height=[h+(24 if mask>>k&1 else 0) for k,h in enumerate(local)])
        r=audit_doorframes(dict(doorframe_attachments=[record]),Snapshot(),dict(assets=[asset]),data)
        self.assertEqual(r['solved_marked_placements'],1);self.assertFalse(r['issues'])
        cells[1,13,12].word^=1
        r=audit_doorframes(dict(doorframe_attachments=[record]),Snapshot(),dict(assets=[asset]),data)
        self.assertEqual(r['solved_marked_placements'],0);self.assertTrue(r['issues'])

if __name__=='__main__':unittest.main()
