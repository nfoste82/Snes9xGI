#!/usr/bin/env python3
"""Compare identical live benchmark sequences, stage timings, and PNG files."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    args = parser.parse_args()
    before, after = [json.loads((p / "report.json").read_text()) for p in (args.before, args.after)]
    if before["status"] != "complete" or after["status"] != "complete":
        parser.error("Both reports must be complete")
    for key in ("status", "profile_sha256", "state_sha256", "rom_file_sha256", "connections",
                "movement", "presentation", "warmup_frames", "measurement_frames"):
        if before[key] != after[key]:
            parser.error(f"Different {key}: {before[key]} / {after[key]}")
    if len(before["cases"]) != len(after["cases"]):
        parser.error("Different case counts")
    for a, b in zip(before["cases"], after["cases"]):
        for key in ("name", "ram_sha256", "original_rgb555_sha256", "completed_measured_frames"):
            if a[key] != b[key]:
                parser.error(f"Different {key} in {a['name']}")
        old, new = a["gpu_ms"]["median"], b["gpu_ms"]["median"]
        hashes = [hashlib.sha256((p / c["image"]).read_bytes()).hexdigest()
                  for p, c in ((args.before, a), (args.after, b))]
        print(f"{a['name']:38} {old:7.2f} -> {new:7.2f} ms ({100*(new/old-1):+.1f}%) "
              f"PNG bytes identical={hashes[0] == hashes[1]}")
        for name, stats in b.get("gpu_stages_ms", {}).items():
            prior = a.get("gpu_stages_ms", {}).get(name)
            if prior:
                print(f"  {name:22} {prior['median']:8.3f} -> {stats['median']:8.3f} ms")


if __name__ == "__main__":
    main()
