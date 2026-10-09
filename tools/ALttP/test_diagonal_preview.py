import unittest
from types import SimpleNamespace
from alttp_diagonal_preview import diagonal_previews

class DiagonalPreviewTest(unittest.TestCase):
    def test_legacy_cell_only_snapshot_has_no_preview_proof(self):
        from test_alttp_validate_room_heights import Snapshot
        self.assertEqual(diagonal_previews(Snapshot(),{},bytearray(12876)),{})

    def test_partial_real_or_decode_adapter_inputs_are_not_hidden(self):
        from alttp_room_geometry import RoomSnapshot
        with self.assertRaisesRegex(ValueError,'complete graphics'):
            diagonal_previews(object.__new__(RoomSnapshot),{},bytearray(12876))
        with self.assertRaisesRegex(ValueError,'complete graphics'):
            diagonal_previews(SimpleNamespace(layers=[(0,0)]),{},bytearray(12876))
        s,p,t,w=self.fixture()
        with self.assertRaisesRegex(ValueError,'complete ROM template'):
            diagonal_previews(s,p,t[:100])
        s.layers=[(0,0),(0,)]
        with self.assertRaisesRegex(ValueError,'valid graphics layer'):
            diagonal_previews(s,p,t)

    def fixture(self):
        words={};data=bytearray(12876);assets=[]
        def put(off,w):data[off:off+2]=w.to_bytes(2,'little')
        for axis,off in enumerate((0x2e8,0x2c8)):
            for band in range(3):
                w=100+axis*3+band;put(off+band*2,w)
                assets.append(dict(tile_hash=str(w),height=[24+8*(2-band)+7-(x if axis else y) for y in range(8) for x in range(8)]))
        for r in range(5):
            put(0x420+r*2,200+r)
            assets.append(dict(tile_hash=str(200+r),height=[1]*64))
        for col in range(6):
            for row in range(5):words[20+col,20-col+row]=200+row
        class Snapshot:
            layers=[(0,0),(0,0)]
            def _tile_hash(self,address):return str(address//16)
            def cell(self,layer,x,y):
                if not(0<=x<64 and 0<=y<64):raise ValueError('out of map')
                w=words.get((x,y),999)
                return SimpleNamespace(word=w,tile_hash=str(w),hflip=False,vflip=False)
        return Snapshot(),dict(assets=assets),data,words

    def test_exact_bands_and_overwritten_rejection(self):
        s,p,t,w=self.fixture();r=diagonal_previews(s,p,t)
        self.assertEqual(len(r),24)
        self.assertEqual(r[20,21]['height'][:8],list(range(47,39,-1)))
        w[20,20]=999
        r=diagonal_previews(s,p,t)
        self.assertNotIn((20,20),r)
        self.assertIn((20,21),r)
        w[40,40]=201
        self.assertNotIn((40,40),diagonal_previews(s,p,t))

    def test_disagreeing_cross_section_and_opening_veto(self):
        s,p,t,w=self.fixture();p['assets'][0]['height'][0]+=1
        self.assertFalse(diagonal_previews(s,p,t))
        p['assets'][0]['height'][0]-=1
        next(a for a in p['assets'] if a['tile_hash']=='201')['tile_role']='door'
        self.assertNotIn((20,21),diagonal_previews(s,p,t))

if __name__=='__main__':unittest.main()
