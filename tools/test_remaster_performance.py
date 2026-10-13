"""Regression gate contracts: reject invalid comparisons rather than passing them."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

import remaster_performance as perf


def report():
    result = {key: "fixed" for key in perf.IDENTITY}
    result.update(status="complete", presentation="deterministic", measurement_frames=2)
    result["cases"] = [{"name": "Composite", "completed_measured_frames": 2,
                        "matches_first_case_ram": True, "samples": [{"emulated_frame": 30}, {"emulated_frame": 31}],
                        "ram_sha256": "ram", "original_rgb555_sha256": "rgb", "view": 0,
                        "bounces": 0, "width": 256, "height": 224,
                        **{metric: {"median": 10, "p95": 12} for metric in perf.METRICS}}]
    return result


class PerformanceContracts(unittest.TestCase):
    def summary(self, reports):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for index, item in enumerate(reports):
                path = Path(directory) / str(index)
                path.mkdir()
                (path / "report.json").write_text(json.dumps(item))
                paths.append(path)
            return perf.summarize(paths)

    def test_median_of_runs_resists_one_noisy_run(self):
        reports = [report() for _ in range(3)]
        reports[2]["cases"][0]["gpu_ms"]["median"] = 100
        result = self.summary(reports)
        self.assertEqual(result["cases"][0]["metrics"]["gpu_ms"]["median"]["value"], 10)

    def test_slowdown_fails_and_noise_passes(self):
        base = self.summary([report() for _ in range(3)])
        current = copy.deepcopy(base)
        current["cases"][0]["metrics"]["gpu_ms"]["p95"]["value"] = 15
        self.assertEqual(sum(c["regression"] for c in perf.compare(base, current, 15, .5)), 1)
        current["cases"][0]["metrics"]["gpu_ms"]["p95"]["value"] = 13
        self.assertFalse(any(c["regression"] for c in perf.compare(base, current, 15, .5)))

    def test_all_identity_fields_enforced(self):
        for key in perf.IDENTITY:
            candidate = report()
            candidate[key] = "different"
            with self.subTest(key=key), self.assertRaises(ValueError):
                perf.comparable(report(), candidate)

    def test_behavior_mismatch_rejected(self):
        for key in ("ram_sha256", "original_rgb555_sha256", "view", "bounces", "width", "height"):
            candidate = report()
            candidate["cases"][0][key] = "different"
            with self.subTest(key=key), self.assertRaises(ValueError):
                perf.comparable(report(), candidate)

    def test_incomplete_and_nonfinite_samples_rejected(self):
        for change in ("count", "nan", "frames", "status", "ram"):
            candidate = report()
            if change == "count":
                candidate["cases"][0]["completed_measured_frames"] = 1
            elif change == "nan":
                candidate["cases"][0]["gpu_ms"]["p95"] = float("nan")
            elif change == "frames":
                candidate["cases"][0]["samples"][1]["emulated_frame"] = 30
            elif change == "status":
                candidate["status"] = "running"
            else:
                candidate["cases"][0]["matches_first_case_ram"] = False
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.summary([candidate] * 3)

    def test_environment_removes_experiments(self):
        from unittest.mock import patch
        with patch.dict(perf.os.environ, {"S9X_REMASTER_DIRECT_SAMPLES": "14", "MTL_CAPTURE_ENABLED": "1"}):
            environment = perf.clean_environment()
        self.assertNotIn("S9X_REMASTER_DIRECT_SAMPLES", environment)
        self.assertNotIn("MTL_CAPTURE_ENABLED", environment)
        self.assertEqual(environment["S9X_REMASTER_DISABLE_STAGE_COUNTERS"], "1")

    def test_compare_cli_writes_results_and_returns_failure(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline = self.summary([report() for _ in range(3)])
            baseline["schema_version"] = 1
            baseline_path = root / "baseline.json"
            baseline_path.write_text(json.dumps(baseline))
            paths = []
            for index in range(3):
                path = root / f"candidate-{index}"
                path.mkdir()
                item = report()
                item["cases"][0]["gpu_ms"]["p95"] = 15
                (path / "report.json").write_text(json.dumps(item))
                paths.append(str(path))
            output = root / "comparison"
            argv = ["performance", "compare", "--baseline", str(baseline_path),
                    "--output", str(output), *paths]
            with patch.object(perf.sys, "argv", argv):
                self.assertEqual(perf.main(), 1)
            self.assertFalse(json.loads((output / "comparison.json").read_text())["passed"])

    def test_live_launcher_rejects_competing_emulator(self):
        from argparse import Namespace
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            input_path = Path(directory) / "input"
            input_path.write_text("fixture")
            args = Namespace(app=input_path, rom=input_path, state=input_path, profile=input_path)
            with patch.object(perf.subprocess, "run") as process:
                process.return_value.returncode = 0
                with self.assertRaisesRegex(ValueError, "Quit the running"):
                    perf.run_benchmarks(args)


if __name__ == "__main__":
    unittest.main()
