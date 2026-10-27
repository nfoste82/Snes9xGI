import copy
import unittest
from alttp_compare_height_contracts import compare


class ContractComparisonTest(unittest.TestCase):
    def setUp(self):
        self.b={'placements':[dict(rule='mask',layer=1,x=3,y=4,status='fail',actual=[7]*64,expected=[0]*64)]}
        self.a=copy.deepcopy(self.b)
        self.a['placements'][0].update(status='pass',actual=[0]*64)
        self.t=[dict(bg=2,x=3,y=4,has_effective_height=True,actual=[0]*64)]

    def test_fixed(self):
        self.assertEqual(compare(self.b,self.a,self.t)['status'],'pass')

    def test_changed_expectation_cannot_hide_defect(self):
        self.a['placements'][0].update(actual=[7]*64,expected=[7]*64)
        self.t[0]['actual']=[7]*64
        r=compare(self.b,self.a,self.t)
        self.assertEqual(r['failed'],1)
        self.assertEqual(r['mismatch_pixels'],64)

    def test_missing_source_zero_array_is_not_pass(self):
        self.t[0]['has_effective_height']=False
        self.assertEqual(compare(self.b,self.a,self.t)['status'],'fail')

    def test_partial_pixel_regression_in_previously_passing(self):
        self.b['placements'][0].update(status='pass',actual=[0]*64)
        self.t[0]['actual'][63]=7
        self.a['placements'][0]['actual'][63]=7
        r=compare(self.b,self.a,self.t)
        self.assertEqual(r['previously_passing'],1)
        self.assertEqual(r['records'][0]['mismatch_pixels'],[63])

    def test_unresolved_not_repair(self):
        self.a['placements'][0]['status']='unresolved'
        self.assertEqual(compare(self.b,self.a,self.t)['status'],'fail')

    def test_missing_not_repair(self):
        self.a['placements']=[]
        self.assertEqual(compare(self.b,self.a,self.t)['status'],'fail')

    def test_hflip_compares_displayed_not_canonical_pixels(self):
        canonical=list(range(64))
        displayed=[canonical[y*8+7-x] for y in range(8) for x in range(8)]
        self.b['placements'][0]['expected']=displayed
        self.a['placements'][0].update(actual=displayed,expected=displayed)
        self.t[0]['actual']=canonical
        self.assertEqual(compare(self.b,self.a,self.t,{(1,3,4):0x4000})['status'],'pass')
