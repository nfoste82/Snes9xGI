#!/usr/bin/env python3
"""Offline bounded live-trace summary; never prints source host paths."""
import argparse
import json
import math
import statistics


def chrome_trace(rows):
    """Overlapping pipeline spans, not a fictitious sum of CPU and GPU work."""
    events = []

    def span(name, thread, start, duration, frame):
        if start > 0 and duration >= 0:
            events.append(dict(name=name, ph="X", pid=1, tid=thread,
                ts=start*1e6, dur=duration*1000, args={"frame_id": frame}))

    for row in rows:
        frame, metrics = row["frame_id"], row["metrics"]
        if row.get("refresh_complete"):
            span("Refresh", "Emulator", row.get("refresh_start_monotonic", 0),
                 row["refresh_inclusive_ms"], frame)
        if row.get("pacing_complete"):
            start = row.get("pacing_start_monotonic", 0)
            span("Audio sync", "Emulator", start, row["audio_sync_ms"], frame)
            span("Frame limiter", "Emulator", start + row["audio_sync_ms"]/1000,
                 row["frame_limiter_ms"], frame)
        if row.get("gpu_evidence"):
            span("GPU frame", "GPU (host uptime)", metrics.get("gpu_start_uptime", 0),
                 metrics.get("gpu_ms", 0), frame)
        if row.get("present_complete"):
            events.append(dict(name="Presented", ph="i", s="t", pid=1,
                tid="Display (host uptime)", ts=row["presented_uptime"]*1e6,
                args={"frame_id": frame}))
    return {"traceEvents": events, "displayTimeUnit": "ms",
            "otherData": {"clock_note": "CPU uses steady_clock; GPU/display use host uptime. Compare intervals within each clock, not absolute CPU/GPU offsets."}}

REASONS = {1: "forced", 2: "dimensions", 4: "scene_metadata", 8: "material",
           16: "content_geometry", 32: "emission_metadata", 64: "instance_identity",
           128: "context", 256: "palette", 512: "raster", 1024: "composition_depth",
            2048: "normal_mask", 4096: "cold"}

ROOM_REASONS = dict(enumerate(("room", "snapshot_invalid", "profile", "producer",
    "rom", "entry", "plane", "collision", "maps", "attributes", "graphics",
    "templates", "stairs", "graphics_base", "missing_classification", "research",
    "background", "verified", "unpublishable")))


def statistics_ms(values):
    values = sorted(values)
    if not values:
        return None
    return {"samples": len(values), "p50": statistics.median(values),
            "p95": values[math.ceil(len(values) * .95) - 1],
            "p99": values[math.ceil(len(values) * .99) - 1], "max": values[-1]}


