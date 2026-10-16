#!/usr/bin/env python3
"""Offline, ROM-only overworld artwork/collision browser. No geometry inputs.

Palette and collision loader transcription: snesrev/zelda3 load_gfx.c and
tile_detect.c at fbbb3f967a51fafe642e6140d0753979e73b4090.
See zelda3-MIT-LICENSE.txt and VISUAL-CONTEXT.md.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

from PIL import Image, ImageDraw

FORMAT = "alttp-visual-context-v1"
FILES = {"bundle.json", "collision.json", "artwork.png", "commands.json"}
COLLISION_TABLE = 0x8E9459
# kOwBgPalInfo: negative entries inherit entry-state palette registers.
OW_BG_PAL_INFO = (
    (0, -1, 7), (0, 1, 7), (0, 2, 7), (0, 3, 7), (0, 4, 7), (0, 5, 7),
    (0, 6, 7), (7, 6, 5), (0, 8, 7), (0, 9, 7), (0, 10, 7), (0, 11, 7),
    (0, -1, 7), (0, -1, 7), (3, 4, 7), (4, 4, 3), (16, -1, 6),
    (16, 1, 6), (16, 17, 6), (16, 3, 6), (16, 4, 6), (16, 5, 6),
    (16, 6, 6), (18, 19, 4), (18, 5, 4), (16, 9, 6), (16, 11, 6),
    (16, 12, 6), (16, 13, 6), (16, 14, 6), (16, 15, 6),
)
LIMITS = [
    "Offline base ROM state, not a runtime capture or live camera.",
    "No event overlays, persistent edits, actors, rain, fades, or color math.",
    "Explicit animation phase; not synchronized with gameplay.",
    "Collision is a base ROM lookup, not completed runtime collision state.",
    "Behavior labels describe isolated outdoor samples, not legal movement or floor identity.",
    "No actor footprint, reachability, equipment, directional transitions, or overlapping gameplay planes modeled.",
    "Area-local coordinates; no wrapping or adjacent-area stitching.",
]


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def adjusted_attribute(raw, word):
    """Overworld_GetTileAttributeAtLocation, NOT an artwork flip heuristic."""
    return raw | ((word >> 14) & 1) if 0x10 <= raw < 0x1C else raw


def behavior(attribute):
    """Deliberately narrow TileDetect_ExecuteInner outdoor sample summary."""
    if attribute in (1, 2, 3, 0x26, 0x43):
        return "blocking_sample"
    if attribute in (0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1A, 0x1B):
        return "directional_slope_sample"
    if attribute == 0:
        return "normal_sample_not_floor_proof"
    if attribute in (8, 0x0B):
        return "deep_water_sample_state_dependent"
    if attribute == 9:
        return "shallow_water_sample"
    return "other_behavior_not_modeled"


COLORS = {
    "blocking_sample": (255, 65, 65),
    "directional_slope_sample": (255, 180, 35),
    "normal_sample_not_floor_proof": (50, 190, 235),
    "deep_water_sample_state_dependent": (75, 80, 255),
    "shallow_water_sample": (90, 135, 240),
    "other_behavior_not_modeled": (200, 100, 215),
}


def rgb555(word):
    return tuple(((word >> shift) & 31) * 255 // 31 for shift in (0, 5, 10))


def rom_palette(rom, area, selector):
    """Normal unfaded OW BG palette load; unknown inherited slots stay None."""
    if not 0 <= selector < len(OW_BG_PAL_INFO):
        return [None] * 128, {"mode": "diagnostic_grayscale", "reason": "unsupported palette selector"}
    aux1, aux2, aux3 = OW_BG_PAL_INFO[selector]
    mode = (2 if (area & 63) in (3, 5, 7) else 0) + bool(area & 64)
    palette = [None] * 128
    backdrop = 0x2A32 if area & 64 else 0x2669
    for i in range(0, 128, 16):
        palette[i] = backdrop

    def load(address, index, count, destination, rows):
        if index < 0:
            return
        for row in range(rows):
            for column in range(count):
                palette[destination + row * 16 + column] = rom.get_word(
                    address + (index * count * rows + row * count + column) * 2)

    load(0x9BE6C8, mode, 7, 0x21, 5)
    load(0x9BE86C, aux1, 7, 0x29, 3)
    load(0x9BE86C, aux2, 7, 0x59, 3)
    load(0x9BE604, aux3, 7, 0x71, 1)
    return palette, {
        "mode": "rom_normal_unfaded", "area_selector": selector,
        "main_mode": int(mode), "aux_selectors": [aux1, aux2, aux3],
        "source_addresses": [0x80FD1C + area, 0x9BE6C8, 0x9BE86C, 0x9BE604],
        "runtime_color_match": "unverified; no matching gameplay capture compared",
        "capture_palette_used": False,
        "unknown_slots": [i for i, v in enumerate(palette) if v is None],
    }


def command_descriptor(bundle):
    base = [sys.executable, "-B", str(Path(__file__).resolve())]
    target = ["--bundle", str(bundle.resolve())]
    return {
        "format": FORMAT + "-commands", "coordinate_units": "area-local pixels, top-left origin",
        "execution": "offline crop/pan only; no emulator controls",
        "commands": {
            "view": base + ["view"] + target + ["--x", "384", "--y", "512", "--width", "256", "--height", "224", "--scale", "3"],
            "step": base + ["step"] + target + ["--dx", "128", "--dy", "0", "--clamp"],
            "overview": base + ["overview"] + target + ["--max-size", "512"],
            "inspect": base + ["inspect"] + target + ["--x", "512", "--y", "640"],
        },
        "read_with_image_tool": "Each view prints absolute artwork PNG, collision PNG and JSON paths. Open the PNGs with the agent's image/read tool.",
    }


def prepare(rom_path, area, output, phase=0, grayscale=False):
    # Import only ROM readers/decoders. Never import candidate/geometry tools.
    from alttp_atlas import Rom, SUPPORTED_US_SHA256, REFERENCE_COMMIT
    from alttp_atlas_graphics import extract_graphics
    from alttp_atlas_overworld import extract_overworld
    from alttp_atlas_overworld_gfx import BaseOverworldGraphics
    from alttp_atlas_world import AREA_HEAD_TABLE, AREA_SIZE_TABLE

    if output.exists() and any(output.iterdir()):
        raise ValueError("output must be a new or empty directory (no stale/mixed artifacts)")
    rom = Rom(rom_path)
    if rom.sha256 != SUPPORTED_US_SHA256:
        raise ValueError("unsupported ROM fingerprint")
    if not 0 <= area < 128:
        raise ValueError("only ordinary light/dark overworld area heads supported")
    if rom.get_byte(AREA_HEAD_TABLE + (area & 63)) != area & 63:
        raise ValueError("--area must be an area head, not a satellite quadrant")
    if phase not in (0, 1, 2):
        raise ValueError("animation phase must be 0..2")
    record = {"area_id": area, "is_small": bool(rom.get_byte(AREA_SIZE_TABLE + area)),
              "gfx_index": rom.get_byte(0x80FC9C + area)}
    quadrants = extract_overworld(rom)
    graphics = extract_graphics(rom)
    resolver = BaseOverworldGraphics(graphics, [record], quadrants)
    ids = resolver.area_quadrants(area)
    side = 512 if record["is_small"] else 1024
    origin = [(area & 7) * 512, ((area & 63) // 8) * 512]
    image = Image.new("RGB", (side, side), "magenta")
    by_id = {g.content_id: g.indices for g in graphics}
    palette, palette_info = rom_palette(rom, area, rom.get_byte(0x80FD1C + area))
    words = [None] * ((side // 8) ** 2)
    raw = words.copy()
    adjusted = words.copy()
    lut = list(rom.read_bytes(COLLISION_TABLE, 512))
    statuses = Counter()
    unknown_colors = Counter()
    for qi in ids:
        ox = (qi & 7) * 512 - origin[0]
        oy = ((qi & 63) // 8) * 512 - origin[1]
        for tile in quadrants[qi].iter_tiles():
            x, y = ox + tile.x * 8, oy + tile.y * 8
            index = (y // 8) * (side // 8) + x // 8
            words[index] = tile.word
            raw[index] = lut[tile.word & 0x1FF]
            adjusted[index] = adjusted_attribute(raw[index], tile.word)
            resolved = resolver.resolve_tile(area, tile)
            if resolved.status == "animated_vram_slot":
                resolved = resolver.resolve_animated_phase(resolved, phase)
            statuses[resolved.status] += 1
            if resolved.content_id is None:
                continue  # Visible magenta, never a silently invented tile.
            colors = []
            for value in by_id[resolved.content_id]:
                slot = tile.palette * 16 + value
                color = palette[slot]
                if color is None:
                    unknown_colors[slot] += 1
                colors.append((value * 17,) * 3 if grayscale or color is None else rgb555(color))
            stamp = Image.new("RGB", (8, 8))
            stamp.putdata(colors)
            if tile.flip_x:
                stamp = stamp.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if tile.flip_y:
                stamp = stamp.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            image.paste(stamp, (x, y))
    if any(w is None for w in words):
        raise ValueError("incomplete area expansion")
    palette_info["display"] = ("diagnostic_grayscale" if grayscale else
                               "rom_color_with_diagnostic_grayscale_unknown_slots" if unknown_colors else "rom_color")
    palette_info["unknown_used_pixel_count"] = sum(unknown_colors.values())
    palette_info["unknown_used_slots"] = dict(unknown_colors)
    output.mkdir(parents=True, exist_ok=True)
    image.save(output / "artwork.png")
    write_json(output / "collision.json", {
        "format": FORMAT + "-collision", "grid_size": [side // 8, side // 8],
        "cell_size": 8, "order": "row-major", "map8_words": words,
        "raw_attributes": raw, "adjusted_attributes": adjusted, "lookup_table": lut,
        "source_address": COLLISION_TABLE,
    })
    write_json(output / "commands.json", command_descriptor(output))
    manifest = {
        "format": FORMAT, "area_id": area, "quadrants": list(ids), "size": [side, side],
        "world_origin": origin, "rom_sha256": rom.sha256,
        "state": "base_rom_no_runtime_edits", "runtime_capture_evidence": None,
        "reference_commit": REFERENCE_COMMIT, "animation_phase": phase,
        "graphics_context": record, "graphics_packs": resolver.slots_for_area(area),
        "artwork_cell_counts": dict(statuses), "palette": palette_info,
        "collision_legend": {k: list(v) for k, v in COLORS.items()},
        "limitations": LIMITS,
        "files_sha256": {name: digest(output / name) for name in sorted(FILES - {"bundle.json"})},
    }
    write_json(output / "bundle.json", manifest)
    return {"bundle": str(output.resolve()), "manifest": str((output / "bundle.json").resolve()),
            "commands": str((output / "commands.json").resolve()), "size": manifest["size"],
            "palette": palette_info, "artwork_cell_counts": dict(statuses)}


def load_bundle(bundle):
    if {p.name for p in bundle.iterdir()} != FILES:
        raise ValueError("bundle must contain only the four declared visual-context files")
    manifest = json.loads((bundle / "bundle.json").read_text())
    if manifest["format"] != FORMAT:
        raise ValueError("unsupported bundle format")
    for name in FILES - {"bundle.json"}:
        if digest(bundle / name) != manifest["files_sha256"][name]:
            raise ValueError(f"bundle integrity mismatch: {name}")
    collision = json.loads((bundle / "collision.json").read_text())
    w, h = manifest["size"]
    if w not in (512, 1024) or h != w or collision["grid_size"] != [w // 8, h // 8]:
        raise ValueError("invalid bundle dimensions")
    count = w * h // 64
    for key, maximum in (("map8_words", 65535), ("raw_attributes", 255), ("adjusted_attributes", 255)):
        values = collision[key]
        if len(values) != count or any(type(v) is not int or not 0 <= v <= maximum for v in values):
            raise ValueError(f"invalid collision array: {key}")
    lut = collision["lookup_table"]
    if len(lut) != 512 or any(type(v) is not int or not 0 <= v <= 255 for v in lut):
        raise ValueError("invalid collision LUT")
    for word, raw, attr in zip(collision["map8_words"], collision["raw_attributes"], collision["adjusted_attributes"]):
        if raw != lut[word & 511] or attr != adjusted_attribute(raw, word):
            raise ValueError("collision lookup/flip mismatch")
    with Image.open(bundle / "artwork.png") as source:
        if source.size != (w, h):
            raise ValueError("artwork dimensions mismatch")
        image = source.convert("RGB")
    return manifest, collision, image


def bounds(size, x, y, width, height, clamp=False):
    w, h = size
    if not 1 <= width <= w or not 1 <= height <= h:
        raise ValueError("view dimensions must fit inside the area")
    actual_x, actual_y = min(max(x, 0), w - width), min(max(y, 0), h - height)
    if not clamp and (actual_x, actual_y) != (x, y):
        raise ValueError("view outside area; use --clamp explicitly (never wraps or pads)")
    return [actual_x, actual_y, width, height]


def inspect_cell(bundle, x, y):
    manifest, collision, _ = load_bundle(bundle)
    w, h = manifest["size"]
    if not 0 <= x < w or not 0 <= y < h:
        raise ValueError("inspection coordinate outside area")
    i = (y // 8) * (w // 8) + x // 8
    word = collision["map8_words"][i]
    return {"area_id": manifest["area_id"], "state": manifest["state"],
            "pixel": [x, y], "cell": [x // 8, y // 8], "map8_word": word,
            "collision_index": word & 511, "raw_collision": collision["raw_attributes"][i],
            "adjusted_collision": collision["adjusted_attributes"][i],
            "flip_h": bool(word & 0x4000), "flip_v": bool(word & 0x8000),
            "behavior": behavior(collision["adjusted_attributes"][i]),
            "walkability": "not_evaluated_state_and_actor_dependent",
            "rom_sha256": manifest["rom_sha256"], "limitations": LIMITS}


def render_view(bundle, output, x, y, width, height, scale=3, clamp=False, max_size=None):
    manifest, collision, image = load_bundle(bundle)
    rect = bounds(image.size, x, y, width, height, clamp)
    if not 1 <= scale <= 16:
        raise ValueError("scale must be 1..16")
    ax, ay, width, height = rect
    crop = image.crop((ax, ay, ax + width, ay + height))
    tint = Image.new("RGBA", crop.size)
    draw = ImageDraw.Draw(tint)
    counts = Counter()
    for cy in range(ay // 8, (ay + height - 1) // 8 + 1):
        for cx in range(ax // 8, (ax + width - 1) // 8 + 1):
            attr = collision["adjusted_attributes"][cy * (image.width // 8) + cx]
            label = behavior(attr)
            counts[label] += 1
            draw.rectangle((cx * 8 - ax, cy * 8 - ay, cx * 8 + 7 - ax, cy * 8 + 7 - ay),
                           fill=(*COLORS[label], 110))
    overlay = Image.alpha_composite(crop.convert("RGBA"), tint).convert("RGB")
    size = (width * scale, height * scale)
    if max_size is not None:
        if not 1 <= max_size <= 4096:
            raise ValueError("max-size must be 1..4096")
        ratio = min(1, max_size / max(width, height))
        size = (max(1, round(width * ratio)), max(1, round(height * ratio)))
    output.mkdir(parents=True, exist_ok=True)
    crop.resize(size, Image.Resampling.NEAREST).save(output / "artwork.png")
    overlay.resize(size, Image.Resampling.NEAREST).save(output / "collision.png")
    result = {"format": FORMAT + "-view", "bundle": str(bundle.resolve()),
              "bundle_sha256": digest(bundle / "bundle.json"),
              "area_id": manifest["area_id"], "rom_sha256": manifest["rom_sha256"],
              "state": manifest["state"], "runtime_capture_evidence": None,
              "requested_rect": [x, y, width, height], "actual_rect": rect,
              "clamped": [x, y] != rect[:2], "output_size": list(size),
              "palette": manifest["palette"], "behavior_cell_counts": dict(counts),
              "collision_legend": manifest["collision_legend"], "limitations": LIMITS,
              "artwork_png": str((output / "artwork.png").resolve()),
              "collision_png": str((output / "collision.png").resolve()),
              "provenance_json": str((output / "view.json").resolve())}
    write_json(output / "view.json", result)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="command", required=True)
    p = subs.add_parser("prepare")
    p.add_argument("--rom", type=Path, default=Path.home() / "Downloads/Zelda_ALTTP.zip")
    p.add_argument("--area", type=lambda s: int(s, 0), required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--animation-phase", type=int, choices=(0, 1, 2), default=0)
    p.add_argument("--grayscale", action="store_true")
    for name in ("view", "step", "overview", "inspect"):
        p = subs.add_parser(name)
        p.add_argument("--bundle", type=Path, required=True)
        if name == "inspect":
            p.add_argument("--x", type=int, required=True)
            p.add_argument("--y", type=int, required=True)
            continue
        p.add_argument("--output", type=Path)
        if name == "overview":
            p.add_argument("--max-size", type=int, default=512)
            continue
        p.add_argument("--state", type=Path, help="optional persistent offline pan state")
        p.add_argument("--clamp", action="store_true")
        if name == "step":
            p.add_argument("--dx", type=int, default=0)
            p.add_argument("--dy", type=int, default=0)
        else:
            p.add_argument("--x", type=int, default=0)
            p.add_argument("--y", type=int, default=0)
            p.add_argument("--width", type=int, default=256)
            p.add_argument("--height", type=int, default=224)
            p.add_argument("--scale", type=int, default=3)
    args = parser.parse_args(argv)
    try:
        if args.command == "prepare":
            result = prepare(args.rom, args.area, args.output, args.animation_phase, args.grayscale)
        elif args.command == "inspect":
            result = inspect_cell(args.bundle, args.x, args.y)
        else:
            output = args.output or args.bundle.with_name(args.bundle.name + "-views") / args.command
            if output.resolve() == args.bundle.resolve() or args.bundle.resolve() in output.resolve().parents:
                raise ValueError("view outputs must be outside the sealed input bundle")
            if args.command == "overview":
                manifest, _, _ = load_bundle(args.bundle)
                result = render_view(args.bundle, output, 0, 0, *manifest["size"], scale=1, max_size=args.max_size)
            else:
                state = args.state or args.bundle.with_name(args.bundle.name + "-pan.json")
                if state.resolve() == args.bundle.resolve() or args.bundle.resolve() in state.resolve().parents:
                    raise ValueError("pan state must be outside the sealed input bundle")
                if args.command == "step":
                    previous = json.loads(state.read_text())
                    if previous["bundle_sha256"] != digest(args.bundle / "bundle.json"):
                        raise ValueError("pan state belongs to another bundle")
                    x, y, width, height = previous["actual_rect"]
                    x += args.dx
                    y += args.dy
                    scale = previous["scale"]
                else:
                    x, y, width, height, scale = args.x, args.y, args.width, args.height, args.scale
                result = render_view(args.bundle, output, x, y, width, height, scale, args.clamp)
                state.parent.mkdir(parents=True, exist_ok=True)
                write_json(state, {"bundle_sha256": result["bundle_sha256"],
                                   "actual_rect": result["actual_rect"], "scale": scale})
                result["pan_state"] = str(state.resolve())
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (ValueError, OSError, KeyError, TypeError) as exc:
        parser.exit(2, f"visual-context: {exc}\n")


if __name__ == "__main__":
    main()
