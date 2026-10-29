import unittest
from summarize_live_trace import summarize


class LiveTraceSummaryTest(unittest.TestCase):
    def test_absent_and_multiple_reasons(self):
        row = dict(schema=1, sequence=0, frame_id=12, emulated_frame=42,
                   state="unfinished", refresh_complete=1, refresh_inclusive_ms=19,
                   gpu_evidence=0, replay_evidence=False,
                   metrics=dict(frame_id=12, field_reasons=512 | 256, exclusive_ms={}))
        result = summarize([row])
        self.assertEqual(result["reason_frame_counts"]["raster"], 1)
        self.assertEqual(result["reason_frame_counts"]["palette"], 1)
        self.assertEqual(result["states"]["unfinished"], 1)
        self.assertEqual(result["replay_evidence_frames"], 0)
        with self.assertRaises(ValueError):
            summarize([row, row])
        row["metrics"]["frame_id"] = 13
        with self.assertRaises(ValueError):
            summarize([row])


if __name__ == "__main__":
    unittest.main()
