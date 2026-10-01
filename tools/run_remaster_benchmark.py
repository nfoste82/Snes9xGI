#!/usr/bin/env python3
"""Run all seven live-app lighting cases from the same save state."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path, help="Snes9x.app or its executable")
    parser.add_argument("--rom", type=Path, default=Path.home() / "Downloads/Zelda_ALTTP.zip")
    parser.add_argument("--state", type=Path, default=Path.home() / "Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.004.frz")
    parser.add_argument("--profile", type=Path, default=Path(__file__).resolve().parents[1] / "alttp-profile.toml")
    parser.add_argument("--output", required=True, type=Path, help="New results directory")
    parser.add_argument("--seconds", type=float, default=3, help="Emulated seconds per measured case")
    parser.add_argument("--warmup-seconds", type=float, default=0.5)
    parser.add_argument("--move", choices=("none", "left", "right"), default="none")
    parser.add_argument("--presentation", choices=("deterministic", "live"), default="deterministic")
    parser.add_argument("--connections", type=int, help="Override profile connection count")
    parser.add_argument("--timeout", type=float, default=1800, help="Wall-clock timeout in seconds")
    args = parser.parse_args()
    executable = args.app.expanduser().resolve()
    if executable.suffix == ".app":
        executable /= "Contents/MacOS/Snes9x"
    output = args.output.expanduser().resolve()
    if output.exists():
        parser.error("Output directory must not already exist")
    command = [str(executable), "--remaster-benchmark"]
    for option, value in (("rom", args.rom.expanduser().resolve()), ("state", args.state.expanduser().resolve()),
                          ("profile", args.profile.expanduser().resolve()), ("output", output),
                          ("seconds", args.seconds), ("warmup-seconds", args.warmup_seconds),
                          ("move", args.move), ("presentation", args.presentation)):
        command += [f"--benchmark-{option}", str(value)]
    if args.connections is not None:
        command += ["--benchmark-connections", str(args.connections)]
    environment = os.environ.copy()
    environment.pop("S9X_REMASTER_GI_METRICS", None)
    environment.pop("S9X_REMASTER_FRAME_METRICS", None)
    print(f"Running {args.presentation} benchmark; {args.seconds:g} emulated seconds per case", flush=True)
    try:
        subprocess.run(command, env=environment, timeout=args.timeout, check=True)
    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Benchmark failed: {error}. Partial report, if any: {output / 'report.json'}\n")
    report = json.loads((output / "report.json").read_text())
    if report["status"] != "complete" or len(report["cases"]) != 7:
        parser.exit(1, "Benchmark report is incomplete\n")
    print("\nCase                                  GPU median/p95 ms  Fields/mesh ms  Frames/s  Samples")
    for case in report["cases"]:
        print(f"{case['name']:38} {case['gpu_ms']['median']:7.2f}/{case['gpu_ms']['p95']:7.2f} "
              f"{case['scene_fields_ms']['median']:6.2f}/{case['mesh_ms']['median']:6.2f} "
              f"{case['completed_frames_per_wall_second']:8.2f} {case['completed_measured_frames']:7}")
    if not all(case["matches_first_case_ram"] for case in report["cases"]):
        parser.exit(1, "Emulated RAM differs between cases; inspect report before comparing timings\n")
    print(f"Matching end-state RAM across all seven cases. Results: {output}")


if __name__ == "__main__":
    main()
