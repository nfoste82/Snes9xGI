"""Fixture-oracle negative cases: a plausible pass must never hide a defect."""
import unittest
from alttp_verify_room061_detection import verify


class ConfirmedDetectionTests(unittest.TestCase):
    def test_reject_other_room(self):
        with self.assertRaises(AssertionError):verify({'room_id':1},{'room_id':1})

    def test_reject_incomplete_denominator(self):
        report=dict(room_id=97,mode='detection_only',publishable=False,complete_bg_placements=4096)
        with self.assertRaises(AssertionError):verify(report,report)

    def test_reject_geometry_approval(self):
        report=dict(room_id=97,mode='detection_only',publishable=True,complete_bg_placements=8192)
        with self.assertRaises(AssertionError):verify(report,report)


if __name__=='__main__':unittest.main()
