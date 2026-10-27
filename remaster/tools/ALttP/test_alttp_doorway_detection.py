import unittest
import struct
from unittest.mock import patch
from alttp_doorway_detection import check_assembly, check_closed_authored_assembly, displayed, candidates, audit
from alttp_room_geometry import RoomProvenance


class DoorwayDetectionTests(unittest.TestCase):
    def fixture(self, split=True, base=24):
        a=dict(origin=[10,10],direction=3,width=3,height=4,split=split,writer=7,sources=[0x2fd2],members=[])
        cells={}
        for x in range(3):
            for y in range(4):
                l=int(x==2) if split else 1;k=(l,10+x,10+y);a['members'].append(list(k))
                opening=x<2 and y in (1,2)
                local=[0]*64 if opening else [7-i%8 for i in range(64)]
                cells[k]=dict(role='floor' if opening else 'doorway',local=local,actual=[base]*64 if opening else [base+8*x+v for v in local])
        for y in (11,12):cells[(0 if split else 1,9,y)]=dict(role='floor',local=[0]*64,actual=[base]*64)
        return a,cells

    def test_positive_split_and_non_split(self):
        for split in (True,False):
            a,c=self.fixture(split);self.assertEqual({r['status'] for r in check_assembly(a,c)},{'pass'})

    def test_detect_interior_even_self_consistent_solver(self):
        a,c=self.fixture();c[0,10,11]['actual']=[52]*64
        r=next(r for r in check_assembly(a,c) if (r['layer'],r['x'],r['y'])==(0,10,11))
        self.assertEqual(r['expected'],[24]*64);self.assertEqual(r['status'],'fail')

    def test_door_not_floor_and_retains_relief(self):
        a,c=self.fixture();c[0,10,11]=dict(role='door',local=[3]*64,actual=[27]*64)
        r=next(r for r in check_assembly(a,c) if r['rule']=='closed_door_assembly')
        self.assertEqual(r['status'],'pass');self.assertEqual(r['expected'],[27]*64)

    def test_closed_barrier_keeps_authored_zero_and_ignores_open_frame_holes(self):
        a,c=self.fixture(split=False,base=0)
        c[1,10,11]=dict(role='door',local=[i%8 for i in range(64)],actual=[i%8 for i in range(64)])
        # Outer band contains complete authored relief but an open-state mask.
        c[1,12,11]['mask']=(1<<32)-1
        c[1,12,11]['actual'][40]=0
        r=next(r for r in check_closed_authored_assembly(a,c) if (r['x'],r['y'])==(12,11))
        self.assertEqual(r['status'],'fail');self.assertEqual(r['expected'][40],23)
        c[1,12,11]['actual']=r['expected']
        r=next(r for r in check_closed_authored_assembly(a,c) if (r['x'],r['y'])==(12,11))
        self.assertEqual(r['status'],'pass')

    def test_open_assembly_and_ambiguous_closed_band_not_guessed(self):
        a,c=self.fixture();self.assertEqual(check_closed_authored_assembly(a,c),[])
        c[0,10,11]['role']='door';c[0,10,11]['local']=[24]*64
        r=next(r for r in check_closed_authored_assembly(a,c) if (r['x'],r['y'])==(10,11))
        self.assertEqual(r['status'],'unresolved');self.assertIsNone(r['expected'])

    def test_conflicting_landing_is_unknown_not_max(self):
        a,c=self.fixture();c[0,9,12]['actual']=[0]*64
        self.assertEqual({r['status'] for r in check_assembly(a,c)},{'unresolved'})

    def test_plane_overlap_does_not_choose_other_bg(self):
        a,c=self.fixture(base=0)
        for y in (11,12):c[1,9,y]=dict(role='floor',local=[0]*64,actual=[80]*64)
        self.assertEqual({r['status'] for r in check_assembly(a,c)},{'pass'})

    def test_bg2_counterpart_not_own_plane(self):
        a,c=self.fixture(base=0)
        for y in range(10,14):
            c[0,12,y]=dict(role='wall',local=[7-i%8 for i in range(64)],actual=[23-i%8 for i in range(64)])
            c[1,12,y]['actual']=[38-i%8 for i in range(64)]
        r=[r for r in check_assembly(a,c) if r['layer']==1]
        self.assertTrue(all(t['status']=='fail' and t['expected'][0]==23 for t in r))

    def test_cap_already_contains_band_not_double_added(self):
        a,c=self.fixture(base=0)
        for y in range(10,14):
            c[1,12,y]['local']=[23-i%8 for i in range(64)]
            c[1,12,y]['actual']=[39-i%8 for i in range(64)]
        r=[r for r in check_assembly(a,c) if r['layer']==1]
        self.assertTrue(all(t['status']=='fail' and t['expected'][0]==23 for t in r))

    def test_hv_sample_transform(self):
        v=list(range(64))
        self.assertEqual(displayed(v,0xc000),v[::-1])
        self.assertEqual(displayed(displayed(v,0x4000),0x4000),v)

    def test_north_south_contacts_and_intrinsic_cap(self):
        for direction in (0,1):
            a=dict(origin=[10,10],direction=direction,width=4,height=3,split=True,writer=7,sources=[0x2b88],members=[])
            cells={};outer=10 if direction==0 else 12
            for x in range(10,14):
                for y in range(10,13):
                    l=int(y==outer);opening=x in (11,12) and y!=outer
                    band=12-y if direction==0 else y-10
                    local=[0]*64 if opening else [band*8+7-i//8 for i in range(64)]
                    k=(l,x,y);a['members'].append(list(k))
                    cells[k]=dict(role='floor' if opening else 'doorway',local=local,actual=[24+v for v in local])
            for x in (11,12):cells[0,x,13 if direction==0 else 9]=dict(role='floor',local=[0]*64,actual=[24]*64)
            self.assertEqual({r['status'] for r in check_assembly(a,cells)},{'pass'})

    def test_missing_terminal_or_contact_stays_unknown(self):
        a,c=self.fixture();del c[0,9,11]
        self.assertEqual({r['status'] for r in check_assembly(a,c)},{'unresolved'})

    def test_competing_ownership_rejected_not_arbitrarily_selected(self):
        class Cell:
            word=0
            tile_hash='v1:4bpp:0000000000000001'
        class Snapshot:
            room=96
            def cell(self,*args):return Cell()
        class Provenance:
            objects=[dict(is_door=1)]
        a=dict(writer=1,origin=[0,0],direction=0,width=4,height=3,split=True,
               members=[[0,0,0]],sources=[123])
        b=dict(a,origin=[1,0])
        terminal=[dict(bg=l+1,x=x,y=y,hash='1',local=[0]*64,actual=[0]*64)
                  for l in range(2) for y in range(64) for x in range(64)]
        profile=dict(assets=[dict(tile_hash=Cell.tile_hash,tile_role='doorway',height=[0]*64)])
        with patch('alttp_doorway_detection.candidates',return_value=([a,b],{1:{(0,0,0)}})):
            result=audit(Snapshot(),Provenance(),profile,b'',terminal)
        self.assertEqual(result['denominators']['intact_assemblies'],0)
        self.assertEqual(result['unresolved_groups'],{'competing_intact_templates':2})
        self.assertEqual(result['denominators']['unsupported_writer_cells'],1)

    def test_template_writer_overwrite_and_flips(self):
        class Cell:
            def __init__(self,w):self.word=w
        class Snapshot:
            def __init__(self):self.words={}
            def cell(self,l,x,y):return Cell(self.words.get((l,x,y),0))
        class Provenance:pass
        for flip in range(4):
            s=Snapshot();p=Provenance();p.objects=[dict(is_door=0),dict(is_door=1)];p.owners=[[0]*4096 for _ in range(2)]
            data=bytearray(0x3100)
            for x in range(3):
                for y in range(4):
                    word=100+x*4+y;n=0x2fd2+2*(x*4+y);data[n:n+2]=word.to_bytes(2,'little')
                    xx=12-x if flip&1 else 10+x;yy=13-y if flip&2 else 10+y;l=int(x==2)
                    s.words[l,xx,yy]=word^(0x4000 if flip&1 else 0)^(0x8000 if flip&2 else 0);p.owners[l][yy*64+xx]=2
            found,_=candidates(s,p,data);self.assertTrue(any(a['origin']==[10,10] for a in found))
            # A later writer, even with identical artwork, cannot lend support.
            l,x,y=next(iter(s.words));p.owners[l][y*64+x]=0
            found,_=candidates(s,p,data);self.assertFalse(found)

    def test_serialized_altprv1_last_writer_and_identical_overwrite(self):
        class Cell:
            def __init__(self, word): self.word=word
        class Snapshot:
            room=97
            def __init__(self): self.cells=[[Cell(0) for _ in range(4096)] for _ in range(2)]
            def cell(self,l,x,y): return self.cells[l][y*64+x]
        s=Snapshot();templates=bytearray(0x3100);owners=[0]*8192
        for x in range(3):
            for y in range(4):
                word=100+x*4+y;n=0x2fd2+2*(x*4+y)
                templates[n:n+2]=word.to_bytes(2,'little')
                s.cells[1][(10+y)*64+10+x].word=word
                owners[4096+(10+y)*64+10+x]=2
        def serialized(records):
            return (b'ALTPRV1\0'+struct.pack('<HH',s.room,len(records))+
                    b''.join(struct.pack('<5H',*r) for r in records)+
                    struct.pack('<8192H',*owners)+
                    struct.pack('<8192H',*(c.word for layer in s.cells for c in layer)))
        records=[(1,0,0,0,0),(3,6,0,2,1)]
        p=RoomProvenance(serialized(records),s)
        found,writers=candidates(s,p,templates)
        self.assertEqual(set(writers),{2})
        self.assertTrue(found)
        self.assertEqual(p.objects[found[0]['writer']-1]['ordinal'],6)
        # A real later non-door dispatch owns one unchanged word. The intact
        # template must now reject; background0 and sentinelffff are not doors.
        owners[4096+10*64+10]=3;owners[0]=0xffff
        records.append((3,7,0,2,0))
        p=RoomProvenance(serialized(records),s)
        found,writers=candidates(s,p,templates)
        self.assertFalse(found)
        self.assertEqual(sum(map(len,writers.values())),11)


if __name__=='__main__':unittest.main()
