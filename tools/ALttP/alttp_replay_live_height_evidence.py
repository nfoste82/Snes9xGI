#!/usr/bin/env python3
"""Replay complete live capture sidecars through the exact C++ room solver.

This is input reproduction, not validation-v2 or movement/provenance approval.
Output directory must already exist. No input/profile is rewritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("capture", type=Path)
    p.add_argument("--solver", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--rom", type=Path)
    a = p.parse_args()
    if not a.output.is_dir():
        p.error("output directory must exist")
    prefix = str(a.capture)
    state = json.loads(Path(prefix + ".dungeon.json").read_text())
    if state.get("rom_sha256") != "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb":
        p.error("unsupported ROM fingerprint")
    if state.get("collision_mode") != 0:
        p.error("unsupported collision mode; do not replay as mode zero")
    profile = Path(prefix + ".profile.toml").read_bytes()
    digest = 14695981039346656037
    for byte in profile:
        digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
    if f"{digest:x}" != state.get("profile_serialized_fnv1a64"):
        p.error("profile snapshot digest mismatch")
    blob = Path(prefix + ".dungeon-input.bin").read_bytes()
    if len(blob) != 8 + 16384 + 8192 + 65536 or blob[:8] != b"ALTPDI1\0":
        p.error("missing complete live inputs")
    maps, attr, vram = blob[8:16392], blob[16392:24584], blob[24584:]
    mismatches = []
    for layer in range(2):
        if state["ppu_map_size"][layer] != 3:
            p.error("PPU map is not complete 64x64")
        for y in range(64):
            for x in range(64):
                address = (state["ppu_map_base"][layer] + y // 32 * 2048 +
                           x // 32 * 1024 + y % 32 * 32 + x % 32) & 32767
                target = (8192 if layer == 0 else 0) + (y * 64 + x) * 2
                if maps[target:target + 2] != vram[address * 2:address * 2 + 2]:
                    mismatches.append([layer, x, y])
    (a.output / "input-comparison.json").write_text(json.dumps({
        "settled": state["publishable_inputs"], "wram_ppu_mismatch_count": len(mismatches),
        "wram_ppu_mismatches": mismatches, "capture_state": state}, indent=2) + "\n")
    if mismatches or not state["publishable_inputs"]:
        p.error("inputs not settled/equal; inspect input-comparison.json; refusing geometry replay")
    # Lossless solver-only ALTPPD1 wrapper: colors/ownership are intentionally
    # empty, never a gameplay packet. Original full VRAM/maps are unchanged.
    line = bytearray()
    for layer in range(3):
        base = state["ppu_map_base"][layer] if layer < 2 else 0
        graphics = state.get("bg1_graphics_base", state["graphics_base"]) if layer == 0 else state["graphics_base"]
        if graphics == 65535:
            graphics = state["graphics_base"]
        line += struct.pack("<5H", 0, 0, base, graphics // 2, 3)
    line += bytes(256 * 6)
    raw = a.output / "live-solver-input.raw"
    raw.write_bytes(b"ALTPPD1\0" + struct.pack("<3H", state["room"], 0, 0) + line * 224 + vram + bytes(512))
    attributes = Path(str(raw) + ".attr")
    attributes.write_bytes(b"ALTPAT1\0" + struct.pack("<H", state["room"]) + attr)
    command = [str(a.solver), str(raw), str(attributes), prefix + ".profile.toml",
               *map(str, state["link_xy"]), str(state["plane"])]
    for stair in state["stairs"]:
        command += list(map(str, stair))
    if a.rom:
        if hashlib.sha256(a.rom.read_bytes()).hexdigest() != state['rom_sha256']:
            p.error('ROM fingerprint mismatch')
        command += ['--rom', str(a.rom)]
        template_path = Path(prefix+'.dungeon-templates.bin')
        if template_path.exists() and template_path.read_bytes() != b'ALTPDT1\0'+a.rom.read_bytes()[0x1b52:0x1b52+12876]:
            p.error('capture template evidence differs from verified ROM')
    with (a.output / "live-runtime.json").open("w") as out:
        subprocess.run(command, stdout=out, check=True)
    print("Replayed pinned live inputs; full-room movement graph and validation v2 still required.")


if __name__ == "__main__":
    main()
