#!/usr/bin/env python3
"""Game-independent, explicitly authored structural height fields. See STRUCTURAL-HEIGHT-FORMAT.md."""

import argparse
from array import array
from collections import deque
import json
import math
from pathlib import Path
import sys

from PIL import Image, ImageDraw

try:
    import numpy as np
except ImportError:
    np = None


class FormatError(ValueError):
    """Invalid authoring syntax, rather than an unsatisfied numerical constraint."""


def require(condition, message):
    if not condition:
        raise FormatError(message)


def number(value):
    require(isinstance(value, (int, float)) and not isinstance(value, bool)
            and math.isfinite(value), f"Expected finite number, got {value!r}")
    return float(value)


def point(value):
    require(isinstance(value, list) and len(value) == 2, "Expected [x,y]")
    return tuple(number(v) for v in value)


def pixel(value, size):
    x, y = point(value)
    require(x.is_integer() and y.is_integer(), "Contact coordinates must be integer pixels")
    require(0 <= x < size[0] and 0 <= y < size[1], "Contact pixel outside canvas")
    return int(x), int(y)


def mask_image(spec, size, base):
    """Union of filled polygons (each with local holes), optionally unioned with an image."""
    require(isinstance(spec, dict), "Geometry must be an object")
    require(set(spec) <= {"polygons", "mask"}, "Unknown geometry key")
    require(bool(spec.get("polygons")) or "mask" in spec, "Geometry needs polygons or mask")
    result = Image.new("L", size)
    if "mask" in spec:
        with Image.open(base / spec["mask"]) as source:
            require(source.size == size, "Mask dimensions must match scene")
            result = source.convert("L").point(lambda v: 255 if v else 0)
    for polygon in spec.get("polygons", []):
        require(set(polygon) <= {"outer", "holes"}, "Unknown polygon key")
        part = Image.new("L", size)
        draw = ImageDraw.Draw(part)
        for ring, fill in [(polygon["outer"], 255)] + [(h, 0) for h in polygon.get("holes", [])]:
            require(len(ring) >= 3, "Polygon rings need at least three vertices")
            coords = [point(p) for p in ring]
            require(all(v.is_integer() for p in coords for v in p), "Polygon vertices must be integer pixels")
            draw.polygon(coords, fill=fill)
        result.paste(255, mask=part)
    return result


def solve_nodes(scene):
    nodes = scene["nodes"]
    require(isinstance(nodes, dict) and nodes, "nodes must be a nonempty object")
    graph = {key: [] for key in nodes}
    for key, node in nodes.items():
        require(isinstance(node, dict) and set(node) <= {"anchor", "reason"}, f"Invalid node {key}")
        if "anchor" in node:
            number(node["anchor"])
    for i, relation in enumerate(scene.get("relations", [])):
        require(set(relation) <= {"from", "to", "kind", "amount", "scale", "reason"}, "Unknown relation key")
        a, b = relation["from"], relation["to"]
        require(a in nodes and b in nodes, "Relation references unknown node")
        kind = relation["kind"]
        require(kind in ("same", "rise"), "Relation kind must be same or rise")
        if kind == "same":
            require("amount" not in relation and "scale" not in relation, "same has no amount/scale")
            delta = 0.0
        else:
            delta = number(relation["amount"]) * number(relation["scale"])
            require(relation["scale"] > 0, "Relation scale must be positive")
            require(math.isfinite(delta), "Relation product overflows")
        graph[a].append((b, delta, i))
        graph[b].append((a, -delta, i))
    tolerance = number(scene.get("node_tolerance", 1e-8))
    require(tolerance >= 0, "node_tolerance must be nonnegative")
    values, states, components, conflicts = {}, {}, [], []
    visited = set()
    for root in nodes:
        if root in visited:
            continue
        offsets, queue, errors = {root: 0.0}, deque([root]), []
        visited.add(root)
        while queue:
            a = queue.popleft()
            for b, delta, relation_id in graph[a]:
                expected = offsets[a] + delta
                require(math.isfinite(expected), "Node offset overflows")
                if b not in offsets:
                    offsets[b] = expected
                    visited.add(b)
                    queue.append(b)
                elif abs(offsets[b] - expected) > tolerance:
                    errors.append({"kind": "relation", "relation": relation_id,
                                   "residual": offsets[b] - expected})
        anchors = [(key, number(nodes[key]["anchor"]) - offset)
                   for key, offset in offsets.items() if "anchor" in nodes[key]]
        origin = anchors[0][1] if anchors else None
        for key, candidate in anchors[1:]:
            if abs(candidate - origin) > tolerance:
                errors.append({"kind": "anchor", "node": key, "residual": candidate - origin})
        state = "conflict" if errors else "solved" if anchors else "unresolved"
        component_id = len(components)
        for key, offset in offsets.items():
            value = origin + offset if state == "solved" else None
            require(value is None or math.isfinite(value), "Solved node overflows")
            values[key] = value
            states[key] = {"status": state, "value": value, "component": component_id}
        components.append({"nodes": list(offsets), "status": state, "anchors": [a[0] for a in anchors]})
        conflicts.extend(dict(error, component=component_id) for error in errors)
    return values, states, components, conflicts


