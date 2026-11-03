import unittest
from summarize_live_trace import summarize, chrome_trace


class LiveTraceSummaryTest(unittest.TestCase):
    def test_lifecycle_gaps_and_reasons(self):
        rows = []
        for i, time in enumerate((10., 10.016, 10.116)):
            rows.append(dict(schema=2, sequence=i, frame_id=i, emulated_frame=i,
                state="completed", refresh_complete=1, refresh_inclusive_ms=3+i*40,
                gpu_evidence=1, replay_evidence=False, pacing_complete=1,
                audio_sync_ms=i*20, frame_limiter_ms=1, pacing_total_ms=i*20+1,
                present_complete=1, presented_uptime=time,
                metrics=dict(frame_id=i, field_reasons=0, room_reasons=64|256,
                    classification_reasons=256, calls={"classification": 2},
                    exclusive_ms={"classification": i*30})))
        result = summarize(list(reversed(rows)))
        self.assertAlmostEqual(result["presentation_gap_ms"]["max"], 100)
        self.assertEqual(result["presentation_hitches"]["50"], 1)
        self.assertEqual(result["room_reason_frame_counts"]["plane"], 3)
        self.assertEqual(result["audio_sync_ms"]["max"], 40)
        self.assertEqual(result["highest_refresh_frames"][0]["classification_reasons"], ["maps"])
        self.assertEqual(result["exclusive_stage_ms"]["classification"]["max"], 60)
        timeline = chrome_trace(rows)
        self.assertEqual(sum(e["name"] == "Presented" for e in timeline["traceEvents"]), 3)

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
