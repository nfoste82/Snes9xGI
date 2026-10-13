"""Runner contract tests; no C++ compiler, ROM or GPU required."""

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import run_remaster_tests as runner


class RunnerTests(unittest.TestCase):
    def test_selection_and_list_are_side_effect_free(self):
        with patch.object(runner, "command_to_log") as command:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(runner.main(["--list", "--suite", "full", "--metal"]), 0)
            self.assertIn("19 selected; no tests executed", output.getvalue())
            self.assertIn("dungeon_small_stack_optimized", output.getvalue())
            self.assertNotIn("legacy_remaster_smoke", output.getvalue())
            command.assert_not_called()

    def exercise(self, failure=None, metal=False, changing=False):
        calls = []
        specs = [dict(name=n, source="synthetic.cpp", kind="metal" if metal else "portable",
                      flags=[], arguments=[], description="runner contract") for n in ("one", "two")]

        def command(cmd, log, timeout, env):
            calls.append(cmd)
            log.write_text("all output retained\n")
            status = "passed"
            if "-o" in cmd:
                if failure == "compile" and cmd[-1].endswith("one/test"):
                    status = "failed"
                else:
                    Path(cmd[-1]).write_bytes(b"binary")
            elif cmd[0].endswith("one/test") and failure == "run":
                status = "failed"
            return dict(command=cmd, status=status, returncode=0 if status == "passed" else 1)

        with tempfile.TemporaryDirectory() as temporary:
            args = ["--output", temporary, "--sanitize"] + (["--metal"] if metal else [])
            fingerprints = [{"sha256": "initial"}] * 20
            if changing:
                fingerprints[3] = {"sha256": "changed"}
            with patch.object(runner, "test_specs", return_value=specs), \
                    patch.object(runner, "source_snapshot", side_effect=fingerprints), \
                    patch.object(runner, "command_to_log", side_effect=command), \
                    patch.object(runner, "git_text", return_value="dirty"), \
                    patch.object(runner.shutil, "which", return_value=None), \
                    patch.object(runner.sys, "platform", "linux" if metal else "darwin"), \
                    contextlib.redirect_stdout(io.StringIO()):
                code = runner.main(args)
            results = list(Path(temporary).glob("*/results.json"))
            self.assertEqual(len(results), 1)
            report = json.loads(results[0].read_text())
        return code, report, calls

    def test_pass_and_sanitizer_flags(self):
        code, report, calls = self.exercise()
        self.assertEqual(code, 0)
        self.assertEqual(report["counts"]["passed"], 2)
        compile_command = calls[1]
        self.assertIn("-UNDEBUG", compile_command)
        self.assertIn("-fsanitize=address,undefined", compile_command)
        self.assertIn("-fno-sanitize-recover=all", compile_command)

    def test_compile_failure_continues_without_running_stale_binary(self):
        code, report, calls = self.exercise(failure="compile")
        self.assertEqual(code, 1)
        self.assertEqual([t["status"] for t in report["tests"]], ["compile_failed", "passed"])
        self.assertFalse(any(c[0].endswith("one/test") for c in calls))

    def test_runtime_failure_is_not_a_pass_and_later_test_runs(self):
        code, report, _ = self.exercise(failure="run")
        self.assertEqual(code, 1)
        self.assertEqual(report["counts"], dict(selected=2, executed=2, passed=1, failed=1, not_executed=0))

    def test_metal_unavailable_fails_not_skips(self):
        code, report, calls = self.exercise(metal=True)
        self.assertEqual(code, 1)
        self.assertEqual(report["counts"]["passed"], 0)
        self.assertIn("requires macOS", report["tests"][0]["error"])
        self.assertEqual(len(calls), 1)  # compiler identification only

    def test_source_mutation_invalidates_gate(self):
        code, report, _ = self.exercise(changing=True)
        self.assertEqual(code, 1)
        self.assertEqual(report["tests"][0]["status"], "inputs_changed")


if __name__ == "__main__":
    unittest.main()
