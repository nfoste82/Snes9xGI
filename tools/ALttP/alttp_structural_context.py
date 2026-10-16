#!/usr/bin/env python3
"""Height-blind ROM composition and collision evidence browser (see STRUCTURAL-CONTEXT.md)."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import sys
import tempfile

from PIL import Image, ImageDraw
import alttp_visual_context as vc

FORMAT = "alttp-structural-context-v1"
FILES = {"bundle.json", "commands.json", "artwork.png", "collision.json", "composition.json", "behaviors.json"}
COMMIT = "fbbb3f967a51fafe642e6140d0753979e73b4090"
SOURCE = f"https://github.com/snesrev/zelda3/blob/{COMMIT}/src/tile_detect.c"
FORBIDDEN = {"height", "heights", "heightmap", "role", "semantic", "support", "assembly",
             "floor_base", "effective_height", "local_height", "content_id", "candidate", "profile"}
LIMITS = vc.LIMITS + [
    "Map32/16 blocks are ROM composition units, NOT object boundaries or ownership.",
    "Only identical IDs or exactly reflected Map8 word patterns are grouped; no inferred geometry.",
    "Water describes sample material/behavior, not elevation. Diagonal collision is NOT a ramp.",
    "Ledge detector flags are exposed literally; travel direction and signed relationships are not inferred.",
]


def leak_guard(value):
    if isinstance(value, dict):
        if FORBIDDEN.intersection(value):
            raise ValueError("forbidden authored/semantic field in input")
        for child in value.values():
            leak_guard(child)
    elif isinstance(value, list):
        for child in value:
            leak_guard(child)


def behavior(a):
    """Literal outdoor branch effects, not a movement emulator. Unknowns stay explicit."""
    label, effect = "ambiguous_unmodeled", "not transcribed; retain raw byte"
    normal = {0, 5, 6, 7, 0x14, 0x15, 0x16, 0x17, 0x21, 0x23, 0x24, 0x25,
              *range(0x38, 0x3d), 0x41, 0x45, 0x47, 0x49, 0x5e, 0x5f, 0x60,
              0x61, 0x62, 0x64, 0x65, 0x66, *range(0x6c, 0x70), 0xa6, 0xa7,
              0xbe, 0xbf, *range(0xd0, 0xf0)}
    table = {
        4: ("thick_grass", "tiledetect_thick_grass |= bits (outdoors)"),
        8: ("deep_water", "tiledetect_deepwater |= bits"),
        9: ("shallow_water", "tiledetect_shallow_water |= bits"),
        10: ("short_water_ladder_normal_flag", "tiledetect_normal_tiles |= bits"),
        11: ("deep_water_high_nibble", "index_of_interacting_tile = tile; tiledetect_deepwater |= bits << 4 (outdoors)"),
        12: ("moving_floor_flag", "tiledetect_moving_floor_tiles |= bits"),
        13: ("spike_trigger_conditional", "tiledetect_spike_floor_and_tile_triggers |= bits << 4 unless menu blocked or save bit 0x8000"),
        14: ("ice_low_nibble", "tiledetect_icy_floor |= bits"),
        15: ("ice_high_nibble", "tiledetect_icy_floor |= bits << 4"),
        0x1c: ("water_staircase_flag", "tiledetect_water_staircase |= bits"),
        0x27: ("hookshottable_flags", "R14 |= bits; tiledetect_misc_tiles |= bits"),
        0x28: ("ledge_vertical_low", "index_of_interacting_tile = tile; tiledetect_vertical_ledge |= bits"),
        0x29: ("ledge_vertical_high", "index_of_interacting_tile = tile; tiledetect_vertical_ledge |= bits << 4"),
        0x40: ("thick_grass", "tiledetect_thick_grass |= bits"),
        0x42: ("gravestone_outdoor_flags", "tiledetect_key_lock_gravestones |= bits; R14 |= bits (outdoors)"),
        0x44: ("spike_conditional", "bitfield_spike_cactus_tiles |= bits, or R14 |= bits if menu blocked/save bit 0x8000"),
        0x46: ("plaque_flags", "tiledetect_spike_floor_and_tile_triggers |= bits; R14 |= bits"),
        0x48: ("diggable_flags", "tiledetect_destruction_aftermath |= bits; tiledetect_normal_tiles |= bits"),
        0x4a: ("diggable_flags", "tiledetect_destruction_aftermath |= bits; tiledetect_normal_tiles |= bits"),
        0x4b: ("warp_grass_high_flag", "tiledetect_thick_grass |= bits << 4"),
        0x57: ("bonk_rock_flags", "R14 |= bits; bitmask_for_dashable_tiles |= bits << 4"),
        0x67: ("crystal_peg_flags", "R14 |= bits; tiledetect_misc_tiles |= bits; bitfield_spike_cactus_tiles |= bits << 4"),
    }
    if a in normal:
        label, effect = "normal_outdoor_flag_not_floor", "tiledetect_normal_tiles |= bits (outdoors)"
    elif a in (1, 2, 3, 0x26, 0x43):
        label, effect = "standard_collision_flag", "R14 |= bits"
    elif a in table:
        label, effect = table[a]
    elif a in (*range(0x10, 0x14), *range(0x18, 0x1c)):
        label = "diagonal_outer_collision" if a >= 0x18 else "diagonal_collision"
        effect = ("tiledetect_diagonal_tile |= bits; " if a >= 0x18 else "") + f"R12 |= bits; tiledetect_diag_state = {(4, 0, 6, 2)[a & 3]}"
    elif a in (0x1d, 0x1e, 0x1f):
        label, effect = "stair_detector_flags", "index_of_interacting_tile = tile; tiledetect_inroom_staircase |= bits; tiledetect_stair_tile |= bits"
    elif a == 0x20 or 0xb0 <= a <= 0xbd:
        label, effect = "pit_conditional", "tiledetect_pit_tile |= bits unless player_on_somaria_platform"
    elif a == 0x22 or 0x30 <= a <= 0x37:
        label, effect = "stair_tile_flag", "tiledetect_stair_tile |= bits"
    elif a in (0x2a, 0x2b, 0x2c, 0x2e, 0x2d, 0x2f):
        label = "ledge_horizontal_low" if a in (0x2a, 0x2b) else "ledge_diagonal_high" if a in (0x2c, 0x2e) else "ledge_down_diagonal_flag"
        effect = "index_of_interacting_tile = tile; " + (
            "detection_of_ledge_tiles_horiz_uphoriz |= bits" if a in (0x2a, 0x2b) else
            "detection_of_ledge_tiles_horiz_uphoriz |= bits << 4" if a in (0x2c, 0x2e) else
            "tiledetect_ledges_down_leftright |= bits")
    elif a in (0x3d, 0x3e, 0x3f):
        label, effect = "inroom_stair_high_flags", "index_of_interacting_tile = tile; tiledetect_inroom_staircase |= bits << 4; tiledetect_stair_tile |= bits"
    elif 0x4c <= a <= 0x4f:
        label = "unknown_corner_low_flags" if a < 0x4e else "unknown_corner_high_flags"
        effect = "index_of_interacting_tile = tile; detection_of_unknown_tile_types |= " + ("bits (outdoors)" if a < 0x4e else "bits << 4 (outdoors)")
    elif 0x50 <= a <= 0x56:
        label = "liftable_flags"
        effect = f"R14/misc/read flags; interacting_with_liftable_tile_x2 = {2 * (0x54, 0x52, 0x50, 0x51, 0x53, 0x55, 0x56).index(a)}"
        if a in (0x50, 0x51):
            effect += "; bitmask_for_dashable_tiles |= bits << 4"
    elif 0x68 <= a <= 0x6b:
        label, effect = "conveyor_flag", f"tiledetect_var4 |= bits << {(a - 0x68) * 4}"
    elif 0x70 <= a <= 0x7f:
        label, effect = "manipulable_replacement_flags", "R14/misc flags; if bits & 2: tiledetect_var2 |= 1 << (tile & 15)"
    return {"attribute": a, "label": label, "detector_effect": effect,
            "interpretation": "ambiguous" if label == "ambiguous_unmodeled" else "literal_outdoor_detector_only",
            "source": SOURCE + "#L256-L526", "handler": "TileDetect_ExecuteInner (87dc2e), switch case 0x%02x" % a,
            "walkability": "not_evaluated_state_and_actor_dependent",
            "travel_direction": "unresolved_not_inferred_from_detector_flags"}


def word_fields(word):
    return {"word": word, "tile_number": word & 1023, "collision_index": word & 511,
            "palette": (word >> 10) & 7, "priority": bool(word & 0x2000),
            "flip_h": bool(word & 0x4000), "flip_v": bool(word & 0x8000)}


def transform(words, side, h=False, v=False):
    """Spatial reflection AND XOR of member artwork flips, retaining all other bits."""
    mask = (0x4000 if h else 0) | (0x8000 if v else 0)
    return tuple(words[(side - 1 - y if v else y) * side + (side - 1 - x if h else x)] ^ mask
                 for y in range(side) for x in range(side))


def compose(rom, quadrants, ids, origin, size):
    from alttp_atlas_overworld import MAP32_TO_MAP16_TABLES, MAP16_TO_MAP8_TABLE, _decode_map32_definition
    result = {"order": "row-major; members TL, TR, BL, BR", "map32_ids": [None] * (size // 32) ** 2,
              "map16_ids": [None] * (size // 16) ** 2, "map8_words": [None] * (size // 8) ** 2,
              "map32_definitions": {}, "map16_definitions": {}, "streams": []}
    for qi in ids:
        q = quadrants[qi]
        ox, oy = (qi & 7) * 512 - origin[0], ((qi & 63) // 8) * 512 - origin[1]
        for unit, values in ((32, q.map32_ids), (16, q.map16_ids), (8, q.map8_words)):
            key = f"map{unit}_" + ("words" if unit == 8 else "ids")
            for i, value in enumerate(values):
                x, y = ox // unit + i % (512 // unit), oy // unit + i // (512 // unit)
                result[key][y * (size // unit) + x] = value
        result["streams"].append({"quadrant": qi, **{
            name: {"address": address, "compressed_hex": rom.read_bytes(address, length).hex()}
            for name, address, length in (("high", q.high_stream_address, q.high_stream_length),
                                          ("low", q.low_stream_address, q.low_stream_length))}})
    for value in sorted(set(result["map32_ids"])):
        group, variant = divmod(value, 4)
        addresses = [a + group * 6 for a in MAP32_TO_MAP16_TABLES]
        packed = [rom.read_bytes(a, 6) for a in addresses]
        result["map32_definitions"][str(value)] = {
            "member_ids": [_decode_map32_definition(p, 0, variant) for p in packed],
            "packed_group_addresses": addresses, "packed_group_hex": [p.hex() for p in packed], "variant": variant}
    for value in sorted(set(result["map16_ids"])):
        address = MAP16_TO_MAP8_TABLE + value * 8
        raw = rom.read_bytes(address, 8)
        result["map16_definitions"][str(value)] = {
            "address": address, "raw_hex": raw.hex(),
            "member_words": [int.from_bytes(raw[i:i+2], "little") for i in range(0, 8, 2)]}
    return result


def descriptor(bundle):
    base = [sys.executable, "-B", str(Path(__file__).resolve())]
    target = ["--bundle", str(bundle.resolve())]
    return {"format": FORMAT + "-commands", "coordinates": "area-local pixels; world coordinates also reported",
            "commands": {name: base + [name] + target + args for name, args in {
                "view": ["--x", "384", "--y", "512", "--width", "256", "--height", "224", "--scale", "3"],
                "overview": ["--max-size", "1024"], "inspect": ["--x", "512", "--y", "640"],
                "examples": ["--x", "512", "--y", "640", "--unit", "16", "--match", "id", "--limit", "24"],
                "metatiles": ["--unit", "32", "--offset", "0", "--limit", "24"],
            }.items()}, "read_images": "Open printed PNG paths with image/read tool; JSON retains full raw evidence.",
            "pan": "Call view with arbitrary x/y. No live camera or hidden state. Outputs are siblings, never bundle mutations."}


def prepare(rom_path, visual, output):
    from alttp_atlas import Rom, SUPPORTED_US_SHA256
    from alttp_atlas_overworld import extract_overworld
    if output.exists() and any(output.iterdir()):
        raise ValueError("output must be new or empty; immutable bundle cannot be overwritten")
    outside(output, visual)
    manifest, collision, image = vc.load_bundle(visual)
    leak_guard(manifest)
    leak_guard(collision)
    leak_guard(json.loads((visual / "commands.json").read_text()))
    rom = Rom(rom_path)
    if rom.sha256 != SUPPORTED_US_SHA256 or manifest["rom_sha256"] != rom.sha256:
        raise ValueError("unsupported or mismatched ROM fingerprint")
    # Rebuild through actual ROM decoders, not an arbitrary user-supplied picture/annotation.
    # Exact comparison also rejects resealed visual manifests/arrays containing extra fields.
    with tempfile.TemporaryDirectory(prefix="structural-rom-") as temp:
        rebuilt = Path(temp) / "visual"
        vc.prepare(rom_path, manifest["area_id"], rebuilt, manifest["animation_phase"])
        canonical, raw, rgb = vc.load_bundle(rebuilt)
        if ({k: v for k, v in manifest.items() if k != "files_sha256"} !=
                {k: v for k, v in canonical.items() if k != "files_sha256"} or
                collision != raw or image.tobytes() != rgb.tobytes()):
            raise ValueError("visual input does not match canonical ROM decode")
    composition = compose(rom, extract_overworld(rom), manifest["quadrants"], manifest["world_origin"], image.width)
    if composition["map8_words"] != collision["map8_words"]:
        raise ValueError("composition/visual Map8 mismatch")
    output.mkdir(parents=True, exist_ok=True)
    image.save(output / "artwork.png")
    vc.write_json(output / "collision.json", collision)
    vc.write_json(output / "composition.json", composition)
    vc.write_json(output / "behaviors.json", {str(a): behavior(a) for a in range(256)})
    vc.write_json(output / "commands.json", descriptor(output))
    result = {k: manifest[k] for k in ("area_id", "size", "world_origin", "rom_sha256", "state", "runtime_capture_evidence",
                                      "quadrants", "animation_phase", "graphics_context", "graphics_packs", "palette", "artwork_cell_counts")}
    result.update(format=FORMAT, limitations=LIMITS, reference_commit=COMMIT,
                  composition_sources={"decoder": "tools/ALttP/alttp_atlas_overworld.py",
                      "reference": f"https://github.com/snesrev/zelda3/blob/{COMMIT}/src/overworld.c",
                      "handlers": ["Overworld_DecompressAndDrawOneQuadrant", "Overworld_ParseMap32Definition", "OverworldCopyMap16ToBuffer"],
                      "packing": f"https://github.com/snesrev/zelda3/blob/{COMMIT}/assets/extract_resources.py"},
                  visual_bundle_sha256=vc.digest(visual / "bundle.json"),
                  denominators={"map8": len(composition["map8_words"]), "map16": len(composition["map16_ids"]),
                                "map32": len(composition["map32_ids"])},
                  attribute_counts=dict(Counter(collision["adjusted_attributes"])))
    result["files_sha256"] = {name: vc.digest(output / name) for name in sorted(FILES - {"bundle.json"})}
    vc.write_json(output / "bundle.json", result)
    load_bundle(output)
    return {"bundle": str(output.resolve()), "commands": str((output / "commands.json").resolve()),
            "denominators": result["denominators"], "attribute_counts": result["attribute_counts"]}


def load_bundle(bundle):
    if {p.name for p in bundle.iterdir()} != FILES or any(p.is_symlink() for p in bundle.iterdir()):
        raise ValueError("bundle must contain exactly the six declared regular files")
    m = json.loads((bundle / "bundle.json").read_text())
    leak_guard(m)
    if m["format"] != FORMAT or m["size"] not in ([512, 512], [1024, 1024]):
        raise ValueError("invalid bundle format/dimensions")
    data = {}
    for name in FILES - {"bundle.json"}:
        if vc.digest(bundle / name) != m["files_sha256"][name]:
            raise ValueError(f"bundle integrity mismatch: {name}")
        if name.endswith(".json"):
            data[name] = json.loads((bundle / name).read_text())
            leak_guard(data[name])
    c, comp = data["collision.json"], data["composition.json"]
    w = m["size"][0]
    if data["behaviors.json"] != {str(a): behavior(a) for a in range(256)}:
        raise ValueError("noncanonical behavior descriptions")
    if comp["map8_words"] != c["map8_words"] or len(c["lookup_table"]) != 512:
        raise ValueError("composition/collision mismatch")
    for unit, key in ((8, "map8_words"), (16, "map16_ids"), (32, "map32_ids")):
        values = comp[key]
        if len(values) != (w // unit) ** 2 or any(type(v) is not int or not 0 <= v <= 65535 for v in values):
            raise ValueError("incomplete composition array")
    if len(c["raw_attributes"]) != (w // 8) ** 2 or len(c["adjusted_attributes"]) != (w // 8) ** 2:
        raise ValueError("incomplete collision array")
    for word, raw, attr in zip(c["map8_words"], c["raw_attributes"], c["adjusted_attributes"]):
        if raw != c["lookup_table"][word & 511] or attr != vc.adjusted_attribute(raw, word):
            raise ValueError("collision lookup/flip mismatch")
    from alttp_atlas_overworld import _decode_map32_definition
    for unit in (16, 32):
        for i, value in enumerate(comp[f"map{unit}_ids"]):
            definition = comp[f"map{unit}_definitions"][str(value)]
            if unit == 16:
                raw = bytes.fromhex(definition["raw_hex"])
                members = [int.from_bytes(raw[j:j+2], "little") for j in range(0, 8, 2)]
                expected = definition["member_words"]
            else:
                members = [_decode_map32_definition(bytes.fromhex(p), 0, value % 4) for p in definition["packed_group_hex"]]
                expected = definition["member_ids"]
            x, y = i % (w // unit), i // (w // unit)
            child = comp["map8_words" if unit == 16 else "map16_ids"]
            actual = [child[(y * 2 + j // 2) * (w // (unit // 2)) + x * 2 + j % 2] for j in range(4)]
            if members != expected or actual != members:
                raise ValueError("composition members disagree")
    with Image.open(bundle / "artwork.png") as source:
        if source.size != tuple(m["size"]):
            raise ValueError("artwork dimensions mismatch")
        image = source.convert("RGB")
    return m, c, comp, image


def cell_info(m, c, comp, x, y):
    w, h = m["size"]
    if not 0 <= x < w or not 0 <= y < h:
        raise ValueError("inspection coordinate outside area")
    cx, cy = x // 8, y // 8
    i = cy * (w // 8) + cx
    result = {"pixel": [x, y], "cell": [cx, cy], "world_pixel": [x + m["world_origin"][0], y + m["world_origin"][1]],
              "quadrant": m["quadrants"][(y // 512) * (w // 512) + x // 512],
              **word_fields(comp["map8_words"][i]), "raw_attribute": c["raw_attributes"][i],
              "adjusted_attribute": c["adjusted_attributes"][i], "behavior": behavior(c["adjusted_attributes"][i]),
              "attribute_source": {"table": "$8E:9459", "adjustment": "raw | ((word >> 14) & 1) only for 0x10 <= raw < 0x1c; V has no effect",
                                   "citation": SOURCE + "#L14-L29"}}
    for unit in (16, 32):
        bx, by = x // unit, y // unit
        value = comp[f"map{unit}_ids"][by * (w // unit) + bx]
        result[f"map{unit}"] = {"id": value, "origin": [bx * unit, by * unit],
                                "map8_member_xy": [cx % (unit // 8), cy % (unit // 8)],
                                "definition": comp[f"map{unit}_definitions"][str(value)]}
    result["map32"]["map16_member_xy"] = [(x // 16) % 2, (y // 16) % 2]
    return result


def inspect(bundle, x, y):
    m, c, comp, _ = load_bundle(bundle)
    return {"area_id": m["area_id"], "rom_sha256": m["rom_sha256"], "state": m["state"],
            **cell_info(m, c, comp, x, y), "limitations": LIMITS}


def outside(output, bundle):
    if output.resolve() == bundle.resolve() or bundle.resolve() in output.resolve().parents:
        raise ValueError("outputs must be outside immutable input bundle")


def save_images(bundle, output, images, evidence):
    outside(output, bundle)
    output.mkdir(parents=True, exist_ok=True)
    paths = {}
    for name, image in images.items():
        image.save(output / (name + ".png"))
        paths[name + "_png"] = str((output / (name + ".png")).resolve())
    evidence.update(format=FORMAT + "-review", bundle_sha256=vc.digest(bundle / "bundle.json"),
                    limitations=LIMITS, **paths)
    vc.write_json(output / "evidence.json", evidence)
    return {**paths, "evidence_json": str((output / "evidence.json").resolve())}


def render_view(bundle, output, x, y, width, height, scale=3, clamp=False, max_size=None):
    m, c, comp, image = load_bundle(bundle)
    rect = vc.bounds(image.size, x, y, width, height, clamp)
    if not 1 <= scale <= 16:
        raise ValueError("scale must be 1..16")
    ax, ay, width, height = rect
    crop = image.crop((ax, ay, ax + width, ay + height))
    images = {"artwork": crop}
    detailed = crop.copy()
    draw = ImageDraw.Draw(detailed, "RGBA")
    counts = Counter()
    for cy in range(ay // 8, (ay + height - 1) // 8 + 1):
        for cx in range(ax // 8, (ax + width - 1) // 8 + 1):
            a = c["adjusted_attributes"][cy * (image.width // 8) + cx]
            counts[a] += 1
            # Per-byte stable diagnostic color; raw byte labels are on enlarged overlay.
            color = ((a * 73 + 41) % 256, (a * 151 + 97) % 256, (a * 199 + 173) % 256, 115)
            draw.rectangle((cx * 8 - ax, cy * 8 - ay, cx * 8 - ax + 7, cy * 8 - ay + 7), fill=color)
    images["collision"] = detailed
    for unit in (8, 16, 32):
        overlay = crop.copy()
        d = ImageDraw.Draw(overlay)
        for gx in range((ax // unit) * unit, ax + width, unit):
            d.line((gx - ax, 0, gx - ax, height), fill="yellow")
        for gy in range((ay // unit) * unit, ay + height, unit):
            d.line((0, gy - ay, width, gy - ay), fill="yellow")
        images[f"map{unit}"] = overlay
    size = (width * scale, height * scale)
    if max_size is not None:
        if not 1 <= max_size <= 4096:
            raise ValueError("max-size must be 1..4096")
        ratio = min(1, max_size / max(width, height))
        size = (max(1, round(width * ratio)), max(1, round(height * ratio)))
    images = {k: im.resize(size, Image.Resampling.NEAREST) for k, im in images.items()}
    sx, sy = size[0] / width, size[1] / height
    # Labels only when readable. No detail lost: evidence JSON includes every intersecting cell.
    if sx >= 3:
        for unit in (8, 16, 32):
            d = ImageDraw.Draw(images[f"map{unit}"])
            for by in range(ay // unit, (ay + height - 1) // unit + 1):
                for bx in range(ax // unit, (ax + width - 1) // unit + 1):
                    key = "map8_words" if unit == 8 else f"map{unit}_ids"
                    value = comp[key][by * (image.width // unit) + bx]
                    d.text(((bx * unit - ax) * sx + 1, (by * unit - ay) * sy + 1), f"{value:04x}" if unit == 8 else f"{value:x}", fill="white", stroke_width=1, stroke_fill="black")
        d = ImageDraw.Draw(images["collision"])
        for cy in range(ay // 8, (ay + height - 1) // 8 + 1):
            for cx in range(ax // 8, (ax + width - 1) // 8 + 1):
                a = c["adjusted_attributes"][cy * (image.width // 8) + cx]
                d.text(((cx * 8 - ax) * sx, (cy * 8 - ay) * sy), f"{a:02x}", fill="white", stroke_width=1, stroke_fill="black")
    return save_images(bundle, output, images, {"requested_rect": [x, y, width, height], "actual_rect": rect,
        "clamped": [x, y] != rect[:2], "output_size": list(size),
        "legend": {str(a): {"count": n, **behavior(a)} for a, n in sorted(counts.items())},
        "cells": [cell_info(m, c, comp, cx * 8, cy * 8) for cy in range(ay // 8, (ay + height - 1) // 8 + 1)
                  for cx in range(ax // 8, (ax + width - 1) // 8 + 1)]})


def block_words(comp, width, x, y, unit):
    side = unit // 8
    return tuple(comp["map8_words"][(y // 8 + j) * (width // 8) + x // 8 + i]
                 for j in range(side) for i in range(side))


def occurrences(m, comp, x, y, unit, match):
    w, h = m["size"]
    if not 0 <= x < w or not 0 <= y < h:
        raise ValueError("selection outside area")
    x, y = x // unit * unit, y // unit * unit
    key = "map8_words" if unit == 8 else f"map{unit}_ids"
    value = comp[key][(y // unit) * (w // unit) + x // unit]
    selected = block_words(comp, w, x, y, unit)
    patterns = [(hflip, vflip, transform(selected, unit // 8, hflip, vflip))
                for hflip, vflip in ((False, False), (True, False), (False, True), (True, True))]
    matches = []
    for by in range(0, h, unit):
        for bx in range(0, w, unit):
            other = comp[key][(by // unit) * (w // unit) + bx // unit]
            flips = ([{"h": False, "v": False}] if other == value else []) if match == "id" else [
                {"h": hf, "v": vf} for hf, vf, pattern in patterns if block_words(comp, w, bx, by, unit) == pattern]
            if flips:
                matches.append({"origin": [bx, by], "world_origin": [bx + m["world_origin"][0], by + m["world_origin"][1]],
                                "id" if unit != 8 else "word": other, "matching_transforms": flips})
    return matches


def contact_sheet(bundle, output, unit, x=0, y=0, match="id", offset=0, limit=24, catalog=False):
    m, c, comp, image = load_bundle(bundle)
    if unit not in (8, 16, 32) or not 1 <= limit <= 64 or offset < 0:
        raise ValueError("unit 8/16/32, limit 1..64, offset >= 0 required")
    if catalog:
        key = "map8_words" if unit == 8 else f"map{unit}_ids"
        first = {}
        counts = Counter(comp[key])
        for i, value in enumerate(comp[key]):
            first.setdefault(value, [i % (image.width // unit) * unit, i // (image.width // unit) * unit])
        all_rows = [{"id" if unit != 8 else "word": value, "origin": first[value], "count": counts[value]}
                    for value in sorted(first)]
    else:
        all_rows = occurrences(m, comp, x, y, unit, match)
    rows = all_rows[offset:offset + limit]
    if not rows:
        raise ValueError("empty page; offset exceeds available entries")
    sheet = Image.new("RGB", (4 * 224, ((len(rows) + 3) // 4) * 240), "#202020")
    d = ImageDraw.Draw(sheet)
    for i, row in enumerate(rows):
        bx, by = row["origin"]
        # Native context is 64+unit square; explicitly clipped at area boundary, never wrapped/padded.
        left, top, right, bottom = max(0, bx - 32), max(0, by - 32), min(image.width, bx + unit + 32), min(image.height, by + unit + 32)
        crop = image.crop((left, top, right, bottom)).resize(((right - left) * 2, (bottom - top) * 2), Image.Resampling.NEAREST)
        cx, cy = (i % 4) * 224 + 4, (i // 4) * 240 + 44
        sheet.paste(crop, (cx, cy))
        d.rectangle((cx + (bx - left) * 2, cy + (by - top) * 2, cx + (bx + unit - left) * 2 - 1, cy + (by + unit - top) * 2 - 1), outline="yellow", width=2)
        value = row.get("id", row.get("word"))
        d.text(((i % 4) * 224 + 4, (i // 4) * 240 + 2), f"{offset+i}: {value:04x} @ {bx},{by}\nworld {row['origin'][0]+m['world_origin'][0]},{row['origin'][1]+m['world_origin'][1]}" +
               (f" count={row['count']}" if catalog else "\nflips=" + ",".join(f"H{int(t['h'])}V{int(t['v'])}" for t in row["matching_transforms"])), fill="white")
        row["context_rect"] = [left, top, right - left, bottom - top]
        row["members"] = [cell_info(m, c, comp, bx + dx, by + dy) for dy in range(0, unit, 8) for dx in range(0, unit, 8)]
    return save_images(bundle, output, {"sheet": sheet}, {"unit": unit, "match": "exact_id_catalog" if catalog else match,
        "selection": None if catalog else [x, y], "total": len(all_rows), "offset": offset, "shown": len(rows),
        "remaining": max(0, len(all_rows) - offset - len(rows)), "entries": rows,
        "all_occurrences": None if catalog else all_rows})


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    prep = sub.add_parser("prepare")
    prep.add_argument("--rom", type=Path, default=Path.home() / "Downloads/Zelda_ALTTP.zip")
    prep.add_argument("--visual-bundle", type=Path, required=True)
    prep.add_argument("--output", type=Path, required=True)
    for name in ("view", "overview", "inspect", "examples", "metatiles"):
        q = sub.add_parser(name)
        q.add_argument("--bundle", type=Path, required=True)
        if name != "inspect":
            q.add_argument("--output", type=Path)
        if name in ("view", "inspect", "examples"):
            q.add_argument("--x", type=int, required=True)
            q.add_argument("--y", type=int, required=True)
        if name == "view":
            q.add_argument("--width", type=int, default=256)
            q.add_argument("--height", type=int, default=224)
            q.add_argument("--scale", type=int, default=3)
            q.add_argument("--clamp", action="store_true")
        if name == "overview":
            q.add_argument("--max-size", type=int, default=1024)
        if name in ("examples", "metatiles"):
            q.add_argument("--unit", type=int, choices=(8, 16, 32), default=16)
            q.add_argument("--offset", type=int, default=0)
            q.add_argument("--limit", type=int, default=24)
        if name == "examples":
            q.add_argument("--match", choices=("id", "flipped-words"), default="id")
    args = p.parse_args(argv)
    try:
        if args.command == "prepare":
            result = prepare(args.rom, args.visual_bundle, args.output)
        elif args.command == "inspect":
            result = inspect(args.bundle, args.x, args.y)
        else:
            output = args.output or args.bundle.with_name(args.bundle.name + "-views") / args.command
            if args.command == "view":
                result = render_view(args.bundle, output, args.x, args.y, args.width, args.height, args.scale, args.clamp)
            elif args.command == "overview":
                m, _, _, _ = load_bundle(args.bundle)
                result = render_view(args.bundle, output, 0, 0, *m["size"], scale=1, max_size=args.max_size)
            else:
                result = contact_sheet(args.bundle, output, args.unit, getattr(args, "x", 0), getattr(args, "y", 0),
                                       getattr(args, "match", "id"), args.offset, args.limit, args.command == "metatiles")
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (ValueError, OSError, KeyError, TypeError) as exc:
        p.exit(2, f"structural-context: {exc}\n")


if __name__ == "__main__":
    main()