def expression(spec, nodes):
    """Compile expression once; callable accepts scalars or NumPy arrays. None means unresolved."""
    require(isinstance(spec, dict), "height expression must be an object")
    kind = spec["kind"]

    def node(key):
        require(key in nodes, f"Expression references unknown node {key}")
        return nodes[key]

    if kind == "flat":
        require(set(spec) == {"kind", "node"}, "flat requires kind,node")
        z = node(spec["node"])
        return None if z is None else lambda x, y: z
    if kind == "plane":
        require(set(spec) == {"kind", "node", "origin", "gradient"}, "plane requires node,origin,gradient")
        z, (ox, oy), (a, b) = node(spec["node"]), point(spec["origin"]), point(spec["gradient"])
        return None if z is None else lambda x, y: a * (x - ox) + b * (y - oy) + z
    if kind == "ramp":
        require(set(spec) == {"kind", "low", "high", "start", "axis", "span"}, "ramp requires low,high,start,axis,span")
        low, high = node(spec["low"]), node(spec["high"])
        sx, sy = point(spec["start"])
        ax, ay = point(spec["axis"])
        length = math.hypot(ax, ay)
        span = number(spec["span"])
        require(length > 0 and span > 0, "Ramp axis and span must be nonzero/positive")
        ax, ay = ax / length, ay / length
        if low is None or high is None:
            return None

        def ramp(x, y):
            t = ((x - sx) * ax + (y - sy) * ay) / span
            t = np.clip(t, 0, 1) if np is not None and isinstance(t, np.ndarray) else max(0, min(1, t))
            return low + (high - low) * t
        return ramp
    if kind in ("min", "max"):
        require(set(spec) == {"kind", "children"} and len(spec["children"]) >= 2, "min/max require at least two children")
        children = [expression(child, nodes) for child in spec["children"]]
        if any(child is None for child in children):
            return None

        def combine(x, y):
            values = [child(x, y) for child in children]
            if np is not None and any(isinstance(v, np.ndarray) for v in values):
                result = values[0]
                for value in values[1:]:
                    result = (np.minimum if kind == "min" else np.maximum)(result, value)
                return result
            return (min if kind == "min" else max)(values)
        return combine
    raise FormatError(f"Unknown expression kind {kind!r}")


def line_pixels(start, end, size):
    """Bresenham, inclusive endpoints and deterministic integer samples."""
    x, y = pixel(start, size)
    ex, ey = pixel(end, size)
    dx, dy = abs(ex - x), -abs(ey - y)
    sx, sy = (1 if x < ex else -1), (1 if y < ey else -1)
    error, result = dx + dy, []
    while True:
        result.append((x, y))
        if (x, y) == (ex, ey):
            return result
        twice = 2 * error
        if twice >= dy:
            error += dy
            x += sx
        if twice <= dx:
            error += dx
            y += sy


