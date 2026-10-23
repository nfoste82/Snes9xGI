#!/usr/bin/env python3
"""Record/check repeated freeze-based performance baselines, or compare saved runs."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
IDENTITY = (
    "schema_version", "gpu", "os", "rom_file_sha256", "state_sha256", "profile_sha256",
    "movement", "presentation", "connections", "representative_emitters",
    "warmup_frames", "measurement_frames", "debug_sphere_enabled", "audio_muted",
    "transport_threads_override", "serial_direct_override", "serial_indirect_override",
    "full_mesh_override", "stage_counters_disabled", "direct_samples_override",
    "full_sphere_samples_override", "full_depth_samples_override", "expanded_sampled_depth_override",
)
METRICS = ("gpu_ms", "scene_fields_ms", "mesh_ms", "gpu_setup_ms")
PERCENTILES = ("median", "p95")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_report(directory):
    report = json.loads((directory / "report.json").read_text())
    if report.get("status") != "complete" or not report.get("cases"):
        raise ValueError(f"Incomplete benchmark: {directory}")
    for key in IDENTITY:
        if key not in report:
            raise ValueError(f"Missing identity field {key}: {directory}")
    names = [case["name"] for case in report["cases"]]
    if len(names) != len(set(names)):
        raise ValueError("Duplicate benchmark cases")
    for case in report["cases"]:
        samples = case.get("samples", [])
        count = case["completed_measured_frames"]
        if not count or len(samples) != count or not case["matches_first_case_ram"]:
            raise ValueError(f"Invalid sample count/RAM: {case['name']}")
        if report["presentation"] == "deterministic" and count != report["measurement_frames"]:
            raise ValueError(f"Incomplete deterministic frames: {case['name']}")
        frames = [s["emulated_frame"] for s in samples]
        if frames != sorted(set(frames)):
            raise ValueError(f"Nonunique/unordered frame tags: {case['name']}")
        for metric in METRICS:
            for stat in PERCENTILES:
                value = case[metric][stat]
                if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value) or value < 0:
                    raise ValueError(f"Invalid {metric}.{stat}: {case['name']}")
        for key in ("ram_sha256", "original_rgb555_sha256", "view", "bounces", "width", "height"):
            if key not in case:
                raise ValueError(f"Missing {key}: {case['name']}")
    return report


def comparable(reference, candidate):
    for key in IDENTITY:
        if reference[key] != candidate[key]:
            raise ValueError(f"Incomparable {key}: {reference[key]!r} / {candidate[key]!r}")
    if [c["name"] for c in reference["cases"]] != [c["name"] for c in candidate["cases"]]:
        raise ValueError("Incomparable case list")
    for a, b in zip(reference["cases"], candidate["cases"]):
        for key in ("ram_sha256", "original_rgb555_sha256", "view", "bounces", "width", "height"):
            if a[key] != b[key]:
                raise ValueError(f"Incomparable {key} in {a['name']}")
        if reference["presentation"] == "deterministic":
            if [s["emulated_frame"] for s in a["samples"]] != [s["emulated_frame"] for s in b["samples"]]:
                raise ValueError(f"Different deterministic frame sequence: {a['name']}")


def summarize(directories):
    if len(directories) < 3:
        raise ValueError("At least three independent runs required")
    reports = [load_report(p) for p in directories]
    for report in reports[1:]:
        comparable(reports[0], report)
    cases = []
    for index, case in enumerate(reports[0]["cases"]):
        metrics = {}
        for metric in METRICS:
            metrics[metric] = {}
            for stat in PERCENTILES:
                values = [r["cases"][index][metric][stat] for r in reports]
                metrics[metric][stat] = {"value": statistics.median(values), "runs": values}
        cases.append({"name": case["name"], "metrics": metrics})
    return {"report": reports[0], "cases": cases, "runs": [str(p) for p in directories]}


def compare(baseline, current, relative_percent, absolute_ms):
    comparable(baseline["report"], current["report"])
    results = []
    for old, new in zip(baseline["cases"], current["cases"]):
        for metric in METRICS:
            for stat in PERCENTILES:
                before = old["metrics"][metric][stat]["value"]
                after = new["metrics"][metric][stat]["value"]
                allowance = max(absolute_ms, before * relative_percent / 100)
                results.append({"case": old["name"], "metric": metric, "stat": stat,
                                "before_ms": before, "after_ms": after, "allowed_increase_ms": allowance,
                                "regression": after > before + allowance})
    return results


def clean_environment():
    # Shipping-cost benchmark, never inherit an experiment/readback override.
    environment = {k: v for k, v in os.environ.items()
                   if not k.startswith("S9X_REMASTER_") and k != "MTL_CAPTURE_ENABLED"}
    environment["S9X_REMASTER_DISABLE_STAGE_COUNTERS"] = "1"
    return environment


def run_benchmarks(args):
    executable = args.app.expanduser().resolve()
    if executable.suffix == ".app":
        executable /= "Contents/MacOS/Snes9x"
    for path in (executable, args.rom.expanduser(), args.state.expanduser(), args.profile.expanduser()):
        if not path.is_file():
            raise ValueError(f"Missing input: {path}")
    # A second running emulator contaminates the baseline and may own shared UI.
    running = subprocess.run(["pgrep", "-x", "Snes9x"], capture_output=True, text=True)
    if running.returncode == 0:
        raise ValueError("Quit the running Snes9x app before recording/checking performance")
    if running.returncode != 1:
        raise ValueError("Could not check for a running Snes9x app")
    output = args.output.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=False)
    provenance = {"executable": str(executable), "executable_sha256": digest(executable)}
    framework = executable.parent.parent / "Frameworks/snes9x_framework.framework/Versions/A/snes9x_framework"
    if framework.is_file():
        provenance["framework_sha256"] = digest(framework)
    (output / "launch.json").write_text(json.dumps(provenance, indent=2) + "\n")
    directories = []
    for index in range(args.repeats):
        directory = output / f"run-{index + 1:02d}"
        command = [sys.executable, str(ROOT / "tools/run_remaster_benchmark.py"),
                   "--app", str(executable), "--rom", str(args.rom.expanduser().resolve()),
                   "--state", str(args.state.expanduser().resolve()),
                   "--profile", str(args.profile.expanduser().resolve()), "--output", str(directory),
                   "--seconds", str(args.seconds), "--warmup-seconds", str(args.warmup_seconds),
                   "--move", args.move, "--presentation", args.presentation, "--timeout", str(args.timeout)]
        if not args.all_cases:
            command.append("--composite-only")
        if args.connections is not None:
            command.extend(("--connections", str(args.connections)))
        print(f"Run {index + 1}/{args.repeats}: {directory}", flush=True)
        with (output / f"run-{index + 1:02d}.log").open("w") as log:
            subprocess.run(command, env=clean_environment(), stdout=log, stderr=subprocess.STDOUT, check=True)
        directories.append(directory)
    result = summarize(directories)
    result["provenance"] = provenance
    return result


def positive(value):
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="command", required=True)
    for name in ("record", "check"):
        sub = subs.add_parser(name)
        sub.add_argument("--baseline", type=Path, required=True)
        sub.add_argument("--output", type=Path, required=True, help="New directory for reports/logs")
        sub.add_argument("--app", type=Path, default=ROOT / "build/release/Snes9x.app")
        sub.add_argument("--rom", type=Path, default=Path.home() / "Downloads/Zelda_ALTTP.zip")
        sub.add_argument("--state", type=Path, default=Path.home() / "Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.006.frz")
        sub.add_argument("--profile", type=Path, default=ROOT / "Profiles/ALttP/alttp-profile.toml")
        sub.add_argument("--seconds", type=positive, default=3)
        sub.add_argument("--warmup-seconds", type=positive, default=0.5)
        sub.add_argument("--repeats", type=int, default=3)
        sub.add_argument("--move", choices=("none", "left", "right"), default="none")
        sub.add_argument("--presentation", choices=("deterministic", "live"), default="deterministic")
        sub.add_argument("--connections", type=int)
        sub.add_argument("--all-cases", action="store_true", help="Seven lighting cases instead of Composite/zero bounce")
        sub.add_argument("--timeout", type=positive, default=1800)
    sub = subs.add_parser("compare", help="Check previously collected runs; does not launch the game")
    sub.add_argument("--baseline", type=Path, required=True)
    sub.add_argument("--output", type=Path, required=True, help="New directory for comparison")
    sub.add_argument("runs", type=Path, nargs="+", help="At least three directories containing report.json")
    for sub in (subs.choices["check"], subs.choices["compare"]):
        sub.add_argument("--relative-percent", type=positive, default=15)
        sub.add_argument("--absolute-ms", type=positive, default=0.5)
    args = parser.parse_args()
    try:
        baseline_path = args.baseline.expanduser().resolve()
        if args.command == "record":
            if baseline_path.exists():
                raise ValueError("Baseline exists; choose a new file for deliberate re-baselining")
            if not baseline_path.parent.is_dir():
                raise ValueError("Baseline parent directory must exist")
        else:
            baseline = json.loads(baseline_path.read_text())
            if baseline.get("schema_version") != 1:
                raise ValueError("Unsupported baseline schema")
        if args.command == "compare":
            current = summarize([p.expanduser().resolve() for p in args.runs])
            args.output.expanduser().resolve().mkdir(parents=True, exist_ok=False)
        else:
            if args.repeats < 3:
                raise ValueError("At least three repetitions required")
            current = run_benchmarks(args)
        current["schema_version"] = 1
        output = args.output.expanduser().resolve()
        (output / "summary.json").write_text(json.dumps(current, indent=2) + "\n")
        if args.command == "record":
            baseline_path.write_text(json.dumps(current, indent=2) + "\n")
            print(f"Baseline recorded: {baseline_path}")
            for case in current["cases"]:
                gpu = case["metrics"]["gpu_ms"]
                print(f"{case['name']}: GPU median {gpu['median']['value']:.3f} / p95 {gpu['p95']['value']:.3f} ms")
            return 0
        checks = compare(baseline, current, args.relative_percent, args.absolute_ms)
        failures = [c for c in checks if c["regression"]]
        (output / "comparison.json").write_text(json.dumps({"passed": not failures, "checks": checks}, indent=2) + "\n")
        for check in checks:
            if check["regression"] or check["metric"] == "gpu_ms":
                print(f"{'FAIL' if check['regression'] else 'PASS'} {check['case']} {check['metric']}.{check['stat']}: "
                      f"{check['before_ms']:.3f} -> {check['after_ms']:.3f} ms")
        print(f"{len(failures)} regressions / {len(checks)} checks. Results: {output}")
        return 1 if failures else 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"Performance check failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
