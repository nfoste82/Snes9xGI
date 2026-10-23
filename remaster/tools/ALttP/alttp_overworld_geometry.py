#!/usr/bin/env python3
"""Complete outdoor input/candidate exporter (ALTPOW1, not dungeon ALTPPD1).

Base ROM expansion is reproducible but is NOT a captured gameplay state. Optional
ALTPOWS1 snapshots contain complete final Map16/Map8/writer grids; they do not turn
initial ROM graphics into verified live VRAM. No hash-global metadata is emitted.
The catalog/replay frontend can consume candidate.json and its occurrence records.
Run --help for commands. Outputs contain local ROM-derived data; do not commit.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
import hashlib
import json
from pathlib import Path
import subprocess

from alttp_atlas import Rom, SUPPORTED_US_SHA256
from alttp_atlas_graphics import extract_graphics
from alttp_atlas_overworld import extract_overworld, MAP16_TO_MAP8_TABLE
from alttp_atlas_overworld_gfx import BaseOverworldGraphics
from alttp_atlas_world import catalog_world

REVISION = "outdoor-core-1"
COLLISION_TABLE = 0x8E9459
SHARED_ART = "v1:4bpp:b3881650e2d5d461"
NORMAL = set((0, 5, 6, 7, 0x21, 0x41, 0x45, 0x47, 0x48, 0x49, 0x4a,
              0x5e, 0x5f, 0x60, 0x61, 0x62, 0x64, 0x65, 0x66, 0xbe, 0xbf))
NORMAL.update(range(0x14, 0x18)); NORMAL.update(range(0x23, 0x26))
NORMAL.update(range(0x38, 0x3d)); NORMAL.update(range(0x6c, 0x70))
NORMAL.update(range(0xd0, 0xf0))


def adjusted_attribute(raw, word):
    return raw | ((word >> 14) & 1) if 0x10 <= raw < 0x1c else raw


def behavior(a):
    """TileDetect_ExecuteInner outdoor behavior, NOT full legal Link movement."""
    if a in (1, 2, 3, 0x26, 0x43): return "solid"
    if 0x10 <= a <= 0x13 or 0x18 <= a <= 0x1b: return "slope"
    if a in (8, 9, 0xb): return "water"
    if a == 0x20 or 0xb0 <= a <= 0xbd: return "pit"
    if (a == 0xa or 0x1c <= a <= 0x1f or a == 0x22 or 0x28 <= a <= 0x37
            or 0x3d <= a <= 0x3f or a == 0x4b or 0x80 <= a <= 0xaf):
        return "transition"
    if a in NORMAL: return "normal"
    if (0x50 <= a <= 0x5d or a in (0x27, 0x42, 0x46, 0x63, 0x67)
            or 0x70 <= a <= 0x7f or a >= 0xc0): return "interactive"
    return "special"


def stable_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def validate_snapshot(snapshot, rom_sha, area, quadrants):
    """Reject partial/stale snapshots, silent writers, and invented base provenance."""
    if snapshot.get("format") != "ALTPOWS1" or snapshot.get("rom_sha256") != rom_sha:
        raise ValueError("snapshot format/ROM mismatch")
    if snapshot.get("area") != area or not snapshot.get("state_key"):
        raise ValueError("snapshot area/state mismatch")
    if not snapshot.get("source_sha256") or not snapshot.get("graphics_context"):
        raise ValueError("snapshot needs pinned source and graphics context")
    grids = snapshot.get("quadrants", [])
    if sorted(g.get("index", -1) for g in grids) != sorted(quadrants):
        raise ValueError("snapshot must cover each complete area quadrant exactly once")
    for g in grids:
        for key, count, maximum in (("map16", 1024, 3751), ("map8", 4096, 65535),
                                     ("writers", 4096, 0xffffffff)):
            v = g.get(key, [])
            if len(v) != count or any(type(x) is not int or not 0 <= x <= maximum for x in v):
                raise ValueError(f"incomplete/invalid snapshot {key}")
    return {g["index"]: g for g in grids}


def collision_components(placements):
    """Inventory-only point-cell contacts. NOT accepted movement/support edges.

    Does not cross water, slopes, transitions, unknowns, BGs, or support surfaces.
    In particular a normal tile is not a numeric height/flat artwork label.
    """
    cells = {(p["x"], p["y"], p["layer"], p["surface"]): i
             for i, p in enumerate(placements) if p["behavior"] == "normal"}
    seen = set(); components = []
    for root in sorted(cells):
        if root in seen: continue
        queue = deque([root]); seen.add(root); members = []
        while queue:
            x, y, bg, surface = queue.popleft()
            members.append(cells[x, y, bg, surface])
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                n = x + dx, y + dy, bg, surface
                if n in cells and n not in seen: seen.add(n); queue.append(n)
        cid = len(components)
        for i in members: placements[i]["collision_component"] = cid
        components.append({"id": cid, "members": len(members), "seed": list(root),
                           "status": "unresolved", "level": None,
                           "evidence": "normal-attribute adjacency only; actor footprint not reproduced"})
    return components


def export_area(rom, quadrants, resolver, area, snapshot=None):
    qs = resolver.area_quadrants(area)
    runtime = validate_snapshot(snapshot, rom.sha256, area, qs) if snapshot else {}
    attrs = rom.read_bytes(COLLISION_TABLE, 512)
    placements = []; family_groups = defaultdict(list)
    for qi in qs:
        q = quadrants[qi]; state = runtime.get(qi)
        for tile in q.iter_tiles():
            n = tile.y*64 + tile.x
            m16 = state["map16"][tile.map16_y*32 + tile.map16_x] if state else tile.map16_id
            word = state["map8"][n] if state else tile.word
            writer = state["writers"][n] if state else 0
            expected = rom.get_word(MAP16_TO_MAP8_TABLE + m16*8 + ((tile.y & 1)*2 + (tile.x & 1))*2)
            if state and word != expected and writer == 0:
                raise ValueError("final Map8 differs from selected Map16 without a later writer")
            if state and (m16 != tile.map16_id or word != tile.word) and writer == 0:
                raise ValueError("changed placement cannot retain ROM base writer provenance")
            # Re-resolve the final word, NOT the stale base tile word.
            from dataclasses import replace
            gfx = resolver.resolve_tile(area, replace(tile, word=word))
            raw = attrs[word & 0x1ff]; adjusted = adjusted_attribute(raw, word)
            original32 = not state or all(
                state["map16"][(tile.map32_y*2+dy)*32+tile.map32_x*2+dx] ==
                q.map16_ids[(tile.map32_y*2+dy)*32+tile.map32_x*2+dx]
                for dy in range(2) for dx in range(2))
            p = {"x": (qi & 7)*64 + tile.x, "y": ((qi & 63) >> 3)*64 + tile.y,
                 "layer": 0, "surface": 0, "quadrant": qi,
                 "map32": tile.map32_id if original32 else None,
                 "base_map32": tile.map32_id,
                 "map32_member": (tile.map16_y & 1)*2 + (tile.map16_x & 1),
                 "map16": m16, "map16_member": (tile.y & 1)*2 + (tile.x & 1),
                 "word": word, "writer": writer, "base_word": tile.word,
                 "map8_slot": word & 0x3ff, "collision_index": word & 0x1ff,
                 "raw_collision": raw, "collision": adjusted, "behavior": behavior(adjusted),
                 "flip_h": bool(word & 0x4000), "flip_v": bool(word & 0x8000),
                 "hash": gfx.content_id, "graphics_status": gfx.status,
                 "graphics_evidence": "base ROM loader only" if state else "base ROM loader",
                 "role": "unknown", "status": "unresolved", "base": None, "local": None,
                 "normals": None, "assembly": None, "support": None,
                 "geometry_source": None, "collision_component": None,
                 "missing_stage": "assembly_semantics"}
            placements.append(p)
            family_groups[m16].append(p)
    families = []
    for mid, members in sorted(family_groups.items()):
        words = [rom.get_word(MAP16_TO_MAP8_TABLE+mid*8+i*2) for i in range(4)]
        families.append({"map16": mid, "definition_address": MAP16_TO_MAP8_TABLE+mid*8,
                         "words": words, "raw_collision": [attrs[w & 0x1ff] for w in words],
                         "placements": len(members), "semantic_status": "needs_family_annotation",
                         "role": None, "evidence": "ROM definition, not semantic height evidence"})
    components = collision_components(placements)
    state_key = snapshot["state_key"] if snapshot else "base-ROM-only:no-overlays:no-events"
    pin = {"rom_sha256": rom.sha256, "area": area, "world": "dark" if area & 64 else "light",
           "quadrants": list(qs), "state_key": state_key,
           "graphics_context": snapshot["graphics_context"] if snapshot else f"initial-ROM-theme:{resolver.areas[area]['gfx_index']}",
           "metadata_revision": REVISION, "floor_rise": None,
           "snapshot_sha256": stable_digest(snapshot) if snapshot else None,
           "provenance_sha256": stable_digest([(p["word"], p["writer"], p["map16"]) for p in placements])}
    unsupported = ["calibrated reusable assembly semantics", "signed ground/terrace endpoint calibration",
                   "Link multi-point legal movement and special transitions", "verified obstacle support contacts",
                   "live VRAM/state and scroll mapping comparison", "linked candidate replay traversal"]
    if not snapshot: unsupported.insert(0, "runtime overlays, persistent edits and pinned gameplay state")
    return {"format": "ALTPOW1", "revision": REVISION, "pin": pin,
            "status": "blocked", "verdict": "blocked by missing evidence",
            "scope": {"complete_base_quadrants": True, "complete_gameplay_state": bool(snapshot),
                      "BG_layers": [0], "support_surfaces": [0],
                      "unsupported_layers": ["overlays/BG2", "overlapping support surfaces"]},
            "coverage": {"placements": len(placements), "solved": 0, "classified": 0,
                         "unresolved": len(placements), "conflicts": 0,
                         "empty": 0, "non_geometric": 0,
                         "by_behavior": dict(Counter(p["behavior"] for p in placements)),
                         "by_graphics": dict(Counter(p["graphics_status"] for p in placements))},
            "support_graph": {"components": components, "accepted_movement_edges": [],
                              "signed_constraints": [], "anchors": [],
                              "note": "point-cell inventory only; no inferred height or movement connectivity"},
            "families": families, "placements": placements,
            "collision_source": {"address": COLLISION_TABLE, "bytes": 512,
                                 "sha256": hashlib.sha256(attrs).hexdigest(),
                                 "shared_art_slots": {str(i): attrs[i] for i in (271, 347, 369)}},
            "blockers": unsupported,
            "capture_checks": {"matched": 0, "mismatched": 0, "unobserved": len(placements)}}


def review_summary(candidate):
    """Small parent-facing catalog/inspector input; never fabricates replay links."""
    ps = candidate["placements"]; pin = candidate["pin"]
    shared = [p for p in ps if p["hash"] == SHARED_ART]
    slots = Counter((p["map8_slot"], p["raw_collision"], p["collision"], p["behavior"]) for p in shared)
    x0, x1 = min(p["x"] for p in ps)*8, (max(p["x"] for p in ps)+1)*8
    y0, y1 = min(p["y"] for p in ps)*8, (max(p["y"] for p in ps)+1)*8
    # Coverage navigation is deterministic and overlapping. These are required
    # viewports, NOT synthetic screenshots or proof of captured coverage.
    viewports = []
    for y in range(y0, y1, 192):
        for x in range(x0, x1, 256):
            viewports.append({"x": min(x, x1-256), "y": min(y, y1-224),
                              "width": 256, "height": 224, "capture": None,
                              "status": "unsupported:no-linked-runtime-capture"})
    unique = {(v["x"], v["y"]): v for v in viewports}
    return {"format": "ALTPOWREVIEW1", "pin": pin, "generation_revision": REVISION,
            "candidate_sha256": stable_digest(candidate), "status": "blocked",
            "coverage": candidate["coverage"], "blockers": candidate["blockers"],
            "complete_area_pixel_bounds": [x0, y0, x1, y1],
            "required_viewports": list(unique.values()), "linked_runtime_captures": [],
            "shared_art": {"hash": SHARED_ART, "placements": len(shared),
                           "slots": [{"slot": s, "raw": r, "adjusted": a, "behavior": b, "count": n}
                                     for (s, r, a, b), n in sorted(slots.items())],
                           "map16_definitions": sorted({p["map16"] for p in shared})},
            "human_review": "not performed", "ready_for_review": False}


def validate_candidate(candidate):
    """Outdoor publication gate. A blocked candidate deliberately exits nonzero."""
    errors = []
    if candidate.get("format") != "ALTPOW1": return ["unsupported outdoor format"]
    ps = candidate["placements"]; pin = candidate["pin"]
    if pin["rom_sha256"] != SUPPORTED_US_SHA256: errors.append("unsupported ROM")
    expected = {(q, x, y) for q in pin["quadrants"] for y in range(64) for x in range(64)}
    actual = {(p["quadrant"], p["x"] % 64, p["y"] % 64) for p in ps}
    if actual != expected or len(ps) != len(expected): errors.append("incomplete/duplicate area grid")
    for p in ps:
        qi = p["quadrant"]
        if not ((qi & 7)*64 <= p["x"] < (qi & 7)*64+64 and
                ((qi & 63) >> 3)*64 <= p["y"] < ((qi & 63) >> 3)*64+64):
            errors.append("unstable quadrant/global map coordinate"); break
        if (p["collision"] != adjusted_attribute(p["raw_collision"], p["word"]) or
                p["behavior"] != behavior(p["collision"])):
            errors.append("collision lookup/transform mismatch"); break
    solved = sum(p["status"] == "solved" for p in ps)
    if solved != candidate["coverage"]["solved"]: errors.append("solved count mismatch")
    if candidate["coverage"]["placements"] != len(ps): errors.append("placement denominator mismatch")
    if any(p["status"] not in ("solved", "non_geometric", "empty") for p in ps):
        errors.append("missing/conflicting required placement geometry")
    for p in ps:
        if p["status"] == "solved" and (p["base"] is None or not p["geometry_source"] or
            not isinstance(p["local"], list) or len(p["local"]) != 64 or
            not isinstance(p["normals"], list) or len(p["normals"]) != 64 or
            p["role"] == "unknown"):
            errors.append("solved placement lacks effective geometry evidence"); break
    if candidate["blockers"]: errors.append("unresolved evidence blockers")
    if candidate["scope"].get("unsupported_layers"):
        errors.append("unobserved layers/support-surface denominator")
    if not candidate["scope"]["complete_gameplay_state"]: errors.append("no pinned complete gameplay state")
    if candidate["capture_checks"]["unobserved"] or candidate["capture_checks"]["mismatched"]:
        errors.append("complete matching runtime capture checks missing")
    return errors


ROLES = ("unknown", "floor", "wall", "cap", "corner", "obstacle", "transition", "empty", "non_geometric")
STATUSES = ("unresolved", "classified", "solved", "conflict", "overwritten", "non_geometric")
GEOMETRIES = ("flat", "face", "corner", "detail", "ramp", "sampled")


def apply_rules(candidate, rules, core):
    """Automatic exact family discovery and signed geometry through portable core.

    ALTPOWR1 is reusable metatile semantics, not a per-map rectangle/hash override.
    Support bindings are deliberately explicit evidence graph node IDs. Do not
    bind every occurrence of a family to the same terrace unless justified; leave
    bindings empty for unproved placements. Graph anchors/constraints are state
    pinned separately from intrinsic assembly definitions.
    """
    if rules.get("format") != "ALTPOWR1" or rules.get("pin") != candidate["pin"]:
        raise ValueError("rules require exact complete candidate input pin")
    families = rules.get("families", [])
    lines = ["OWCORE1"]
    pin = candidate["pin"]
    quote = lambda v: json.dumps(str(v), ensure_ascii=False)
    lines.append(" ".join(quote(pin[k]) for k in ("rom_sha256", "state_key", "graphics_context", "metadata_revision", "provenance_sha256")) + f" {pin['area']}")
    ps = candidate["placements"]; lines.append(str(len(ps)))
    for p in ps:
        lines.append(" ".join(str(p[k] if p[k] is not None else 65535) for k in
                     ("x", "y", "layer", "surface", "quadrant", "map32", "map16", "word",
                      "map32_member", "map16_member", "raw_collision", "writer")))
    graph = rules.get("support_graph", {})
    lines.append(str(graph.get("nodes", 0)))
    edges = graph.get("constraints", []); lines.append(str(len(edges)))
    for e in edges: lines.append(f"{e['from']} {e['to']} {e['rise']} {quote(e['evidence'])}")
    anchors = graph.get("anchors", []); lines.append(str(len(anchors)))
    for a in anchors: lines.append(f"{a['node']} {a['height']} {quote(a['evidence'])}")
    observations = graph.get("observations", []); lines.append(str(len(observations)))
    for o in observations:
        if not o.get("evidence"): raise ValueError("support observations require evidence")
        lines.append(f"{o['x']} {o['y']} {o['layer']} {o['surface']} {o['node']} {quote(o['evidence'])}")
    lines.append(str(len(families)))
    for f in families:
        if not f.get("evidence") or f.get("graphics_context") != pin["graphics_context"]:
            raise ValueError("family needs evidence and matching graphics context")
        # Global support binding is rejected: copies of a family may have different
        # bases. The first bridge discovers semantics only; state-specific contacts
        # need a separately proven ownership adapter rather than per-family levels.
        if f.get("supports"):
            raise ValueError("global family support bindings forbidden; supply placement contact adapter")
        lines.append(" ".join((quote(f["id"]), quote(f["evidence"]), quote(f["graphics_context"]),
                               str(f["width"]), str(f["height"]), str(int(f.get("allow_h", False))),
                               str(int(f.get("allow_v", False))))))
        contacts = f.get("contacts", []); lines.append(str(len(contacts)))
        for c in contacts: lines.append(f"{c['endpoint']} {c['x']} {c['y']}")
        lines.append("0") # no per-family/global support binding
        members = f["members"]; lines.append(str(len(members)))
        for m in members:
            if "hash" in m: raise ValueError("hash-global family semantics forbidden")
            vals = [m["x"], m["y"], m["map16"], m["word"], m["map16_member"],
                    ROLES.index(m["role"]), GEOMETRIES.index(m["geometry"]),
                    m.get("support", -1), m.get("low", -1), m.get("high", -1),
                    *m.get("normal", [0, 0]), *m.get("axis", [0, 1]),
                    m.get("start", 0), m.get("span", 1), m.get("corner_low", -1),
                    m.get("corner_high", -1), int(m.get("replaceable", False)), int(m.get('owns',True)),
                    *m.get("detail", [0]*64)]
            if len(m.get("detail", [0]*64)) != 64: raise ValueError("detail must contain 64 samples")
            lines.append(" ".join(map(str, vals)))
            if m['geometry']=='sampled':
                if len(m['progress'])!=64 or len(m['pixel_normals'])!=64:
                    raise ValueError('sampled assembly patch requires 64 progress and normal values')
                lines.append(' '.join(map(str,m['progress'])))
                lines.append(' '.join(str(v) for n in m['pixel_normals'] for v in n))
    run = subprocess.run([str(core)], input="\n".join(lines)+"\n", text=True,
                         capture_output=True, check=True)
    result = json.loads(run.stdout)
    rules_digest = stable_digest(rules)
    if len(result["placements"]) != len(ps): raise ValueError("core denominator mismatch")
    for p, r in zip(ps, result["placements"]):
        # Regeneration must clear old effective arrays even when new rules stop
        # matching or support becomes contradictory/unobserved.
        p.update(base=None, local=None, normals=None, geometry_source=None,
                 missing_stage="assembly_semantics")
        p["role"] = ROLES[r["role"]]; p["status"] = STATUSES[r["status"]]
        p["evidence"] = r["evidence"]
        p["assembly"] = r["assembly"] if r["assembly"] >= 0 else None
        p["support"] = r["support"] if r["support"] >= 0 else None
        if r["has_geometry"]:
            p["base"] = r["base"]; p["local"] = r["local"]; p["normals"] = r["normals"]
            p["geometry_source"] = "portable-core:" + rules_digest
            p["missing_stage"] = None
        elif p["assembly"] is not None: p["missing_stage"] = "support_ownership_or_projection"
    counts = Counter(p["status"] for p in ps)
    for k in ("solved", "classified", "unresolved", "conflicts", "empty", "non_geometric"):
        candidate["coverage"][k] = counts["conflict" if k == "conflicts" else k]
    candidate["rules_sha256"] = rules_digest
    candidate["automatic_assemblies"] = result["assemblies"]
    candidate['assemblies'] = result.get('assembly_records', [])
    candidate["support_graph"]["levels"] = result["levels"]
    return candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=Path.home()/"Downloads/Zelda_ALTTP.zip")
    parser.add_argument("--area", type=lambda x: int(x, 0), default=0x1b)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--snapshot", type=Path, help="complete ALTPOWS1 final-state adapter input")
    parser.add_argument("--audit-corpus", action="store_true", help="inventory all 160 base quadrants")
    parser.add_argument("--rules", type=Path, help="ALTPOWR1 reusable families (requires --core)")
    parser.add_argument("--core", type=Path, help="compiled alttp_overworld_core.cpp bridge")
    parser.add_argument("--review-summary", type=Path, help="parent catalog input with unsupported viewport markers")
    parser.add_argument("--castle", action="store_true", help="compile authored reusable castle curtain families")
    parser.add_argument("--castle-rise", type=int, default=32, help="declared authored scale, not measured gameplay rise")
    parser.add_argument("--write-rules", type=Path, help="write compiled authored semantics/contact graph")
    parser.add_argument("--validate", type=Path, help="validate candidate; nonzero while incomplete")
    args = parser.parse_args()
    if args.validate:
        errors = validate_candidate(json.loads(args.validate.read_text()))
        print(json.dumps({"ok": not errors, "errors": errors}, indent=2))
        return int(bool(errors))
    if args.output is None: parser.error("--output required")
    if not args.output.parent.is_dir(): parser.error("output parent must already exist")
    rom = Rom(args.rom)
    if rom.sha256 != SUPPORTED_US_SHA256: raise ValueError("unsupported ROM fingerprint")
    quadrants = extract_overworld(rom)
    resolver = BaseOverworldGraphics(extract_graphics(rom), catalog_world(rom)["overworld_areas"], quadrants)
    snapshot = json.loads(args.snapshot.read_text()) if args.snapshot else None
    candidate = export_area(rom, quadrants, resolver, args.area, snapshot)
    if args.castle:
        if not args.core: parser.error('--castle requires --core')
        if args.rules: parser.error('--castle and --rules are alternative generators')
        from alttp_overworld_castle import generate
        candidate, castle_rules = generate(candidate,args.core,args.castle_rise)
        if args.write_rules: args.write_rules.write_text(json.dumps(castle_rules,indent=2)+'\n')
    if args.rules:
        if not args.core: parser.error("--rules requires --core")
        candidate = apply_rules(candidate, json.loads(args.rules.read_text()), args.core)
    # Complete corpus inventory exercises the identical placement/collision export
    # for all area heads, including explicit special-area graphics failures.
    if args.audit_corpus:
        counts = Counter(); behaviors = Counter(); gfx = Counter(); areas = []
        for area in sorted(resolver.areas):
            c = candidate if area == args.area and not snapshot else export_area(rom, quadrants, resolver, area)
            if args.castle and c is not candidate and c['pin']['graphics_context']=='initial-ROM-theme:36':
                c, _ = generate(c,args.core,args.castle_rise)
            counts.update({k: c["coverage"][k] for k in ("placements", "solved", "classified", "unresolved", "conflicts")})
            behaviors.update(c["coverage"]["by_behavior"]); gfx.update(c["coverage"]["by_graphics"])
            areas.append({"area": area, "quadrants": c["pin"]["quadrants"], "placements": c["coverage"]["placements"],
                          'solved':c['coverage']['solved'], 'classified':c['coverage']['classified'],
                          'family_activation':'castle-theme36' if args.castle and c['pin']['graphics_context']=='initial-ROM-theme:36'
                                              else 'unsupported-family/graphics'})
        candidate["corpus_audit"] = {"area_heads": len(areas), "quadrants": sum(len(a["quadrants"]) for a in areas),
                                     **counts, "by_behavior": dict(behaviors), "by_graphics": dict(gfx),
                                     "areas": areas, "unsupported_state_denominator": "not enumerated: overlays/events/runtime variants"}
    args.output.write_text(json.dumps(candidate, separators=(",", ":")) + "\n")
    if args.review_summary:
        if not args.review_summary.parent.is_dir(): parser.error("review summary parent must already exist")
        args.review_summary.write_text(json.dumps(review_summary(candidate), indent=2) + "\n")
    print(json.dumps({"output": str(args.output), "pin": candidate["pin"],
                      "coverage": candidate["coverage"], "collision_source": candidate["collision_source"],
                      "family_definitions": len(candidate["families"]),
                      "collision_components": len(candidate["support_graph"]["components"]),
                      "validation_errors": validate_candidate(candidate)}, indent=2))
    return 0 # export success is NOT publication success (use --validate)


if __name__ == "__main__":
    raise SystemExit(main())
