import unittest
from alttp_authored_door_detection import check


class ClosedPanelTests(unittest.TestCase):
    def fixture(self):
        h=[y for y in range(8) for x in range(8)]
        return dict(tile_role='door',height=h),dict(bg=2,x=1,y=2,hash='1',
            actual=[v+24 for v in h],has_effective_height=True)

    def test_authored_relief_with_datum(self):
        a,t=self.fixture();self.assertEqual(check(a,t)['status'],'pass_conditional')

    def test_lost_center_source(self):
        a,t=self.fixture();t['has_effective_height']=False
        self.assertEqual(check(a,t)['reason'],'missing_selected_source')

    def test_partial_overwrite_not_flattened(self):
        a,t=self.fixture();t['actual'][19]=0
        self.assertEqual(check(a,t)['reason'],'nonconstant_authored_relief_offset')

    def test_doorway_cutout_is_not_closed_panel(self):
        a,t=self.fixture();a['tile_role']='doorway';t['actual'][19]=0
        self.assertIsNone(check(a,t))

    def test_no_guessed_array_or_variant(self):
        a,t=self.fixture();a.pop('height')
        self.assertEqual(check(a,t)['status'],'unsupported')
        a,t=self.fixture();a['geometry_by_flip']=True
        self.assertEqual(check(a,t)['status'],'unsupported')

    def test_hv_canonical_arrays_remain_canonical(self):
        a,t=self.fixture();a['height']=list(reversed(a['height']))
        t['actual']=[v+16 for v in a['height']];t['hflip']=t['vflip']=True
        self.assertEqual(check(a,t)['observed_offset'],16)


if __name__=='__main__':unittest.main()
