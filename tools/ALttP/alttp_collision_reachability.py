#!/usr/bin/env python3
"""Room-local, attribute-backed doorway and stair reachability review.

The doorway mode uses initialized collision planes and observed stair edges.
The legacy probe mode uses only plain attribute 00 and a full-body guard.
Output records sample positions, not visual floor support or elevations.
"""

from __future__ import annotations

import argparse
import base64
from collections import deque
import json
import hashlib
from pathlib import Path
import struct
import time


SIZE = 64
PIXELS = SIZE * 8
# tile_detect.c: kDetectTiles_tab0..3. Directions: north, south, west, east.
LEADING = (((0, 8), (8, 8), (15, 8)),
           ((8, 24), (0, 24), (15, 24)),
           ((0, 8), (0, 16), (0, 23)),
           ((15, 8), (15, 16), (15, 23)))
STEPS = ((0, -1), (0, 1), (-1, 0), (1, 0))


def stair_edges_from_trace(trace: list[dict], room: int) -> list[dict]:
    """Use observed whole staircase traversals, not assumed landing offsets."""
    edges = []
    start = None
    for previous, current in zip(trace, trace[1:]):
        if current.get("room", room) != room:
            raise ValueError("stair trace leaves requested room")
        if current["submodule"] in (8, 16) and previous["submodule"] == 0:
            start = previous
        if start is not None and current["submodule"] == 0:
            edges.append({"from": [start["plane"], start["x"], start["y"]],
                          "to": [current["plane"], current["x"], current["y"]],
                          "frames": current["frame"] - start["frame"],
                          "evidence": "reference_engine_stair_playback"})
            start = None
    return edges


