import unittest
from alttp_capture_structure_join import compare_pixel


class CaptureJoinTest(unittest.TestCase):
    def test_split_doorway_visible_bg1_not_hidden_bg2_and_nested_writer(self):
        c=dict(source=dict(writer=66),status='fail',expected=[0]*64)
        # Production scene exposes the BG1 interior, not the opaque-looking
        # BG2 candidate from a separate uncomposited cell inventory.
        visible=dict(sx=192,sy=116,bg=1,buffer='scene',has_height=1,actual=16)
        r=compare_pixel(visible,c,[768,12])
        self.assertEqual((r['writer'],r['cx'],r['cy'],r['buffer']),(66,56,16,'scene'))
        self.assertTrue(r['mismatch'])
        self.assertFalse(compare_pixel(dict(visible,actual=0),c,[768,12])['mismatch'])

    def test_resolved_packet_source_not_offline_pass(self):
        # Identical identity/offset and available array do not prove transport.
        p=dict(sx=216,sy=136,bg=2,k=63,has_height=1,actual=1,placement=0)
        c=dict(writer=30,status='pass',expected=[25+x+y for y in range(8) for x in range(8)])
        r=compare_pixel(p,c,[768,3240])
        self.assertEqual((r['cx'],r['cy'],r['expected']),(59,38,25))
        self.assertTrue(r['mismatch'])
        self.assertFalse(compare_pixel(dict(p,actual=25),c,[768,3240])['mismatch'])

    def test_same_range_partial_error(self):
        p=dict(sx=7,sy=7,has_height=1,actual=0)
        c=dict(writer=1,status='pass',expected=list(range(64)))
        self.assertTrue(compare_pixel(p,c,[0,0])['mismatch'])

    def test_missing_source_and_unproved_contract(self):
        p=dict(sx=0,sy=0,has_height=0,actual=0)
        c=dict(writer=1,status='pass',expected=[0]*64)
        self.assertTrue(compare_pixel(p,c,[0,0])['mismatch'])
        self.assertFalse(compare_pixel(p,dict(c,expected=None,status='unresolved'),[0,0])['mismatch'])


if __name__=='__main__':unittest.main()
