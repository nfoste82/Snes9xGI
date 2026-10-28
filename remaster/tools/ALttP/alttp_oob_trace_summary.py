#!/usr/bin/env python3
"""Summarize a dungeon_oob_trace_probe run, without choosing a sheet datum.

All indices are complete-room BG-local cells. An admitted exposure join is
reported as such, not upgraded to ROM assembly/gameplay-plane ownership proof.
"""
import argparse
import collections
import json
from pathlib import Path


def records(prefix, suffix):
    with Path(str(prefix) + suffix).open() as stream:
        return [json.loads(line) for line in stream]


def summarize(prefix):
    events = records(prefix, ".oob-events.jsonl")
    terminals = records(prefix, ".oob-terminals.jsonl")
    joins = records(prefix, ".oob-joins.jsonl")
    equations = records(prefix, ".wall-equations.jsonl")
    cells = records(prefix, ".oob-cells.jsonl")
    adjacency = collections.defaultdict(set)
    for event in events:
        kind, _, a, b, _, status, *_ = event
        if kind == 0 and status:
            adjacency[a].add(b)
    accepted = collections.defaultdict(list)
    for event in events:
        if event[0] == 1 and event[5] == 1:
            accepted[event[1]].append(event)

    def path(a, b):
        queue, previous = [a], {a: None}
        for node in queue:
            if node == b:
                break
            for peer in sorted(adjacency[node]):
                if peer not in previous:
                    previous[peer] = node
                    queue.append(peer)
        if b not in previous:
            return None
        result = []
        while b is not None:
            result.append([b // 4096, b % 64, b % 4096 // 64])
            b = previous[b]
        return result[::-1]

    contradictions = []
    ranges = []
    for component, candidates in sorted(accepted.items()):
        heights = [e[6] for e in candidates]
        edges = [t for t in terminals if len(t) >= 32 and t[18] == 1 and
                 any(t[0] * 4096 + t[1] == e[3] and
                     t[2] * 4096 + t[3] == e[2] and t[4] == (e[4] ^ 1)
                     for e in candidates)]
        samples = [h for t in edges for h in t[24:32] if h != -2147483648]
        ranges.append({"component": component, "minimum": min(heights),
                       "maximum": max(samples) if samples else max(heights),
                       "accepted_callback_count": len(heights),
                       "accepted_edge_sample_count": len(samples),
                       "nonfatal_height_disagreement": min(heights) != (max(samples) if samples else max(heights)),
                       "floor_owners": sorted({e[7] for e in candidates}),
                       "planes": sorted({e[8] for e in candidates})})
        first = candidates[0]
        for candidate in candidates[1:]:
            if candidate[6:9] != first[6:9]:
                contradictions.append({
                    "component": component, "first": first,
                    "different_accepted_terminal": candidate,
                    "fatal_under_minimum_policy": first[8] != candidate[8],
                    "same_floor_owner_and_plane": first[7:9] == candidate[7:9],
                    "admitted_cardinal_path": path(first[2], candidate[2]),
                })
                break
    statuses = collections.Counter(e[5] for e in events if e[0] == 3)
    return {
        "cell_denominator": len(cells),
        "role_counts_by_bg": [dict(collections.Counter(c["surface"] for c in cells if c["bg"] == bg)) for bg in range(2)],
        "oob_placement_status_counts": dict(statuses),
        "join_attempts": len(joins), "admitted_join_attempts": sum(j[5] for j in joins),
        "terminal_callbacks": sum(e[0] == 1 for e in events),
        "wall_terminal_callbacks": len(terminals),
        "wall_terminal_reason_counts": dict(collections.Counter(t[5] for t in terminals)),
        "accepted_crest_counts": dict(collections.Counter(str(e[6:9]) for e in events if e[0] == 1 and e[5] == 1)),
        "poison_events": [e for e in events if e[0] == 2],
        "independently_accepted_contradictions": contradictions,
        "accepted_crest_ranges": ranges,
        "invalid_wall_nodes": sum(e[5] for e in equations),
        "cyclic_wall_nodes": sum(e[6] for e in equations),
        "inconsistent_floor_equation_nodes": sum(e[7] for e in equations),
        "limitations": ["Exposure joins do not attest sheet gameplay plane or writer assembly.",
                        "No captured writer dispatch/last-writer history is available.",
                        "No repair or whole-room approval follows from this diagnostic."],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.prefix), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