def doorway_graph(planes: tuple[bytes, bytes], seed: tuple[int, int, int],
                  stairs: list[dict]) -> tuple[dict, list[set[tuple[int, int]]]]:
    """Pixel graph using Zelda3's three leading-edge samples, collision mode 0.

    Plain cells are traversable. Door attributes 80..8f allow only their
    documented doorway axis; all other behaviors are held at the boundary.
    Stair endpoints are directed, playback-verified edges, enabled on arrival.
    Plane numbers are gameplay state, not inferred visual elevations.
    """
    if len(planes) != 2 or any(len(p) != 4096 for p in planes):
        raise ValueError("graph requires two 64x64 collision planes")
    nodes = [seed] + [tuple(edge[key]) for edge in stairs for key in ("from", "to")]
    if any(p not in (0, 1) or not (0 <= x <= 496 and 0 <= y <= 487) for p, x, y in nodes):
        raise ValueError("seed/stair endpoint outside supported room bounds")
    links = {}
    for edge in stairs:
        links.setdefault(tuple(edge["from"]), []).append(tuple(edge["to"]))
    parents = {seed: None}
    queue = deque([seed])
    positions = [set(), set()]
    used = set()
    while queue:
        node = queue.popleft()
        plane, x, y = node
        positions[plane].add((x, y))
        for target in links.get(node, []):
            used.add((node, target))
            if target not in parents:
                parents[target] = node
                queue.append(target)
        for direction, ((dx, dy), samples) in enumerate(zip(STEPS, LEADING)):
            nx, ny = x + dx, y + dy
            target = (plane, nx, ny)
            if target in parents or not (0 <= nx <= 496 and 0 <= ny <= 487):
                continue
            values = [planes[plane][((ny + sy) // 8) * 64 + (nx + sx) // 8]
                      for sx, sy in samples]
            # Zelda3 door parity: even = north/south, odd = west/east.
            if not all(v == 0 or (0x80 <= v <= 0x8f and (v & 1) == (direction >= 2))
                       for v in values):
                continue
            # Do not leave a doorway sideways through samples of plain fill.
            center = planes[plane][((y + 16) // 8) * 64 + (x + 8) // 8]
            if 0x80 <= center <= 0x8f and (center & 1) != (direction >= 2):
                continue
            parents[target] = node
            queue.append(target)
    return {"seed": list(seed), "nodes": len(parents),
            "cardinal_edges": "unit-pixel leading-edge predicate in doorway_graph",
            "stair_edges": [{**edge, "reached": (tuple(edge["from"]), tuple(edge["to"])) in used}
                            for edge in stairs]}, positions


def floor_levels(positions: list[set[tuple[int, int]]], stairs: list[dict],
                 stair_records: list[dict]) -> dict:
    """Assign relative levels to cardinal components joined by oriented stairs."""
    component_at: list[dict[tuple[int, int], int]] = [{}, {}]
    component_positions: dict[int, tuple[int, set[tuple[int, int]]]] = {}
    next_component = 0
    for plane in range(2):
        remaining = set(positions[plane])
        while remaining:
            start = min(remaining, key=lambda point: (point[1], point[0]))
            remaining.remove(start)
            found = {start}
            queue = deque([start])
            while queue:
                x, y = queue.popleft()
                for dx, dy in STEPS:
                    neighbor = (x + dx, y + dy)
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        found.add(neighbor)
                        queue.append(neighbor)
            component_positions[next_component] = (plane, found)
            for point in found:
                component_at[plane][point] = next_component
            next_component += 1

    constraints = []
    unresolved = []
    for edge in stairs:
        source, target = tuple(edge["from"]), tuple(edge["to"])
        a = component_at[source[0]].get(source[1:])
        b = component_at[target[0]].get(target[1:])
        if a is None or b is None or a == b:
            unresolved.append({"edge": edge, "reason": "landing_component"})
            continue
        center_x = (source[1] + target[1]) / 2
        center_y = (source[2] + target[2]) / 2
        candidates = [record for record in stair_records
                      if record.get("high_side") in ("north", "south")]
        record = min(candidates, key=lambda item:
                     abs(item["tile_xy"][0] * 8 + 8 - center_x) +
                     abs(item["tile_xy"][1] * 8 + 12 - center_y), default=None)
        if record is None:
            unresolved.append({"edge": edge, "reason": "missing_orientation"})
            continue
        north_is_source = source[2] < target[2]
        high_is_source = north_is_source == (record["high_side"] == "north")
        low, high = (b, a) if high_is_source else (a, b)
        constraints.append((low, high, edge, record))

    graph: dict[int, list[tuple[int, int]]] = {}
    for low, high, _, _ in constraints:
        graph.setdefault(low, []).append((high, 1))
        graph.setdefault(high, []).append((low, -1))
    levels: dict[int, int] = {}
    conflicts = []
    for start in sorted(graph):
        if start in levels:
            continue
        relative = {start: 0}
        queue = deque([start])
        conflict = False
        while queue:
            current = queue.popleft()
            for neighbor, delta in graph[current]:
                proposed = relative[current] + delta
                if neighbor in relative:
                    conflict |= relative[neighbor] != proposed
                else:
                    relative[neighbor] = proposed
                    queue.append(neighbor)
        if conflict:
            conflicts.append(sorted(relative))
            continue
        base = min(relative.values())
        levels.update({component: level - base for component, level in relative.items()})

    cells: dict[tuple[int, int, int], int] = {}
    ambiguous = set()
    for component, level in levels.items():
        plane, found = component_positions[component]
        for x, y in body_cells(found, bytes(4096)):
            key = plane, x, y
            if key in cells and cells[key] != level:
                ambiguous.add(key)
            else:
                cells[key] = level
    for key in ambiguous:
        del cells[key]
    return {"unit": "floor_rise", "solved_components": len(levels),
            "constraints": [{"low_component": low, "high_component": high,
                             "edge": edge, "stair": record}
                            for low, high, edge, record in constraints],
            "cells_plane_x_y_level": [[plane, x, y, level]
                                      for (plane, x, y), level in sorted(cells.items())],
            "conflicting_components": conflicts, "unresolved_stairs": unresolved,
            "limits": ["Levels are relative within each stair-connected component graph.",
                       "Unsolved components retain authored tile heights.",
                       "Gameplay planes remain separate when room-local cells overlap."]}


def doorway_review(room: int, data: bytes, state: dict, trace: list[dict],
                   extra_traces: list[list[dict]] | None = None) -> dict:
    if (state.get("format") != "alttp-room-traversal-state-v1" or state["room_id"] != room or
            state["collision_mode"] != 0 or state["default_wall_attribute"] != 1):
        raise ValueError("graph requires initialized collision-mode-0 capture for matching room")
    seed = (state["plane"], *state["link_xy"])
    if not trace or (trace[0]["plane"], trace[0]["x"], trace[0]["y"]) != seed:
        raise ValueError("trace must begin at captured doorway seed")
    begin = time.perf_counter()
    planes = read_attributes(data, room)
    traces = [trace, *(extra_traces or [])]
    edges = []
    for item in traces:
        for edge in stair_edges_from_trace(item, room):
            if edge not in edges:
                edges.append(edge)
    graph, positions = doorway_graph(planes, seed, edges)
    elevation = floor_levels(positions, edges, state.get("stairs", []))
    checks = [frame for item in traces for frame in item if frame["submodule"] == 0]
    missed = [item for item in checks if (item["x"], item["y"]) not in positions[item["plane"]]]
    # Lossless row runs of graph nodes, keeping all pixel phases available for
    # subsequent path/footprint queries without an enormous per-node JSON list.
    runs = []
    for p in range(2):
        rows = {}
        for x, y in positions[p]:
            rows.setdefault(y, []).append(x)
        for y, row in sorted(rows.items()):
            xs = sorted(row)
            start = end = xs[0]
            for x in xs[1:]:
                if x != end + 1:
                    runs.append([p, y, start, end])
                    start = x
                end = x
            runs.append([p, y, start, end])
    graph["node_runs_plane_y_xmin_xmax"] = runs
    return {"format": "alttp-doorway-graph-review-v1", "room_id": room,
            "state": state, "graph": graph, "elevation": elevation,
            "seconds": time.perf_counter() - begin,
            "attribute_sha256": hashlib.sha256(data).hexdigest(),
            "playback_check": {"ordinary_positions": len(checks), "missing_positions": missed},
            "planes": [{"plane": f"gameplay plane {p}", "seed_pixels": [state["link_xy"]] if p == seed[0] else [],
                        "reachable_foot_positions": len(positions[p]),
                        "reachable_body_cells": body_cells(positions[p], planes[p]),
                        "south_center_cells": occupied_cells(positions[p])}
                       for p in range(2)],
            "limits": ["Static cardinal graph; no diagonal nudging, ledge hops, moving objects or state changes.",
                       "Only plain attributes and axis-aligned doors are modeled; other behaviors stop expansion.",
                       "Stair edges are observed reference-engine transitions, enabled only when reached.",
                       "Sample-position cells are not a visual tile/support mask; bridge occlusion is separate.",
                       "Reference-engine playback has not yet been compared with matching Snes9x gameplay."]}


def read_attributes(data: bytes, room: int) -> tuple[bytes, bytes]:
    if (len(data) != 10 + 2 * SIZE * SIZE or data[:8] != b"ALTPAT1\0" or
            struct.unpack_from("<H", data, 8)[0] != room):
        raise ValueError("collision attributes do not match room")
    return data[10:4106], data[4106:]


def plain_at(plane: bytes, x: int, y: int) -> bool:
    return 0 <= x < PIXELS and 0 <= y < PIXELS and plane[(y // 8) * SIZE + x // 8] == 0


def plain_footprint(plane: bytes, x: int, y: int) -> bool:
    """Require the sampled 16x24 body to be in the plain-attribute region."""
    return all(plain_at(plane, x + dx, y + dy)
               for dx in (0, 8, 15) for dy in (0, 8, 16, 23))


def reachable(plane: bytes, seeds: list[tuple[int, int]], stride: int = 1) -> set[tuple[int, int]]:
    """Traverse only transitions whose destination has a plain full footprint.

    The leading-edge samples are explicitly checked; the full footprint guard
    avoids claiming traversal through special attributes that the real engine
    can reach from a different starting state. All probes are room-local.
    """
    if stride < 1:
        raise ValueError("stride must be positive")
    invalid = [seed for seed in seeds if not plain_footprint(plane, *seed)]
    if invalid:
        raise ValueError(f"seed is not a plain Link-sized position: {invalid}")
    visited = set(seeds)
    queue = deque(seeds)
    while queue:
        x, y = queue.popleft()
        for (dx, dy), samples in zip(STEPS, LEADING):
            nx, ny = x + dx * stride, y + dy * stride
            node = (nx, ny)
            if node in visited or not all(plain_at(plane, nx + sx, ny + sy)
                                          for sx, sy in samples):
                continue
            if not plain_footprint(plane, nx, ny):
                continue
            visited.add(node)
            queue.append(node)
    return visited


def occupied_cells(positions: set[tuple[int, int]]) -> list[list[int]]:
    """South-center sample cells; no ground contact is implied."""
    return [list(cell) for cell in sorted({((x + 8) // 8, (y + 23) // 8)
                                          for x, y in positions})]


def body_cells(positions: set[tuple[int, int]], plane: bytes) -> list[list[int]]:
    """Nonblocking 8x8 cells touched by a reachable 16x16 collision body.

    Zelda3 samples movement from y+8 through y+23; sprite pixels above that
    range are visual and must not classify floor. Enumerating the four body
    corners is sufficient because cells are 8x8 and the body is 16x16.
    """
    cells = {(px // 8, py // 8) for x, y in positions
             for px in (x, x + 15) for py in (y + 8, y + 23)
             if plane[(py // 8) * SIZE + px // 8] == 0}
    return [list(cell) for cell in sorted(cells)]


def review(room: int, data: bytes, seeds_by_plane: list[list[tuple[int, int]]],
           stride: int = 1) -> dict:
    planes = read_attributes(data, room)
    results = []
    for label, plane, seeds in zip(("upper", "lower"), planes, seeds_by_plane):
        positions = reachable(plane, seeds, stride) if seeds else set()
        results.append({"plane": label, "seed_pixels": [list(seed) for seed in seeds],
                        "reachable_foot_positions": len(positions),
                        "reachable_body_cells": body_cells(positions, plane),
                        "south_center_cells": occupied_cells(positions)})
    return {"format": "alttp-collision-reachability-review-v1", "room_id": room,
            "state": "linked renderer default room state", "stride_pixels": stride,
            "planes": results,
            "limits": ["Seeds and actor-plane choice are probes, not validated entrance state.",
                       "Only attribute 00 with a plain full Link footprint is traversed.",
                       "Foot cells are potential sample positions, not verified Link occupancy or ground support.",
                       "Other attributes, doors, stairs, dynamic objects and room states are unresolved.",
                       "The renderer's collision planes have not been validated against gameplay movement."]}


def write_review_svg(path: Path, report: dict, background: bytes) -> None:
    source = "data:image/png;base64," + base64.b64encode(background).decode("ascii")
    lines = ['<svg xmlns="http://www.w3.org/2000/svg" width="1056" height="620" '
             'viewBox="0 0 1056 620" style="background:#151923;font-family:system-ui,sans-serif">',
             f'<text x="16" y="26" fill="white" font-size="18">Room 0x{report["room_id"]:03x}: '
             f'{"doorway + stair reachability" if "graph" in report else "plain-attribute diagnostic"} (collision body)</text>']
    for index, plane in enumerate(report["planes"]):
        left = 16 + index * 524
        lines.append(f'<text x="{left}" y="50" fill="white" font-size="14">'
                     f'{plane["plane"]}: {len(plane["reachable_body_cells"])} body-covered cells; '
                     f'{"doorway seed" if plane["seed_pixels"] else "via stairs"} {plane["seed_pixels"] or ""}</text>')
        lines.append(f'<image href="{source}" x="{left}" y="62" width="512" height="512"/>')
        for x, y in plane["reachable_body_cells"]:
            lines.append(f'<rect x="{left + x * 8}" y="{62 + y * 8}" width="8" height="8" '
                         f'fill="{"#49d3a2" if "graph" in report else "#f34fbb"}" opacity="0.34"><title>({x},{y}): '
                         f'reachable Link collision body covers this nonblocking cell; visual support unassigned</title></rect>')
        for x, y in plane["seed_pixels"]:
            lines.append(f'<circle cx="{left + x + 8}" cy="{62 + y + 23}" r="5" '
                         f'fill="white" stroke="black" stroke-width="2"/>')
    for edge in report.get("graph", {}).get("stair_edges", []):
        for key in ("from", "to"):
            p, x, y = edge[key]
            lines.append(f'<circle cx="{16 + p * 524 + x + 8}" cy="{62 + y + 23}" r="4" '
                         f'fill="#ffb44f"><title>Stair {key}; reached={edge["reached"]}</title></circle>')
    lines.append('<text x="16" y="603" fill="#cbd3dd" font-size="13">'
                 'Green: nonblocking cells covered by reachable collision body. Orange: stair endpoints. Plane is not visual height.</text>'
                 if "graph" in report else '<text x="16" y="603" fill="#ffb9dd" font-size="13">'
                 'Pink: restricted attribute connectivity, not verified ground.</text>')
    lines.append('</svg>')
    path.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--attributes", type=Path, required=True)
    parser.add_argument("--room", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--upper-seed", nargs=2, type=int, action="append", default=[], metavar=("X", "Y"))
    parser.add_argument("--lower-seed", nargs=2, type=int, action="append", default=[], metavar=("X", "Y"))
    parser.add_argument("--stride", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--background", type=Path, help="room-local 512x512 PNG for diagnostic SVG")
    parser.add_argument("--state", type=Path, help="doorway capture state; requires --stair-trace")
    parser.add_argument("--stair-trace", type=Path, help="reference-engine route JSONL observing stair traversals")
    parser.add_argument("--extra-stair-trace", type=Path, action="append", default=[],
                        help="additional route JSONL observing another reachable staircase")
    args = parser.parse_args()
    if bool(args.state) != bool(args.stair_trace):
        parser.error("--state and --stair-trace must be supplied together")
    result = (doorway_review(args.room, args.attributes.read_bytes(), json.loads(args.state.read_text()),
                            [json.loads(line) for line in args.stair_trace.read_text().splitlines()],
                            [[json.loads(line) for line in path.read_text().splitlines()]
                             for path in args.extra_stair_trace])
              if args.state else review(args.room, args.attributes.read_bytes(),
                    [list(map(tuple, args.upper_seed)), list(map(tuple, args.lower_seed))], args.stride))
    args.output.write_text(json.dumps(result, sort_keys=True, indent=2) + "\n")
    if args.background:
        write_review_svg(args.output.with_suffix(".svg"), result, args.background.read_bytes())
    for plane in result["planes"]:
        print(f"{plane['plane']}: {plane['reachable_foot_positions']} positions, "
              f"{len(plane['reachable_body_cells'])} body-covered cells")
    if "graph" in result:
        print(f"{result['seconds']:.3f}s; stairs: {result['graph']['stair_edges']}")


if __name__ == "__main__":
    main()
