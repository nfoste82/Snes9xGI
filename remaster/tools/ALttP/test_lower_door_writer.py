import unittest
from types import SimpleNamespace
from alttp_validate_room_heights import _doorway_footprints


class LowerDoorWriterTests(unittest.TestCase):
    def test_full_four_word_split_writer_flips_and_overwrite(self):
        for flip in range(4):
            data=bytearray(12876)
            cells={}
            def cell(bg,x,y):
                return cells.get((bg,x,y),SimpleNamespace(layer=bg,x=x,y=y,word=0xffff,tile_hash='empty'))
            assets={}
            for x in range(4):
                for y in range(4):
                    word=0x100+x*4+y
                    data[0x1288+2*(x*4+y):0x1288+2*(x*4+y)+2]=word.to_bytes(2,'little')
                    px=20+(3-x if flip&1 else x);py=20+(3-y if flip&2 else y)
                    for bg in range(2 if y==3 else 1):
                        key=bg,px,py;h=f'part{x}{y}'
                        cells[key]=SimpleNamespace(layer=bg,x=px,y=py,word=word^(0x4000 if flip&1 else 0)^(0x8000 if flip&2 else 0),tile_hash=h)
                        assets[h]=dict(tile_role='doorway',doorway_support=x==0 and y==1)
            s=SimpleNamespace(cell=cell)
            claims=[c for c in _doorway_footprints(s,assets,data) if c['source']==0x1288]
            self.assertEqual(len(claims),1)
            self.assertEqual(len(claims[0]['members']),12)
            # A signed opening is not structural, but its final write attests
            # this handler. Losing it must destroy whole-writer ownership.
            c=cell(0,20+(2 if flip&1 else 1),20+(2 if flip&2 else 1));c.word^=1
            self.assertFalse([c for c in _doorway_footprints(s,assets,data) if c['source']==0x1288])


if __name__=='__main__':unittest.main()
