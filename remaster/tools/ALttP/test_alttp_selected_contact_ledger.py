import unittest
import json
import pathlib
import subprocess
import tempfile
from alttp_selected_contact_ledger import build


class ContactLedgerTest(unittest.TestCase):
    def fixture(self, flip=False, floor=0, missing=False, ambiguous=False):
        rows=[];physical={}
        for x in range(16):
            for y in range(8):
                wx=15-x if flip else x
                wall=x<8
                p={'id':1 if wall else 2,'owner':2<<24,'source':2,'bg':1,'has_height':1,'has_normals':1,'hash':'wall' if wall else 'floor','k':y*8+x%8,'base':90 if wall else floor,'local':7-x if wall else 0,'role':2 if wall else 1}
                if missing and x==8 and y==3:p['has_height']=0
                rows.append({'sx':wx,'sy':y,'use_scene':1,'scene':p,
                             'selection_policy':'S9xRemasterSelectVisibleChannels/v1',
                             'visible':p,'donors':{'height':p if p['has_height'] else {}}})
        physical[(1,0 if flip else 1,0)]={'plane_bases':[floor,floor if ambiguous else 255],'occupied_floor_owners':[12,13 if ambiguous else -1],'collision':[0,0]}
        return rows,physical

    def test_signed_and_reflected_contacts(self):
        for flip in (False,True):
            for floor in (0,30,-20):
                ledger,_=build(*self.fixture(flip,floor),[0,0])
                wall=[r for r in ledger if r['hash']=='wall']
                self.assertEqual(len(wall),64)
                self.assertTrue(all(r['expected_base']==floor for r in wall))

    def test_missing_and_ambiguous(self):
        for kwargs in ({'missing':True},{'ambiguous':True}):
            ledger,_=build(*self.fixture(**kwargs),[0,0])
            self.assertTrue(all(r['expected_base'] is None for r in ledger))

    def test_input_base_not_oracle(self):
        rows,physical=self.fixture()
        for r in rows:
            if r['scene']['role']==2:r['scene']['base']=-900
        ledger,_=build(rows,physical,[0,0])
        self.assertTrue(all(r['expected_base']==0 for r in ledger if r['hash']=='wall'))

    def test_owner_and_collision_are_required(self):
        for field,value in (('occupied_floor_owners',[-1,-1]),('collision',[1,1])):
            rows,physical=self.fixture()
            next(iter(physical.values()))[field]=value
            ledger,_=build(rows,physical,[0,0])
            self.assertTrue(all(r['expected_base'] is None for r in ledger))

    def test_parent_covered_floor_counterexample(self):
        rows, physical = self.fixture()
        for r in rows:
            # Exact legacy parent packet cannot establish selected exposure.
            r.pop('selection_policy')
            r['scene']['id'] = 1 if r['sx'] < 8 else 2
            r['scene']['owner'] = (2 << 24)
            r['main'] = dict(r['scene']); r['sub'] = {}
            if r['sx'] == 8:
                r['main'] = {'id':999, 'owner':(2 << 24) | (1 << 16),
                             'source':2, 'bg':2, 'has_height':1,
                             'has_normals':1, 'hash':'cover', 'k':r['sy']*8,
                             'base':99, 'local':0, 'role':1}
        ledger, _ = build(rows, physical, [0,0])
        self.assertEqual(sum(r['reason']=='unique_selected_floor_contact_path'
                             for r in ledger if r['hash']=='wall'), 0)

    def test_legacy_is_unsupported_not_exposed(self):
        rows, physical = self.fixture()
        for r in rows:r.pop('selection_policy')
        ledger, counts = build(rows, physical, [0,0])
        self.assertEqual(counts, {'unsupported_selection_observation':128})

    def test_visible_height_donor_and_owner_required(self):
        for change, reason in (({'id':99}, 'height_donor_not_visible_occurrence'),
                               ({'owner':0}, 'height_donor_not_visible_occurrence')):
            rows, physical = self.fixture()
            for r in rows:
                r['donors']['height'] = dict(r['visible'], **change)
            ledger, counts = build(rows, physical, [0,0])
            self.assertEqual(counts, {reason:128})
        rows, physical = self.fixture()
        for r in rows:r['visible']['owner']=0
        _, counts = build(rows, physical, [0,0])
        self.assertEqual(counts, {'unsupported_visible_owner':128})


