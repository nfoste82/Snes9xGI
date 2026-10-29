#!/usr/bin/env python3
"""Offline bounded live-trace summary; never prints source host paths."""
import argparse
import json
import math
import statistics

REASONS = {1: "forced", 2: "dimensions", 4: "scene_metadata", 8: "material",
           16: "content_geometry", 32: "emission_metadata", 64: "instance_identity",
           128: "context", 256: "palette", 512: "raster", 1024: "composition_depth",
           2048: "normal_mask", 4096: "cold"}


def summarize(rows):
    if len(rows) > 600:
        raise ValueError("trace exceeds bounded schema")
    ids = [r["frame_id"] for r in rows]
    if len(set(ids)) != len(ids):
        raise ValueError("duplicate frame IDs")
    if any(r["schema"] != 1 or r["metrics"]["frame_id"] != r["frame_id"] for r in rows):
        raise ValueError("schema/lineage mismatch")
    complete = [r for r in rows if r["refresh_complete"]]
    times = sorted(r["refresh_inclusive_ms"] for r in complete)
    reasons = {name: sum(bool(r["metrics"]["field_reasons"] & bit) for r in rows)
               for bit, name in REASONS.items()}
    high = sorted(complete, key=lambda r: r["refresh_inclusive_ms"], reverse=True)[:3]
    return {"schema": 1, "frames": len(rows), "refresh_complete": len(complete),
            "states": {s: sum(r["state"] == s for r in rows)
                       for s in ("completed", "slot_drop", "unfinished")},
            "refresh_p50_ms": statistics.median(times) if times else None,
            "refresh_p95_ms": times[math.ceil(len(times) * .95) - 1] if times else None,
            "reason_frame_counts": reasons,
            "highest_refresh_frames": [{"frame_id": r["frame_id"],
                                        "emulated_frame": r["emulated_frame"],
                                        "refresh_ms": r["refresh_inclusive_ms"],
                                        "exclusive_ms": r["metrics"]["exclusive_ms"],
                                        "field_reasons": r["metrics"]["field_reasons"]} for r in high],
            "replay_evidence_frames": sum(bool(r["replay_evidence"]) for r in rows),
            "limits": ["No per-frame packet/palette/raster replay evidence in schema 1",
                       "Refresh is inclusive; do not sum it with nested CPU stages",
                       "GPU metrics valid only where gpu_evidence is true",
                       "Unfinished rows are retained, not zero-cost samples"]}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("trace")
    args = p.parse_args()
    with open(args.trace, encoding="utf-8") as stream:
        rows = [json.loads(line) for line in stream if line.strip()]
    print(json.dumps(summarize(rows), indent=2))


if __name__ == "__main__":
    main()
