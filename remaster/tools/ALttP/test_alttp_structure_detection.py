import unittest
from unittest.mock import patch

from alttp_structure_detection import audit, compare_pixels, external_datum
from alttp_doorway_detection import displayed


class StructureDetectionTests(unittest.TestCase):
    def test_partial_pixels_all_flips_even_unchanged_span(self):
        local=[min(48,55-x-y) for y in range(8) for x in range(8)]
        for flip in (0,0x4000,0x8000,0xc000):
            self.assertEqual(compare_pixels(local,local,0,flip)['status'],'pass')
            wrong=local[:];wrong[15],wrong[63]=wrong[63],wrong[15]
            r=compare_pixels(wrong,local,0,flip)
            self.assertEqual(r['status'],'fail')
            self.assertEqual((min(r['actual']),max(r['actual'])),(min(r['expected']),max(r['expected'])))
            mask=[int(i in (15,63)) for i in range(64)]
            self.assertEqual(r['mismatch_mask'],f'{sum(v<<i for i,v in enumerate(displayed(mask,flip))):016x}')

    def test_datum_relief_retained_conflict_not_max_other_bg(self):
        c={(1,2,3):dict(role='floor',local=[i%2 for i in range(64)],actual=[24+i%2 for i in range(64)]),
           (0,2,3):dict(role='floor',local=[0]*64,actual=[80]*64)}
        self.assertEqual(external_datum(c,[(1,2,3)]),24)
        self.assertIsNone(external_datum(c,[(1,2,3),(0,2,3)]))
        self.assertIsNone(external_datum(c,[(1,2,4)]))
        c[1,2,3]['actual'][1]+=1
        self.assertIsNone(external_datum(c,[(1,2,3)]))

    def fixture(self):
        class Cell:
            word=0;tile_hash='v1:4bpp:0000000000000001';hflip=False;vflip=False
        class Snapshot:
            room=1
            def cell(self,*args):return Cell()
        class Provenance:
            objects=[]
            def owner(self,*args):return 1
        s=Snapshot();p=Provenance()
        profile=dict(assets=[dict(tile_hash=Cell.tile_hash,tile_role='wall',height=[0]*64)])
        terminal=[dict(bg=l+1,x=x,y=y,hash='1',actual=[0]*64) for l in range(2) for y in range(64) for x in range(64)]
        return s,p,profile,terminal

    def test_no_shape_proof_is_not_solved_even_with_arrays(self):
        s,p,profile,t=self.fixture()
        owned={(1,10,10):{('v1:4bpp:0000000000000001',True,0)}}
        with patch('alttp_structure_detection.inspect',return_value=([],owned)),patch('alttp_structure_detection.diagonal_previews',return_value={}):
            r=audit(s,p,profile,b'',{},t)
        self.assertEqual(r['status_counts'],{'unresolved':1})
        self.assertEqual(r['placements'][0]['reason'],'no_independent_transformed_shape_proof')

    def test_competing_writer_phase_claims_reject_even_identical_height(self):
        s,p,profile,t=self.fixture()
        owned={(1,10,10):{('a',True,0),('a',False,0)}}
        with patch('alttp_structure_detection.inspect',return_value=([],owned)),patch('alttp_structure_detection.diagonal_previews',return_value={}):
            r=audit(s,p,profile,b'',{},t)
        self.assertEqual(r['placements'][0]['reason'],'ambiguous_dispatch_ownership')

    def test_complete_inventory_required(self):
        s,p,profile,t=self.fixture()
        with self.assertRaises(ValueError):audit(s,p,profile,b'',{},t[:-1])

    def test_owned_partial_diagonal_terminal_pixels_not_solver_label(self):
        s,p,profile,t=self.fixture()
        wallhash=profile['assets'][0]['tile_hash'];floorhash='v1:4bpp:0000000000000002'
        profile['assets'].append(dict(tile_hash=floorhash,tile_role='floor',height=[0]*64))
        original=s.cell
        class Floor:
            word=0;tile_hash=floorhash;hflip=False;vflip=False
        s.cell=lambda l,x,y:Floor() if (l,x,y)==(1,11,10) else original(l,x,y)
        t[4096+10*64+11]['hash']='2'
        shape=[max(0,7-x-y) for y in range(8) for x in range(8)]
        t[4096+10*64+10]['actual']=shape[:]
        owned={(1,10,10):{(wallhash,False,3)}}
        proof={(10,10):dict(height=shape,minimum=0,maximum=24,intercept=7)}
        with patch('alttp_structure_detection.inspect',return_value=([],owned)),patch('alttp_structure_detection.diagonal_previews',side_effect=lambda *a,**k:proof if a[3]==1 else {}):
            r=audit(s,p,profile,b'',{},t)
            self.assertEqual(r['status_counts'],{'pass':1})
            # Only the slope region changes; the flat zero floor remains exact.
            t[4096+10*64+10]['actual'][0]=0
            r=audit(s,p,profile,b'',{},t)
        self.assertEqual(r['status_counts'],{'fail':1})
        self.assertEqual(r['placements'][0]['mismatch_pixels'],[0])
        self.assertEqual(r['placements'][0]['support_datum'],0)

    def test_intact_stair_preserves_ramp_overwrite_rejects(self):
        s,p,profile,t=self.fixture()
        profile['assets'][0].update(tile_role='floor',height=[0]*64)
        p.objects=[dict(phase=1,ordinal=2,is_door=0)]
        obj={(1,2):dict(subtype=1,object_id=0x1e,x=10,y=10)}
        templates=bytes(12876)
        for c in t:
            if c['bg']==2 and c['x'] in (11,12) and c['y']==14:c['actual']=[24]*64
            if c['bg']==2 and 10<=c['x']<14 and 10<=c['y']<14:c['actual']=[24]*64
        # BG1 has no actual surviving dispatch. BG2 is intact, one member
        # demonstrably lost the common contextual datum despite solver claims.
        p.owner=lambda l,x,y:1 if l==1 and 10<=x<14 and 10<=y<14 else 0
        t[4096+11*64+11]['actual']=[0]*64
        with patch('alttp_structure_detection.inspect',return_value=([],{})),patch('alttp_structure_detection.diagonal_previews',return_value={}):
            r=audit(s,p,profile,templates,obj,t)
            self.assertEqual(r['status_counts'],{'pass':15,'fail':1})
            p.owner=lambda l,x,y:0 if (x,y)==(10,10) else 1 if l==1 and 10<=x<14 and 10<=y<14 else 0
            r=audit(s,p,profile,templates,obj,t)
        self.assertEqual(r['status_counts'],{'unresolved':16})
        self.assertEqual({c['reason'] for c in r['placements']},{'partial_or_overwritten_stair_dispatch'})


if __name__=='__main__':unittest.main()