class ProductionSelectionTest(unittest.TestCase):
    """Packets go through the actual C++ consumer, never a Python selector."""
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='alttp-ledger-')
        cls.out = pathlib.Path(cls.tmp.name)
        root = pathlib.Path(__file__).resolve().parents[3]
        for name, source in [('fixture',root/'remaster/tests/alttp_capture_debug_sources_fixture.cpp'),
                             ('producer',root/'remaster/tools/ALttP/alttp_capture_debug_sources.cpp')]:
            subprocess.run(['c++','-std=c++17','-O1',str(source),'-o',str(cls.out/name)],check=True)

    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()

    def packet(self, mode='open', order=0, hv=0, vertical=0):
        path = self.out/'packet.s9xrmf'
        subprocess.run([str(self.out/'fixture'),str(path),mode,str(order),str(hv),str(vertical)],check=True)
        rows = [json.loads(r) for r in subprocess.check_output([str(self.out/'producer'),str(path)],text=True).splitlines()]
        physical = {(order+1,0 if vertical else 2,2 if vertical else 0):
                    {'plane_bases':[0,255],'occupied_floor_owners':[12,-1],'collision':[0,0]}}
        return rows, physical

    def test_production_transforms_bg_orders_and_authored_zero(self):
        for order in (0,1):
            for hv in range(4):
                for vertical in (0,1):
                    rows, physical = self.packet(order=order,hv=hv,vertical=vertical)
                    ledger,_ = build(rows,physical,[0,0])
                    walls=[r for r in ledger if r['visible']['role']==2]
                    self.assertEqual(len(walls),128)
                    self.assertTrue(all(r['expected_base']==0 for r in walls))

    def test_production_covered_edges_chains_and_presentations(self):
        for order in (0,1):
            for hv in range(4):
                for vertical in (0,1):
                    for mode in ('floor_cover','floor_partial','wall_cover','wall_partial','obj','fallback','missing','hud_cover'):
                        rows, physical = self.packet(mode,order,hv,vertical)
                        ledger,_ = build(rows,physical,[0,0])
                        if mode=='wall_partial':
                            self.assertFalse(any(r['expected_base'] is not None for r in ledger if r['screen'][vertical]<8),mode)
                            # Independent downstream region remains supported.
                            self.assertEqual(sum(r['expected_base']==0 for r in ledger),63)
                        else:
                            self.assertFalse(any(r['expected_base'] is not None for r in ledger),mode)
        for mode in ('open','hud','same_scene','normals_missing'):
            rows, physical = self.packet(mode)
            ledger,_ = build(rows,physical,[0,0])
            self.assertEqual(sum(r['expected_base']==0 for r in ledger),128,mode)
        rows, physical = self.packet('fallback')
        r=next(r for r in rows if r['sx']==16)
        self.assertNotEqual(r['visible']['id'],r['donors']['height']['id'])
        self.assertEqual(r['donors']['height']['local'],0)
        rows, physical = self.packet('same_scene')
        self.assertNotEqual(rows[0]['main']['id'],rows[0]['scene']['id'])
        self.assertEqual(rows[0]['visible']['id'],rows[0]['scene']['id'])
        # Missing height is distinct from missing normals; authored zero height
        # wins even when another BG has a complete normal/height source.
        rows, physical = self.packet('normals_missing')
        self.assertTrue(rows[0]['donors']['height']['has_height'])
        self.assertFalse(rows[0]['donors']['normals']['id'])

    def test_contradictory_physical_plane_is_not_authority(self):
        rows,physical=self.packet()
        q=next(iter(physical.values()))
        q['plane_bases']=[0,0];q['occupied_floor_owners']=[12,13]
        ledger,_=build(rows,physical,[0,0])
        self.assertFalse(any(r['expected_base'] is not None for r in ledger))


if __name__=='__main__':unittest.main()
