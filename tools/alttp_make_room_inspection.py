#!/usr/bin/env python3
"""Make linked inspection captures for one ALTTP dungeon room viewport.

The local reference renderer must already have been built from the supplied
ROM. Outputs are local review artifacts; they contain ROM-derived artwork.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tomllib
import zlib

from alttp_room_geometry import (RoomProvenance, RoomSnapshot, geometry_report,
                                 room_object_roles,
                                 load_tile_semantics, room_stairs, write_height_map,
                                 write_offset_preview,
                                 write_preview,
                                 write_semantics_template)


def png_from_ppm(source: Path, target: Path) -> None:
    data = source.read_bytes()
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    if not match:
        raise ValueError("inspection preview is not a P6 PPM")
    width, height = (int(group) for group in match.groups())
    pixels = data[match.end():]
    if len(pixels) != width * height * 3:
        raise ValueError("inspection preview has incomplete pixels")

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))

    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    target.write_bytes(b"\x89PNG\r\n\x1a\n" +
                       chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                       chunk(b"IDAT", zlib.compress(rows, 7)) + chunk(b"IEND", b""))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--converter", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--geometry-atlas", type=Path,
                        help="ROM atlas for an experimental stair/floor geometry review map")
    parser.add_argument("--geometry-semantics", type=Path,
                        help="optional edited wall-face orientation file")
    parser.add_argument("--room", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--entrance", type=lambda value: int(value, 0), default=-1,
                        help="ROM entrance ID; defaults to the room's own entrance when one exists")
    parser.add_argument("--view-x", type=int, default=0)
    parser.add_argument("--view-y", type=int, default=0)
    parser.add_argument("--all-viewports", action="store_true",
                        help="capture six overlapping viewports covering the full 512 by 512 room")
    parser.add_argument("--advance-frames", default="0,8,16,24",
                        help="comma-separated game frames to advance after room settle")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 0 <= args.room < 320 or not 0 <= args.view_x <= 256 or not 0 <= args.view_y <= 288:
        parser.error("room or viewport is outside the dungeon bounds")
    advances = [int(value) for value in args.advance_frames.split(",")]
    if not advances or len(advances) != len(set(advances)) or any(not 0 <= value <= 240 for value in advances):
        parser.error("advance frames must be distinct values in 0..240")
    renderer = args.renderer.resolve()
    converter = args.converter.resolve()
    profile = args.profile.resolve()
    geometry_atlas = args.geometry_atlas.resolve() if args.geometry_atlas else None
    if args.geometry_semantics and not geometry_atlas:
        parser.error("--geometry-semantics requires --geometry-atlas")
    semantics = load_tile_semantics(args.geometry_semantics) if args.geometry_semantics else {}
    if geometry_atlas:
        with profile.open("rb") as source:
            geometry_profile = tomllib.load(source)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    variants = []
    viewports = [(x, y) for y in (0, 144, 288) for x in (0, 256)] if args.all_viewports else [
        (args.view_x, args.view_y)]
    seen_images: dict[tuple[int, int, str], int] = {}
    actual_entrance = None
    source_room = None
    theme_id = None
    geometry_entry = None
    geometry_words = None
    for view_x, view_y in viewports:
        for advance in advances:
            stem = f"room-{args.room:03x}-x{view_x:03d}-y{view_y:03d}-f{advance:03d}"
            raw, capture, ppm, png = (output / f"{stem}{suffix}" for suffix in
                                      (".raw", ".s9xrmf", ".ppm", ".png"))
            result = subprocess.run([str(renderer), "--linked", hex(args.room), str(raw),
                                     str(args.entrance), str(view_x), str(view_y), str(advance)],
                                    cwd=renderer.parent, check=True, capture_output=True, text=True)
            info = dict(part.split("=", 1) for part in result.stdout.split())
            if int(info["room"], 16) != args.room:
                raise ValueError("renderer settled in a different room")
            if actual_entrance is None:
                actual_entrance = int(info["entrance"])
                source_room = int(info["source_room"], 16)
                theme_id = int(info["theme"])
            elif (actual_entrance, source_room, theme_id) != (
                    int(info["entrance"]), int(info["source_room"], 16), int(info["theme"])):
                raise ValueError("renderer changed graphics context between views")
            converted = subprocess.run([str(converter), str(raw), str(profile), str(capture), str(ppm)],
                                       check=True, capture_output=True, text=True)
            png_from_ppm(ppm, png)
            if geometry_atlas:
                snapshot = RoomSnapshot.read(raw)
                trace_path = Path(str(raw) + ".prov")
                if not trace_path.is_file():
                    raise ValueError("geometry inspection requires a renderer with ALTPRV1 provenance")
                provenance = RoomProvenance.read(trace_path, snapshot)
                words = tuple(tuple(cell.word for cell in layer) for layer in snapshot.cells)
                if geometry_entry is None:
                    geometry_words = words
                    geometry = geometry_report(snapshot, geometry_profile,
                                               room_stairs(geometry_atlas, args.room,
                                                           geometry_profile.get("game", {}).get("rom_sha256")),
                                               provenance, semantics,
                                               room_object_roles(geometry_atlas, args.room,
                                                                 geometry_profile.get("game", {}).get("rom_sha256")))
                    geometry_json = output / "geometry.json"
                    geometry_png = output / "geometry.png"
                    geometry_json.write_text(json.dumps(geometry, indent=2) + "\n")
                    write_preview(geometry_png, geometry, snapshot, geometry_profile, semantics)
                    write_offset_preview(output / "height-proposals.png", geometry)
                    write_height_map(output / "height-proposals.bin", geometry)
                    write_semantics_template(output / "tile-semantics-template.json",
                                             set(geometry_profile["asset_groups"]["wall_faces"]["tile_hashes"]),
                                             {asset["tile_hash"]: asset for asset in geometry_profile["assets"]})
                    geometry_entry = {
                        "report": geometry_json.name, "preview": geometry_png.name,
                        "height_preview": "height-proposals.png",
                        "height_map": "height-proposals.bin",
                        "solved_flat_tiles": sum(component["tiles"] for component in geometry["components"]
                                                 if component["height"] is not None),
                        "wall_face_tiles": geometry["wall_face_cells"],
                        "wall_heights_solved": False,
                        "object_provenance": True,
                        "orientation_overrides": len(geometry["wall_orientation_overrides"]),
                    }
                if words == geometry_words:
                    proposed_capture = output / f"{stem}-proposed.s9xrmf"
                    proposed_result = subprocess.run(
                        [str(converter), str(raw), str(profile), str(proposed_capture),
                         str(ppm), "--height-map", str(output / "height-proposals.bin")],
                        check=True, capture_output=True, text=True)
                else:
                    proposed_capture = None
            color_hash = hashlib.sha256(png.read_bytes()).hexdigest()
            entry = {"viewport": [view_x, view_y], "advance_frames": advance,
                     "capture": capture.name, "preview": png.name,
                     "capture_bytes": capture.stat().st_size, "preview_sha256": color_hash,
                     "renderer": result.stdout.strip(), "coverage": converted.stderr.strip()}
            if geometry_atlas:
                if proposed_capture is not None:
                    entry["proposed_capture"] = proposed_capture.name
                    entry["proposed_coverage"] = proposed_result.stderr.strip()
                else:
                    entry["geometry_state_changed"] = True
            image_key = (view_x, view_y, color_hash)
            if image_key in seen_images:
                entry["same_preview_as_frame"] = seen_images[image_key]
            else:
                seen_images[image_key] = advance
            variants.append(entry)
            raw.unlink()
            provenance_path = Path(str(raw) + ".prov")
            if provenance_path.is_file():
                provenance_path.unlink()
            ppm.unlink()
            print(f"{stem}: {entry['coverage']}", flush=True)
    manifest = {"format": "alttp-linked-inspection-v1", "room_id": args.room,
                "entrance_id": actual_entrance, "entrance_source_room": source_room,
                "theme_id": theme_id, "graphics_context_inferred": source_room != args.room,
                "viewports": [list(viewport) for viewport in viewports],
                "profile": str(profile), "renderer": str(renderer),
                "limitations": ["BG1 and BG2 carry tile identity and profile metadata.",
                                "The original colors are from the reference engine PPU.",
                                "Sprites and HUD retain color but are not linked to profile metadata.",
                                "Room state and graphics theme follow the selected entrance.",
                                "Proposed captures contain review-only BG2 placement offsets when geometry is enabled."],
                "variants": variants}
    if geometry_entry:
        manifest["geometry"] = geometry_entry
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Inspection captures: {output}")


if __name__ == "__main__":
    main()
