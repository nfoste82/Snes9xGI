import unittest
from alttp_scene_cardinal_continuity import audit, compare
from alttp_doorway_detection import displayed


class SceneBandTest(unittest.TestCase):
    def face(self, base, axis=1, reverse=0, edge=0):
        step=1 if reverse else -1
        local=[(x if axis else y)*step+(0 if reverse else 7) for y in range(8) for x in range(8)]
        return dict(k=(7 if axis else 56) if edge==0 else 0, authored=local,
                    has_height=True, base=base, local=local[(7 if axis else 56) if edge==0 else 0])
    def claim(self,axis=1,reverse=0): return dict(writer=17,axis=axis,reverse=reverse,section=[[1,2],[2,2],[3,2]])
    def test_alternating_scene_wrong_base(self):
        c=self.claim(); a=self.face(44); b=self.face(24,edge=1)
        r=compare(a,b,c,c,1); self.assertEqual((r['status'],r['actual_step'],r['expected_step'],r['residual']),('fail',-13,-1,-12))
    def test_correct_and_reverse_and_both_axes(self):
        for axis in (0,1):
            for reverse in (0,1):
                c=self.claim(axis,reverse); a=self.face(24,axis,reverse); b=self.face(32 if reverse else 16,axis,reverse,1)
                self.assertEqual(compare(a,b,c,c,axis)['status'],'pass')
    def test_missing_or_nonunit_does_not_pass(self):
        c=self.claim();a=self.face(44);b=self.face(36,edge=1);b['has_height']=False
        self.assertIsNone(compare(a,b,c,c,1));b['has_height']=True;b['authored'][1]=b['authored'][0]
        self.assertIsNone(compare(a,b,c,c,1))
    def test_independent_writer_and_orientation_excluded(self):
        c=self.claim();a=self.face(44);b=self.face(36,edge=1);other=dict(c,writer=18)
        self.assertIsNone(compare(a,b,c,other,1));self.assertIsNone(compare(a,b,c,c,0))
    def test_canonical_flip_transforms_recover_oriented_gradient(self):
        for axis,bit in ((1,0x4000),(0,0x8000)):
            c=self.claim(axis,1);a=self.face(24,axis,1);b=self.face(32,axis,1,1)
            a['authored']=displayed(self.face(0,axis,0)['authored'],bit)
            b['authored']=displayed(self.face(0,axis,0)['authored'],bit)
            self.assertEqual(compare(a,b,c,c,axis)['status'],'pass')
    def test_observed_scene_not_terminal_or_hidden_main(self):
        class Cell:
            tile_hash='v1:4bpp:0000000000000001'; word=0; hflip=False; vflip=False
        class Snapshot:
            room=2
            def cell(self,*unused):return Cell()
        terminal=[dict(bg=bg,x=x,y=y,hash='1',base=36,plane_bases=[36,0])
                  for bg in (1,2) for y in range(64) for x in range(64)]
        claim=self.claim();claims={(2,0,0):claim,(2,1,0):claim}
        local=self.face(0)['authored'];assets={1:{'height':local}}
        def pixel(sx,base,k):
            s=dict(id=1,source=2,bg=2,hash='1',k=k,ink=1,has_height=True,base=base,local=local[k])
            return dict(sx=sx,sy=0,use_scene=True,main=dict(s,base=36),scene=s)
        rows=[pixel(7,44,7),pixel(8,24,0)]
        result=audit(Snapshot(),claims,terminal,assets,rows,[0,0])
        self.assertEqual(result['status_counts'],{'fail':1})
        self.assertEqual(result['records'][0]['residual'],-12)
        rows[1]['scene']['hash']='2'
        with self.assertRaisesRegex(ValueError,'stale'): audit(Snapshot(),claims,terminal,assets,rows,[0,0])