def summarize(rows):
    if len(rows) > 600:
        raise ValueError("trace exceeds bounded schema")
    ids = [r["frame_id"] for r in rows]
    if len(set(ids)) != len(ids):
        raise ValueError("duplicate frame IDs")
    if any(r["schema"] not in (1, 2) or r["metrics"]["frame_id"] != r["frame_id"] for r in rows):
        raise ValueError("schema/lineage mismatch")
    complete = [r for r in rows if r["refresh_complete"]]
    times = sorted(r["refresh_inclusive_ms"] for r in complete)
    reasons = {name: sum(bool(r["metrics"]["field_reasons"] & bit) for r in rows)
               for bit, name in REASONS.items()}
    high = sorted(complete, key=lambda r: r["refresh_inclusive_ms"], reverse=True)[:10]
    presented = sorted(r["presented_uptime"] for r in rows
                       if r.get("present_complete") and r.get("presented_uptime", 0) > 0)
    gaps = [(b-a)*1000 for a, b in zip(presented, presented[1:]) if b > a]
    stages = sorted({s for r in rows for s in r["metrics"].get("exclusive_ms", {})})
    result = {"schema": 2, "frames": len(rows), "refresh_complete": len(complete),
             "states": {s: sum(r["state"] == s for r in rows)
                        for s in ("completed", "slot_drop", "unfinished", "emulator_skip", "presentation_failed", "gpu_failed")},
            "refresh_p50_ms": statistics.median(times) if times else None,
            "refresh_p95_ms": times[math.ceil(len(times) * .95) - 1] if times else None,
            "reason_frame_counts": reasons,
            "highest_refresh_frames": [{"frame_id": r["frame_id"],
                                        "emulated_frame": r["emulated_frame"],
                                         "refresh_ms": r["refresh_inclusive_ms"],
                                         "exclusive_ms": r["metrics"]["exclusive_ms"],
                                         "room_reasons": [name for bit, name in ROOM_REASONS.items()
                                             if r["metrics"].get("room_reasons", 0) & (1 << bit)],
                                         "classification_reasons": [name for bit, name in ROOM_REASONS.items()
                                             if r["metrics"].get("classification_reasons", 0) & (1 << bit)],
                                         "calls": r["metrics"].get("calls", {}),
                                         "field_reasons": r["metrics"]["field_reasons"]} for r in high],
            "replay_evidence_frames": sum(bool(r["replay_evidence"]) for r in rows),
             "limits": ["No per-frame packet/palette/raster replay evidence",
                        "Refresh is inclusive; do not sum it with nested CPU stages",
                        "Elapsed CPU scopes include scheduling/preemption; not thread CPU time",
                        "Presentation statistics cover only callbacks captured inside this window",
                        "GPU metrics valid only where gpu_evidence is true",
                        "Unfinished rows are retained, not zero-cost samples"]}
    result["presentation_gap_ms"] = statistics_ms(gaps)
    result["presentation_interval_fps"] = ((len(presented)-1)/(presented[-1]-presented[0])
        if len(presented)>1 and presented[-1]>presented[0] else None)
    result["presentation_hitches"] = {str(ms): sum(g > ms for g in gaps) for ms in (25, 50, 100)}
    for key, flag in (("audio_sync_ms", "pacing_complete"), ("frame_limiter_ms", "pacing_complete"),
                      ("pacing_total_ms", "pacing_complete"), ("frame_period_ms", "refresh_complete")):
        result[key] = statistics_ms([r[key] for r in rows if r.get(flag) and key in r
                                    and (key != "frame_period_ms" or r[key] > 0)])
    result["exclusive_stage_ms"] = {s: statistics_ms([r["metrics"]["exclusive_ms"].get(s, 0)
        for r in rows if r["metrics"].get("calls", {}).get(s, 0)]) for s in stages}
    result["stage_calls"] = {s: sum(r["metrics"].get("calls", {}).get(s, 0) for r in rows)
                             for s in stages}
    successful = [r for r in rows if r.get("gpu_evidence") and r["metrics"].get("status") == 4]
    result["successful_gpu_ms"] = statistics_ms([r["metrics"]["gpu_ms"] for r in successful])
    result["queue_ms"] = statistics_ms([r["metrics"]["queue_ms"] for r in successful])
    result["drawable_ms"] = statistics_ms([r["metrics"]["drawable_ms"] for r in successful])
    result["classification_outcomes"] = {name: sum(r["metrics"].get("classification_outcome", 0) == value
        for r in rows) for value, name in enumerate(("not_considered", "reused", "rebuilt"))}
    result["room_reason_frame_counts"] = {name: sum(bool(r["metrics"].get("room_reasons", 0) & (1 << bit))
        for r in rows) for bit, name in ROOM_REASONS.items()}
    result["classification_reason_frame_counts"] = {name: sum(bool(r["metrics"].get("classification_reasons", 0) & (1 << bit))
        for r in rows) for bit, name in ROOM_REASONS.items()}
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("trace")
    p.add_argument("--chrome", help="Also export timeline JSON for Perfetto/Chrome trace viewer")
    args = p.parse_args()
    with open(args.trace, encoding="utf-8") as stream:
        rows = [json.loads(line) for line in stream if line.strip()]
    print(json.dumps(summarize(rows), indent=2))
    if args.chrome:
        with open(args.chrome, "x", encoding="utf-8") as out:
            json.dump(chrome_trace(rows), out)


if __name__ == "__main__":
    main()
