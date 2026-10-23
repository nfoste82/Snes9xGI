#!/usr/bin/env python3
"""User-reviewed v4 family regressions, including golden approved C-D arrays."""
import copy
import json
from pathlib import Path
import unittest
import alttp_overworld_geometry_test as contracts
from alttp_overworld_castle import generate


class CastleReview(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if contracts.CORE is None: raise unittest.SkipTest('supply --core')
        root=Path(__file__).resolve().parents[3]/'build/remaster-geometry'
        fixture=root/'overworld-castle-core-v1.json'
        if not fixture.exists(): raise unittest.SkipTest('ROM fixture unavailable')
        cls.base=json.loads(fixture.read_text())
        cls.out,cls.rules=generate(copy.deepcopy(cls.base),contracts.CORE)
        cls.by={(p['x'],p['y']):p for p in cls.out['placements']}
        cls.root=root

    def test_uniform_recess_face_and_two_turns(self):
        a=next(a for a in self.out['assemblies'] if a['family']=='castle-courtyard-recess-crosssection')
        def z(x,y):
            p=self.by[a['x']+x//8,a['y']+y//8]
            return p['base']+p['local'][(y%8)*8+x%8]
        for y in range(16,48):
            self.assertEqual(z(40,y),z(64,y))
            self.assertEqual(z(64,y),z(88,y))
        self.assertGreater(z(16,40),z(40,40))
        self.assertGreater(z(111,40),z(88,40))

    def test_approved_cd_exact_before_after(self):
        # Optional original v3 artifact is independent from regenerated output.
        golden=self.root/'castle-approved-lower-cd.json'
        if not golden.exists(): self.skipTest('v3 golden unavailable')
        old=json.loads(golden.read_text());self.assertEqual(len(old),20)
        for p in old:
            q=self.by[p['x'],p['y']]
            for k in ('map16','word','base','local','normals'): self.assertEqual(q[k],p[k])

    def test_inlay_cap_and_obstacle_support_are_distinct(self):
        hashes={'dc2718cb9cd15edb','385417e424616991','ee8e193a05ca99c1',
                'a682bd6e97be8df1','d93b6cdd6881047f'}
        ps=[p for p in self.out['placements'] if (p['hash'] or '').split(':')[-1] in hashes]
        self.assertEqual(len(ps),20)
        self.assertTrue(all(p['status']=='solved' and p['base']==32 and
                            p['local']==[0]*64 and p['role']=='floor' for p in ps))
        caps=[p for p in self.out['placements'] if p['role']=='cap']
        self.assertTrue(all(p['movement_support']=='not-walkable' for p in caps))
        obstacles=[p for p in self.out['placements'] if p['map16'] in (1159,1160,1163) and p['role']=='obstacle']
        self.assertTrue(obstacles)
        self.assertTrue(all(p['local']==[6]*64 for p in obstacles if p['status']=='solved'))

    def test_decorations_do_not_erase_underlying_face_and_storeys(self):
        for a in self.out['assemblies']:
            if 'round-column' not in a['family']: continue
            ps=[p for p in self.out['placements'] if p['assembly']==a['id']]
            for p in ps:
                if p['status']!='solved': continue
                for i,v in enumerate(p['local']):
                    y=(p['y']-a['y'])*8+i//8
                    self.assertGreaterEqual(v,round(32*max(0,min(1,(47-y)/31))))
        # Full higher storey now solves independently above terrace32 / ledge64.
        self.assertEqual(self.by[248,196]['base'],64)
        self.assertEqual(self.by[254,198]['base'],64)
        self.assertEqual(self.out['coverage']['conflicts'],0)
        # Entire reported upper assembly incl offscreen ledge/storey: no missing
        # cells, not merely a positive total from its central projecting pillar.
        required=[self.by[x,y] for y in range(192,208) for x in range(240,272)]
        self.assertTrue(all(p['status']=='solved' for p in required))

    def test_true_lower_cd_upper_fg_relative_equivalence(self):
        # Correct labels established against original RGB: lower shoulder234,218
        # and upper shoulder240,198, not the previously misnamed golden strip.
        for dy in range(8):
            for dx in range(2):
                a=self.by[234+dx,218+dy];b=self.by[240+dx,198+dy]
                self.assertEqual(a['local'],b['local'])
                self.assertEqual(a['normals'],b['normals'])
                self.assertEqual(b['base']-a['base'],32)

    def test_exact_live_open_door_overwrite_and_rejection(self):
        live=copy.deepcopy(self.base)
        by={(p['x'],p['y']):p for p in live['placements']}
        words=((346,254,254|0x4000,346|0x4000),
               (362,255,255|0x4000,362|0x4000))
        # One coherent final dispatch writer for the exact containing assembly.
        for y in range(194,200):
            for x in range(226,234):by[x,y]['writer']=21
        for y,row in enumerate(words):
            for x,w in enumerate(row):by[228+x,198+y]['word']=w|(7<<10)
        out,_=generate(live,contracts.CORE);ps={(p['x'],p['y']):p for p in out['placements']}
        for y in (198,199):
            for x in (229,230):
                self.assertEqual(ps[x,y]['role'],'floor')
                self.assertEqual(ps[x,y]['base'],32)
                self.assertEqual(ps[x,y]['local'],[0]*64)
        broken=copy.deepcopy(live)
        next(p for p in broken['placements'] if (p['x'],p['y'])==(229,198))['writer']=22
        rejected,_=generate(broken,contracts.CORE)
        self.assertNotEqual(next(p for p in rejected['placements'] if (p['x'],p['y'])==(229,198))['status'],'solved')

    def test_shared_interface_coordinates_not_independent_ramp_restarts(self):
        def sample(x,y):
            p=self.by[x//8,y//8];i=y%8*8+x%8
            return p['base']+p['local'][i],p['normals'][i]
        # Full compatible E-F -> F-G seam; shared side derivative matches on
        # both sides. Cap is horizontal; side is west-facing, never south.
        for x in range(1920,1952):
            a,na=sample(x,1583);b,nb=sample(x,1584)
            self.assertLessEqual(abs(a-b),1,(x,a,b))
            if x<1936:
                self.assertEqual(nb,[-1,0,0])
                if x>1920:self.assertEqual(na,nb)
            else:self.assertEqual(nb,[0,0,1])
        # Every pixel of the perpendicular compatible face/cap contact too.
        for y in range(1568,1584):
            self.assertLessEqual(abs(sample(1935,y)[0]-sample(1936,y)[0]),1)
            self.assertEqual(sample(1936,y)[1],[0,0,1])
        # Upper outside ends at its low ledge64; foot cannot repeat its bottom
        # half and reset to79. A real face-to-ledge crease changes normal.
        for x in range(1952,1968):
            self.assertLessEqual(abs(sample(x,1583)[0]-sample(x,1584)[0]),1)
            self.assertEqual(sample(x,1584)[1],[0,0,1])
            self.assertEqual(sample(x,1584)[0],64)


if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--core',type=Path,required=True)
    a,rest=p.parse_known_args();contracts.CORE=a.core.resolve()
    unittest.main(argv=[__file__,*rest])
