import unittest
from types import SimpleNamespace
from alttp_diagonal_preview import partial_semantic_previews


class PartialDiagonalProofTest(unittest.TestCase):
    def fixture(self, flips):
        hflip, vflip = bool(flips & 1), bool(flips & 2)
        up = 1 if vflip else -1
        tangent = -1 if hflip == vflip else 1
        cells, assets, owned, seeds = {}, [], {}, {}
        for row in range(7):
            key = str(100+row)
            assets.append(dict(tile_hash=key, tile_role='wall', tile_semantics_authored=True,
                               tile_semantic_normals=['south_east'], height=[24]*64))
            for col in range(3):
                x,y = 20+col,30+col*tangent+row*up
                if col == 0 and row < 2:
                    continue  # cardinal final overwrite, not diagonal ownership
                cells[x,y] = SimpleNamespace(x=x,y=y,word=100+row | (0x4000 if hflip else 0) | (0x8000 if vflip else 0),
                                              tile_hash=key,hflip=hflip,vflip=vflip)
                owned[1,x,y] = {(key,True,row % 4)}
                if col:
                    intercept = 7+row*8
                    seeds[x,y] = dict(intercept=intercept,minimum=0,maximum=48,row=0,foot=0,
                                      height=[max(0,min(48,intercept-xx-yy)) for yy in range(8) for xx in range(8)])
        assets.append(dict(tile_hash='floor',tile_role='floor',height=[0]*64))
        class Snapshot:
            def cell(self,layer,x,y):
                return cells.get((x,y),SimpleNamespace(x=x,y=y,word=999,tile_hash='floor',hflip=False,vflip=False))
        return Snapshot(),dict(assets=assets),seeds,owned,cells,up

    def test_all_flips_complete_surviving_bands_and_overwrite(self):
        for flip in range(4):
            s,p,seeds,owned,cells,up = self.fixture(flip)
            result = partial_semantic_previews(s,p,seeds,1,owned)
            for row in range(2,7):
                proof = result[20,30+row*up]
                self.assertEqual(proof['intercept'],7+row*8)
                self.assertEqual(proof['evidence']['samples_verified'],64)
            self.assertNotIn((20,30),result)
            self.assertEqual(partial_semantic_previews(s,p,seeds,1,None),seeds)

    def test_reject_unowned_ambiguous_replaced_and_incomplete_seams(self):
        for flip in range(4):
            for failure in ('unowned','ambiguous','word','flips','missing_seam','wrong_phase','unknown_top','other_writer'):
                s,p,seeds,owned,cells,up = self.fixture(flip)
                key=(20,30+3*up)
                if failure=='unowned':owned.pop((1,*key))
                if failure=='ambiguous':owned[1,*key].add(('wrong',False,0))
                if failure=='other_writer':owned[1,*key]={(cells[key].tile_hash,False,3)}
                if failure=='word':cells[key].word^=1
                if failure=='flips':cells[key].hflip=not cells[key].hflip
                if failure=='missing_seam':
                    n=(21,30+(-1 if flip in (0,3) else 1)+3*up);seeds.pop(n);owned.pop((1,*n))
                if failure=='wrong_phase':
                    n=(21,30+(-1 if flip in (0,3) else 1)+3*up);seeds[n]={**seeds[n],'intercept':99}
                if failure=='unknown_top':
                    cells[20,30+7*up]=SimpleNamespace(x=20,y=30+7*up,word=998,tile_hash='unknown',hflip=False,vflip=False)
                result=partial_semantic_previews(s,p,seeds,1,owned)
                self.assertNotIn(key,result,(flip,failure))

    def test_disagreeing_two_owned_companions_reject_phase(self):
        s,p,seeds,owned,cells,up=self.fixture(0)
        # Place a second independently solved, same-owned column to the west
        # but with a different section maximum; both top phases are coherent.
        for row in range(2,7):
            x,y=19,31-row
            source=cells[20,30-row]
            cells[x,y]=SimpleNamespace(x=x,y=y,word=source.word,tile_hash=source.tile_hash,hflip=False,vflip=False)
            owned[1,x,y]=set(owned[1,20,30-row])
            intercept=15+row*8
            seeds[x,y]=dict(intercept=intercept,minimum=0,maximum=56,row=0,foot=0,
                            height=[max(0,min(56,intercept-xx-yy)) for yy in range(8) for xx in range(8)])
        result=partial_semantic_previews(s,p,seeds,1,owned)
        self.assertNotIn((20,27),result)


if __name__=='__main__':unittest.main()
