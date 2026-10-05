#!/usr/bin/env python3
"""Deterministic, conservative surface constraints for placed ALTTP BG tiles.

This is an offline audit/solver, not a source of automatically trusted live maps.
Rules describe verified contacts; missing evidence produces unknown, never zero.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
import hashlib
import json
from pathlib import Path
from typing import Any


DIRECTIONS = {"north": (0, -1, "south"), "south": (0, 1, "north"),
              "east": (1, 0, "west"), "west": (-1, 0, "east")}
FULL_MASK = (1 << 64) - 1


def _mask(value: str) -> int:
    if not isinstance(value, str) or not value.startswith("0x"):
        raise ValueError("region mask must be a hexadecimal string")
    mask = int(value, 16)
    if not 0 < mask <= FULL_MASK:
        raise ValueError("region mask must select pixels in one 8x8 tile")
    return mask


def _integer(value: Any, name: str) -> int:
    if type(value) is not int:
        raise ValueError(f"{name} must be an integer")
    return value


def _flipped(values: list[int], h: bool, v: bool) -> list[int]:
    return [values[(7 - y if v else y) * 8 + (7 - x if h else x)]
            for y in range(8) for x in range(8)]


def _region(template: dict, placement: dict) -> dict:
    if not isinstance(template, dict) or set(template) != {"id", "role", "mask", "height", "contacts"}:
        raise ValueError("region requires id, role, mask, height, and contacts")
    height = template["height"]
    if type(height) is int:
        height = [height] * 64
    if not isinstance(height, list) or len(height) != 64 or any(type(v) is not int or not 0 <= v <= 255 for v in height):
        raise ValueError("local height must be an integer or 64 integer samples")
    mask = _flipped([bool(_mask(template["mask"]) & (1 << i)) for i in range(64)],
                    placement["hflip"], placement["vflip"])
    contacts = {}
    if not isinstance(template["contacts"], dict):
        raise ValueError("contacts must be an object")
    for direction, spec in template["contacts"].items():
        if direction not in DIRECTIONS or not isinstance(spec, dict) or set(spec) != {"role", "rise", "optional"}:
            raise ValueError("contact needs a cardinal edge, target role, rise, and optional flag")
        if not isinstance(spec["role"], str) or not spec["role"] or type(spec["optional"]) is not bool:
            raise ValueError("invalid contact role or optional flag")
        _integer(spec["rise"], "contact rise")
        dx, dy, _ = DIRECTIONS[direction]
        actual = ("east" if dx == -1 else "west" if dx == 1 else direction) if placement["hflip"] else direction
        if placement["vflip"]:
            actual = {"north": "south", "south": "north"}.get(actual, actual)
        contacts[actual] = spec
    return {"id": template["id"], "role": template["role"], "mask": mask,
            "height": _flipped(height, placement["hflip"], placement["vflip"]),
            "contacts": contacts}


def _edge_pairs(a: dict, b: dict, direction: str) -> list[tuple[int, int]]:
    dx, dy, _ = DIRECTIONS[direction]
    pairs = []
    for i in range(8):
        ax, ay = (7 if dx == 1 else 0, i) if dx else (i, 7 if dy == 1 else 0)
        bx, by = (0 if dx == 1 else 7, i) if dx else (i, 0 if dy == 1 else 7)
        ai, bi = ay * 8 + ax, by * 8 + bx
        if a["mask"][ai] and b["mask"][bi]:
            pairs.append((ai, bi))
    return pairs


def _landing_node(at_position: dict, nodes: dict, layer: int, x: int, y: int) -> tuple | None:
    """A stair endpoint is usable only with exactly one flat, classified surface."""
    candidates = [node for node in at_position[(layer, x, y)]
                  if nodes[node]["role"] == "floor" and nodes[node]["mask"] == [True] * 64 and
                  len(set(nodes[node]["height"])) == 1]
    return candidates[0] if len(candidates) == 1 else None


def solve(scene: dict, rules: dict) -> dict:
    """Solve placement-region offsets. All input iteration orders are canonicalized."""
    if scene.get("format") != "alttp-surface-scene-v1" or rules.get("format") != "alttp-surface-rules-v1":
        raise ValueError("unsupported scene or rule version")
    width, height = (_integer(scene.get(key), key) for key in ("width", "height"))
    if not 0 < width <= 64 or not 0 < height <= 64:
        raise ValueError("scene dimensions must fit a dungeon map")
    templates = {}
    by_hash = defaultdict(list)
    if not isinstance(rules.get("templates"), list):
        raise ValueError("rules require a templates list")
    for template in rules.get("templates", []):
        if not isinstance(template, dict) or set(template) != {"id", "tile_hash", "regions"} or not template["regions"]:
            raise ValueError("each template needs an id, tile_hash, and regions")
        key = template["id"]
        if not isinstance(key, str) or not key or key in templates or not isinstance(template["tile_hash"], str):
            raise ValueError("duplicate or invalid tile template id")
        if not isinstance(template["regions"], list):
            raise ValueError("template regions must be a list")
        templates[key] = template
        by_hash[template["tile_hash"]].append(template)
    if not isinstance(scene.get("placements"), list):
        raise ValueError("placements must be a list")
    placed = {}
    nodes = {}
    audit = []
    for cell in scene.get("placements", []):
        if not isinstance(cell, dict) or not {"layer", "x", "y", "tile_hash", "hflip", "vflip", "writer"} <= set(cell) or set(cell) - {
                "layer", "x", "y", "tile_hash", "hflip", "vflip", "writer", "template_id"}:
            raise ValueError("placement requires layer, x, y, tile_hash, flips, writer, optional template_id")
        layer, x, y = (_integer(cell[k], k) for k in ("layer", "x", "y"))
        if layer not in (0, 1) or not (0 <= x < width and 0 <= y < height):
            raise ValueError("placement is outside the BG map")
        if type(cell["hflip"]) is not bool or type(cell["vflip"]) is not bool:
            raise ValueError("placement flips must be booleans")
        key = layer, x, y
        if key in placed:
            raise ValueError("duplicate BG placement")
        placed[key] = cell
    if len(placed) != 2 * width * height:
        raise ValueError("scene must account for every BG1 and BG2 placement")
    if "anchors" not in scene or not isinstance(scene["anchors"], list):
        raise ValueError("scene requires an explicit anchors list")
    for key in sorted(placed):
        cell = placed[key]
        if "template_id" in cell:
            template = templates.get(cell["template_id"])
            if template is None or template["tile_hash"] != cell["tile_hash"]:
                raise ValueError("placement template_id does not match its tile_hash")
        else:
            candidates = by_hash[cell["tile_hash"]]
            template = candidates[0] if len(candidates) == 1 else None
        if template is None:
            audit.append({"at": list(key), "status": "unknown", "reason":
                          "ambiguous_template_usage" if len(by_hash[cell["tile_hash"]]) > 1 else
                          "no_semantic_template", "writer": cell["writer"]})
            continue
        used = [False] * 64
        seen_ids = set()
        for spec in template["regions"]:
            region = _region(spec, cell)
            if (not isinstance(region["id"], str) or not region["id"] or
                    not isinstance(region["role"], str) or not region["role"] or
                    region["id"] in seen_ids):
                raise ValueError("region ids and roles must be unique nonempty strings")
            seen_ids.add(region["id"])
            if any(a and b for a, b in zip(used, region["mask"])):
                raise ValueError("overlapping regions in template")
            used = [a or b for a, b in zip(used, region["mask"])]
            node = (*key, region["id"])
            nodes[node] = region
            audit.append({"at": list(key), "region": region["id"], "role": region["role"],
                          "status": "pending", "rule": template["id"], "writer": cell["writer"]})
        if not all(used):
            audit.append({"at": list(key), "status": "unknown", "reason": "unclassified_pixels",
                          "pixels": 64 - sum(used), "writer": cell["writer"]})

    at_position = defaultdict(list)
    for node in sorted(nodes):
        at_position[node[:3]].append(node)
    graph = defaultdict(list)
    issues = []
    contact_count = 0
    unmatched = []
    for node in sorted(nodes):
        layer, x, y, _ = node
        region = nodes[node]
        if region["role"] == "excluded":
            if region["contacts"]:
                raise ValueError("excluded regions cannot assert geometry contacts")
            continue
        for direction, spec in sorted(region["contacts"].items()):
            dx, dy, opposite = DIRECTIONS[direction]
            target_key = (layer, x + dx, y + dy)
            matches = []
            incompatible = []
            for target in at_position[target_key]:
                other = nodes[target]
                if other["role"] != spec["role"]:
                    continue
                pairs = _edge_pairs(region, other, direction)
                if not pairs:
                    continue
                reciprocal = other["contacts"].get(opposite)
                if (reciprocal is None or
                        reciprocal["role"] != region["role"] or
                        reciprocal["rise"] != -spec["rise"]):
                    incompatible.append(target)
                    continue
                matches.append((target, pairs))
            if len(matches) != 1:
                if incompatible or not spec["optional"] or len(matches) > 1:
                    unmatched.append({"node": list(node), "edge": direction,
                                      "reason": "incompatible_contact" if incompatible else
                                      "ambiguous_contact" if matches else "missing_contact",
                                      "candidates": [list(target) for target in incompatible]})
                continue
            target, pairs = matches[0]
            if node >= target:
                continue
            deltas = {region["height"][a] - nodes[target]["height"][b] + spec["rise"]
                      for a, b in pairs}
            if len(deltas) != 1:
                issue = {"node": list(node), "other": list(target), "edge": direction,
                         "reason": "inconsistent_pixel_heights", "deltas": sorted(deltas)}
                issues.append(issue)
                unmatched.append(issue)
                continue
            delta = next(iter(deltas))
            graph[node].append((target, delta))
            graph[target].append((node, -delta))
            contact_count += 1
    issues.extend(unmatched)

    # Stair handler orientation is independent of the doorway used to enter
    # the room. The two 8x8 landing samples on EACH side must be classified;
    # one surviving candidate is not silently substituted for a missing one.
    stair_constraints = []
    stairs = scene.get("stairs", [])
    if not isinstance(stairs, list):
        raise ValueError("stairs must be a list")
    seen_stairs = set()
    for stair in sorted(stairs, key=lambda entry: str(entry.get("id", ""))):
        if (not isinstance(stair, dict) or not {"id", "layer", "x", "y", "high_side", "rise"} <= set(stair) or
                set(stair) - {"id", "layer", "x", "y", "high_side", "rise", "writer"} or
                not isinstance(stair["id"], str) or not stair["id"] or stair["id"] in seen_stairs):
            raise ValueError("stair needs unique id, layer, x, y, high_side, rise")
        seen_stairs.add(stair["id"])
        layer = _integer(stair["layer"], "stair layer")
        x, y = _integer(stair["x"], "stair x"), _integer(stair["y"], "stair y")
        rise = _integer(stair["rise"], "stair rise")
        if (layer not in (0, 1) or stair["high_side"] not in ("north", "south") or
                not 0 <= x <= width - 4 or not 0 <= y <= height - 4 or rise <= 0):
            raise ValueError("unsupported stair footprint, orientation, or rise")
        footprint = [(layer, xx, yy) for yy in range(y, y + 4)
                     for xx in range(x, x + 4)]
        source_writer = stair.get("writer")
        if source_writer is not None:
            _integer(source_writer, "stair writer")
            surviving = [key for key in footprint if placed[key]["writer"] == source_writer]
            if not surviving:
                issues.append({"stair": stair["id"], "at": [layer, x, y],
                               "reason": "stair_writer_not_present"})
                continue
            if any(placed[key]["writer"] != source_writer and any(
                    nodes[node]["role"] == "stair" for node in at_position[key])
                    for key in footprint):
                issues.append({"stair": stair["id"], "at": [layer, x, y],
                               "reason": "overwritten_stair_region_requires_support_provenance"})
                continue
        missing_footprint = [list(key) for key in footprint
                             if not any(nodes[node]["role"] == "stair" for node in at_position[key])]
        if missing_footprint:
            issues.append({"stair": stair["id"], "at": [layer, x, y],
                           "reason": "unclassified_stair_footprint",
                           "cells": missing_footprint})
        sides = {side: [_landing_node(at_position, nodes, layer, xx, yy)
                        for xx in (x + 1, x + 2)]
                 for side, yy in (("north", y - 1), ("south", y + 4))}
        if missing_footprint or any(node is None for samples in sides.values() for node in samples):
            issues.append({"stair": stair["id"], "at": [layer, x, y],
                           "high_side": stair["high_side"], "reason": "unclassified_stair_landings"
                           if not missing_footprint else "stair_incomplete",
                           "north": [list(n) if n else None for n in sides["north"]],
                           "south": [list(n) if n else None for n in sides["south"]]})
            continue
        low_side = "south" if stair["high_side"] == "north" else "north"
        low, high = sides[low_side], sides[stair["high_side"]]
        if set(low) & set(high):
            issues.append({"stair": stair["id"], "at": [layer, x, y],
                           "reason": "ambiguous_stair_landings"})
            continue
        # Matched positions across the two-wide stair. This is a handler
        # relationship, not a visual-neighbor or actor-floor heuristic.
        for low_node, high_node in zip(low, high):
            lower_local = nodes[low_node]["height"][0]
            higher_local = nodes[high_node]["height"][0]
            delta = rise + lower_local - higher_local
            graph[low_node].append((high_node, delta))
            graph[high_node].append((low_node, -delta))
            stair_constraints.append({"stair": stair["id"], "lower": list(low_node),
                                      "upper": list(high_node), "rise": rise,
                                      "offset_delta": delta})
        # Once both landings and the entire assembly are classified, each
        # stair region inherits the lower landing's BASE (the authored ramp
        # supplies its local rise). A second stair can raise that base again.
        for key in footprint:
            for ramp_node in at_position[key]:
                if nodes[ramp_node]["role"] != "stair":
                    continue
                reference = low[0 if key[1] - x < 2 else 1]
                delta = nodes[reference]["height"][0]
                graph[reference].append((ramp_node, delta))
                graph[ramp_node].append((reference, -delta))
                stair_constraints.append({"stair": stair["id"], "lower": list(reference),
                                          "ramp": list(ramp_node), "offset_delta": delta})

    stair_constraints.sort(key=lambda item: (item["stair"], item["lower"],
                                             item.get("upper", item.get("ramp"))))

    anchors = {}
    anchor_issues = []
    for anchor in scene.get("anchors", []):
        if not isinstance(anchor, dict) or set(anchor) != {"layer", "x", "y", "region", "height", "evidence"} or not anchor["evidence"]:
            raise ValueError("anchors require an explicit region, height, and evidence")
        node = (_integer(anchor["layer"], "anchor layer"), _integer(anchor["x"], "anchor x"),
                _integer(anchor["y"], "anchor y"), anchor["region"])
        if node not in nodes or nodes[node]["role"] == "excluded":
            anchor_issues.append({"node": list(node), "reason": "unconsumed_anchor"})
            continue
        if node in anchors:
            raise ValueError("duplicate region anchor")
        anchors[node] = (_integer(anchor["height"], "anchor height"), anchor["evidence"])
    issues.extend(anchor_issues)

    results = {}
    visited = set()
    for start in sorted(nodes):
        if nodes[start]["role"] == "excluded":
            continue
        if start in visited:
            continue
        relative = {start: 0}
        queue = deque([start])
        conflict = False
        while queue:
            node = queue.popleft()
            visited.add(node)
            for target, delta in sorted(graph[node]):
                value = relative[node] + delta
                if target in relative and relative[target] != value:
                    conflict = True
                    issues.append({"node": list(node), "other": list(target), "reason": "contradictory_cycle"})
                elif target not in relative:
                    relative[target] = value
                    queue.append(target)
        bases = {value - relative[node] for node, (value, _) in anchors.items() if node in relative}
        if len(bases) > 1:
            conflict = True
            issues.append({"nodes": [list(n) for n in sorted(relative)], "reason": "conflicting_anchors"})
        base = next(iter(bases)) if len(bases) == 1 else None
        for node in relative:
            results[node] = {"status": "conflict" if conflict else "resolved" if base is not None else "relative_only",
                             "height_offset": None if conflict or base is None else base + relative[node],
                             "relative_offset": None if conflict else relative[node],
                             "group": list(start),
                             "anchor_evidence": sorted(str(anchors[n][1]) for n in relative if n in anchors)}

    invalid = {tuple(issue["node"]) for issue in issues if "node" in issue}
    invalid.update(tuple(issue["other"]) for issue in issues if "other" in issue)
    invalid.update(tuple(node) for issue in issues for node in issue.get("nodes", []))
    # A broken required contact invalidates its connected solution, not just
    # the one tile that happened to expose the missing evidence.
    invalid_groups = {tuple(results[node]["group"]) for node in invalid if node in results}
    out_of_range_groups = set()
    for node, result in results.items():
        if result["height_offset"] is not None and any(
                not 0 <= result["height_offset"] + nodes[node]["height"][i] <= 255
                for i in range(64) if nodes[node]["mask"][i]):
            issues.append({"node": list(node), "reason": "height_out_of_range"})
            out_of_range_groups.add(tuple(result["group"]))
    invalid_groups.update(out_of_range_groups)
    for item in audit:
        if item["status"] != "pending":
            continue
        node = (*item["at"], item["region"])
        if nodes[node]["role"] == "excluded":
            item["status"] = "explicitly_excluded"
            continue
        item.update(results[node])
        if tuple(results[node]["group"]) in invalid_groups and item["status"] != "conflict":
            item["status"] = "unknown"
            item["height_offset"] = None
    issues.sort(key=lambda issue: json.dumps(issue, sort_keys=True))
    unmatched.sort(key=lambda issue: json.dumps(issue, sort_keys=True))
    audit.sort(key=lambda item: (item["at"], item.get("region", ""), item["status"]))
    counts = dict(sorted(Counter(item["status"] for item in audit).items()))
    canonical_input = json.dumps({"scene": {**scene, "placements": sorted(scene["placements"],
                                       key=lambda p: (p["layer"], p["x"], p["y"])),
                                            "anchors": sorted(scene.get("anchors", []),
                                                            key=lambda a: (a["layer"], a["x"], a["y"], a["region"])),
                                            "stairs": sorted(stairs, key=lambda s: s["id"])},
                                  "rules": {**rules, "templates": sorted(rules.get("templates", []),
                                                                          key=lambda t: t["id"])}},
                                 sort_keys=True, separators=(",", ":"))
    return {"format": "alttp-surface-report-v1", "input_sha256": hashlib.sha256(canonical_input.encode()).hexdigest(),
            "size": [width, height], "placements": len(placed), "region_counts": counts,
            "contact_constraints": contact_count, "stair_constraints": stair_constraints,
            "unmatched_contacts": unmatched,
            "audit": audit, "issues": issues,
            "publishable": bool(counts.get("resolved")) and all(
                isinstance(scene.get(key), str) and bool(scene[key])
                for key in ("rom_sha256", "state_key", "geometry_evidence")) and not issues and not any(
                item["status"] in ("unknown", "conflict", "relative_only") for item in audit)}


def scene_from_room(snapshot: Any, provenance: Any) -> dict:
    """Adapt the existing checked linked export without guessing semantics."""
    return {"format": "alttp-surface-scene-v1", "width": 64, "height": 64,
            "room_id": snapshot.room,
            "tilemap_sha256": hashlib.sha256(b"".join(cell.word.to_bytes(2, "little")
                                                  for layer in snapshot.cells for cell in layer)).hexdigest(),
            "decoded_art_sha256": hashlib.sha256(b"".join(cell.tile_hash.encode("ascii")
                                                      for layer in snapshot.cells for cell in layer)).hexdigest(),
            "anchors": [],
            "placements": [{"layer": layer, "x": cell.x, "y": cell.y,
                            "tile_hash": cell.tile_hash, "hflip": cell.hflip, "vflip": cell.vflip,
                            "writer": provenance.owner(layer, cell.x, cell.y)}
                           for layer in range(2) for cell in snapshot.cells[layer]]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", type=Path, help="versioned full-room scene JSON")
    parser.add_argument("--raw", type=Path, help="ALTPPD1 linked capture (requires .prov)")
    parser.add_argument("--rules", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if (args.scene is None) == (args.raw is None):
        parser.error("provide exactly one of --scene and --raw")
    if args.raw:
        from alttp_room_geometry import RoomSnapshot, RoomProvenance
        snapshot = RoomSnapshot.read(args.raw)
        provenance = RoomProvenance.read(Path(str(args.raw) + ".prov"), snapshot)
        scene = scene_from_room(snapshot, provenance)
    else:
        scene = json.loads(args.scene.read_text())
    report = solve(scene, json.loads(args.rules.read_text()))
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
    print(f"{report['placements']} placements; {report['region_counts']}; "
          f"{len(report['issues'])} contact issues; publishable={report['publishable']}")


if __name__ == "__main__":
    main()