def solve_scene(scene, base_dir=Path(".")):
    require(isinstance(scene, dict), "Scene must be an object")
    require(set(scene) <= {"format", "width", "height", "height_unit", "height_range", "nodes",
                           "relations", "contacts", "node_tolerance", "domain", "surfaces"}, "Unknown scene key")
    require(scene.get("format") == "structural-height-1", "Expected format structural-height-1")
    require(isinstance(scene.get("height_unit"), str) and bool(scene["height_unit"].strip()), "height_unit is required")
    size = (scene["width"], scene["height"])
    require(all(isinstance(v, int) and not isinstance(v, bool) and v > 0 for v in size), "Invalid scene size")
    width, height = size
    count = width * height
    bounds = scene["height_range"]
    require(len(bounds) == 2, "height_range needs two numbers")
    lo, hi = map(number, bounds)
    require(hi > lo, "height_range must increase")
    values, node_states, components, node_conflicts = solve_nodes(scene)
    base_dir = Path(base_dir)
    domain = (mask_image(scene["domain"], size, base_dir) if "domain" in scene
              else Image.new("L", size, 255)).tobytes()
    surfaces = scene["surfaces"]
    require(isinstance(surfaces, list), "surfaces must be a list")
    require(len(surfaces) < 2**24, "Too many surface IDs for RGB output")
    ids, evaluators, summaries = {}, [], []
    owner = array("i", [-1]) * count
    priority = array("d", [-math.inf]) * count
    ambiguous = bytearray(count)
    for index, surface in enumerate(surfaces):
        require(set(surface) <= {"id", "type", "priority", "geometry", "height", "evidence", "reason", "confidence"}, "Unknown surface key")
        key = surface["id"]
        require(isinstance(key, str) and key and key not in ids, "Surface IDs must be unique nonempty strings")
        require(surface["type"] in ("flat", "wall", "transition", "corner"), "Unknown surface type")
        require(isinstance(surface["reason"], str) and bool(surface["reason"].strip()), "Surface reason required")
        require(isinstance(surface["evidence"], str) and bool(surface["evidence"].strip()), "Surface evidence required")
        confidence = number(surface["confidence"])
        require(0 <= confidence <= 1, "confidence must be in [0,1]")
        p = number(surface["priority"])
        ids[key] = index
        mask = mask_image(surface["geometry"], size, base_dir).tobytes()
        evaluator = expression(surface["height"], values)
        evaluators.append(evaluator)
        summaries.append({"id": key, "pixel_id": index + 1, "type": surface["type"],
                          "evidence": surface["evidence"], "reason": surface["reason"],
                          "confidence": confidence, "priority": p, "expression": surface["height"],
                          "height_status": "solved" if evaluator else "unresolved"})
        if np is not None:
            selected = (np.frombuffer(mask, dtype=np.uint8) != 0) & (np.frombuffer(domain, dtype=np.uint8) != 0)
            priorities = np.frombuffer(priority, dtype=np.float64)
            owners = np.frombuffer(owner, dtype=np.int32)
            ambiguity = np.frombuffer(ambiguous, dtype=np.uint8)
            higher = selected & (p > priorities)
            ambiguity[selected & (p == priorities)] = 1
            owners[higher], priorities[higher], ambiguity[higher] = index, p, 0
        else:
            for i, inside in enumerate(mask):
                if not inside or not domain[i]:
                    continue
                if p > priority[i]:
                    owner[i], priority[i], ambiguous[i] = index, p, 0
                elif p == priority[i]:
                    ambiguous[i] = 1

    heights = array("d", [math.nan]) * count
    coverage, confidence = bytearray(count), bytearray(count)
    out_of_range, nonfinite = 0, 0
    # Evaluate only selected pixels, so expressions never fill voids or hidden surfaces.
    owned = [[] for _ in surfaces]
    for i, selected in enumerate(owner):
        if selected >= 0 and not ambiguous[i]:
            owned[selected].append(i)
    for index, evaluator in enumerate(evaluators):
        indices = owned[index]
        summaries[index]["owned_pixels"] = len(indices)
        if evaluator is None or not indices:
            continue
        if np is not None:
            positions = np.asarray(indices)
            with np.errstate(over="ignore", invalid="ignore"):
                evaluated = np.broadcast_to(evaluator(positions % width, positions // width), positions.shape)
        else:
            evaluated = (evaluator(i % width, i // width) for i in indices)
        for i, z in zip(indices, evaluated):
            z = float(z)
            if not math.isfinite(z):
                nonfinite += 1
                continue
            heights[i] = z
            out_of_range += not lo <= z <= hi
            confidence[i] = round(255 * surfaces[index]["confidence"])
    counts = {"outside": 0, "gap": 0, "unresolved": 0, "overlap": 0, "solved": 0}
    for i in range(count):
        if not domain[i]:
            label, code = "outside", 0
        elif ambiguous[i]:
            label, code = "overlap", 192
        elif owner[i] < 0:
            label, code = "gap", 64
        elif math.isnan(heights[i]):
            label, code = "unresolved", 128
        else:
            label, code = "solved", 255
        coverage[i] = code
        counts[label] += 1

    # Enumerate visible four-neighbour interfaces once. Every pair needs an explicit contact.
    boundaries = {}
    for i, a in enumerate(owner):
        if a < 0 or ambiguous[i]:
            continue
        for j in ([i + 1] if i % width + 1 < width else []) + ([i + width] if i + width < count else []):
            b = owner[j]
            if b >= 0 and b != a and not ambiguous[j]:
                boundaries[(i, j)] = (a, b)
    covered, contact_reports = set(), []
    contact_ids = set()
    for contact in scene.get("contacts", []):
        require(set(contact) <= {"id", "a", "b", "tolerance", "max_distance", "delta", "pairs", "segments", "boundary"}, "Unknown contact key")
        key = contact["id"]
        require(isinstance(key, str) and key not in contact_ids, "Contact IDs must be unique strings")
        contact_ids.add(key)
        require(isinstance(contact.get("boundary", False), bool), "boundary must be boolean")
        require(contact["a"] in ids and contact["b"] in ids, "Contact references unknown surface")
        a, b = ids[contact["a"]], ids[contact["b"]]
        require(a != b, "Contact needs two different surfaces")
        tolerance = number(contact["tolerance"])
        distance = number(contact.get("max_distance", math.sqrt(2)))
        delta = number(contact.get("delta", 0))
        require(tolerance >= 0 and distance >= 0, "Contact tolerance/distance must be nonnegative")
        pairs = []
        if contact.get("boundary", False):
            for (i, j), (left, right) in boundaries.items():
                if {left, right} == {a, b}:
                    if left != a:
                        i, j = j, i
                    pairs.append(((i % width, i // width), (j % width, j // width)))
        for pair in contact.get("pairs", []):
            require(len(pair) == 2, "Contact pair needs two pixels")
            pairs.append((pixel(pair[0], size), pixel(pair[1], size)))
        for segment in contact.get("segments", []):
            require(set(segment) == {"a", "b"}, "Segment needs a,b endpoint pairs")
            aa, bb = segment["a"], segment["b"]
            require(len(aa) == len(bb) == 2, "Segment requires two endpoints per side")
            pa, pb = line_pixels(aa[0], aa[1], size), line_pixels(bb[0], bb[1], size)
            require(len(pa) == len(pb), "Paired segments must have equal sample counts")
            pairs.extend(zip(pa, pb))
        failed, examples, max_error = 0, [], 0.0
        for pa, pb in pairs:
            ia, ib = pa[1] * width + pa[0], pb[1] * width + pb[0]
            error = None
            reason = None
            if (owner[ia] != a or owner[ib] != b or ambiguous[ia] or ambiguous[ib]):
                reason = "wrong_or_ambiguous_owner"
            elif math.dist(pa, pb) > distance + 1e-9:
                reason = "not_spatial_seam"
            else:
                covered.add(tuple(sorted((ia, ib))))
                if math.isnan(heights[ia]) or math.isnan(heights[ib]):
                    reason = "unresolved_height"
                else:
                    error = abs((heights[ib] - heights[ia]) - delta)
                    max_error = max(max_error, error)
                    if error > tolerance:
                        reason = "height_mismatch"
            if reason:
                failed += 1
                if len(examples) < 20:
                    examples.append({"a": pa, "b": pb, "reason": reason, "error": error})
        contact_reports.append({"id": key, "samples": len(pairs), "failed_samples": failed,
                                "max_error": max_error, "empty": not pairs, "examples": examples})
    missing = [pair for pair in boundaries if pair not in covered]
    issues = []
    for label in ("gap", "unresolved", "overlap"):
        if counts[label]:
            issues.append({"kind": label, "pixels": counts[label]})
    unsolved_nodes = [key for key, state in node_states.items() if state["status"] != "solved"]
    if unsolved_nodes:
        issues.append({"kind": "unsolved_nodes", "nodes": unsolved_nodes})
    if missing:
        issues.append({"kind": "missing_contacts", "pixel_pairs": len(missing),
                       "examples": [{"a": [i % width, i // width], "b": [j % width, j // width],
                                     "surfaces": [surfaces[owner[i]]["id"], surfaces[owner[j]]["id"]]}
                                    for i, j in missing[:20]]})
    for contact in contact_reports:
        if contact["failed_samples"] or contact["empty"]:
            issues.append({"kind": "contact_failure", "id": contact["id"]})
    if out_of_range:
        issues.append({"kind": "display_range_exceeded", "pixels": out_of_range})
    if nonfinite:
        issues.append({"kind": "nonfinite_height", "pixels": nonfinite})
    report = {"format": "structural-height-report-1", "status": "review_incomplete" if issues else "complete",
              "width": width, "height": height, "height_unit": scene["height_unit"],
              "height_range": [lo, hi], "coverage": counts, "nodes": node_states,
              "node_components": components, "node_conflicts": node_conflicts,
              "surfaces": summaries, "contacts": contact_reports, "issues": issues,
              "boundary_pixel_pairs": len(boundaries), "missing_contact_pixel_pairs": len(missing),
              "backend": "numpy" if np is not None else "stdlib",
              "artwork_used_for_height": False}
    return {"report": report, "heights": heights, "coverage": coverage, "confidence": confidence,
            "owner": owner, "ambiguous": ambiguous}


def write_outputs(result, output_dir, artwork=None):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    report = result["report"]
    size = report["width"], report["height"]
    lo, hi = report["height_range"]
    gray, unknown, rgb = bytearray(), bytearray(), bytearray()
    numeric = array("d")
    for z, code, owner, ambiguous in zip(result["heights"], result["coverage"], result["owner"], result["ambiguous"]):
        solved = code == 255
        gray.append(1 + round(254 * max(0, min(1, (z - lo) / (hi - lo)))) if solved else 0)
        unknown.append(0 if solved else 255)
        numeric.append(z if solved else math.nan)
        index = owner + 1 if owner >= 0 and not ambiguous else 0
        rgb.extend((index & 255, (index >> 8) & 255, (index >> 16) & 255))
    for name, data in (("height", gray), ("confidence", result["confidence"]),
                       ("coverage", result["coverage"]), ("unknown", unknown)):
        Image.frombytes("L", size, bytes(data)).save(output_dir / f"{name}.png")
    Image.frombytes("RGB", size, bytes(rgb)).save(output_dir / "surfaceids.png")
    if sys.byteorder != "little":
        numeric.byteswap()
    (output_dir / "height.f64").write_bytes(numeric.tobytes())
    if artwork is not None:
        with Image.open(artwork) as source:
            require(source.size == size, "Artwork dimensions must match scene (no rescaling)")
            art = source.convert("RGB")
        comparison = Image.new("RGB", (size[0] * 2, size[1]))
        comparison.paste(art, (0, 0))
        comparison.paste(Image.frombytes("L", size, bytes(gray)).convert("RGB"), (size[0], 0))
        comparison.save(output_dir / "comparison.png")
    (output_dir / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path)
    parser.add_argument("--outputdir", type=Path, required=True)
    parser.add_argument("--artworkPNG", type=Path, help="Optional side-by-side comparison only")
    parser.add_argument("--allow-incomplete", action="store_true", help="Return success for review-incomplete scenes; artifacts/report remain incomplete")
    args = parser.parse_args(argv)
    try:
        scene = json.loads(args.scene.read_text())
        result = solve_scene(scene, args.scene.parent)
        write_outputs(result, args.outputdir, args.artworkPNG)
    except (FormatError, KeyError, TypeError, OSError, ValueError, OverflowError) as error:
        args.outputdir.mkdir(parents=True, exist_ok=True)
        report = {"format": "structural-height-report-1", "status": "invalid_input", "error": str(error)}
        (args.outputdir / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"Invalid input: {error}", file=sys.stderr)
        return 2
    report = result["report"]
    print(f"{report['status']}: {report['coverage']['solved']} solved pixels; {len(report['issues'])} issues; {args.outputdir / 'report.json'}")
    return 0 if report["status"] == "complete" or args.allow_incomplete else 1


if __name__ == "__main__":
    sys.exit(main())
