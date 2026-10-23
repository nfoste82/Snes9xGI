#!/usr/bin/env python3
"""Build the local, headless Zelda3 room preview adapter from a supplied ROM.

The game engine and ROM-derived assets stay in an ignored output directory.
This repository stores only the small adapter, not the ROM or game artwork.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

from alttp_atlas import SUPPORTED_US_SHA256


def rom_bytes(path: Path) -> bytes:
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            members = [name for name in archive.namelist()
                       if name.lower().endswith((".sfc", ".smc"))]
            if len(members) != 1:
                raise ValueError("ROM ZIP must contain exactly one .sfc or .smc")
            data = archive.read(members[0])
    else:
        data = path.read_bytes()
    if len(data) % 0x8000 == 0x200:
        data = data[0x200:]
    if hashlib.sha256(data).hexdigest() != SUPPORTED_US_SHA256:
        raise ValueError("ROM does not match the supported USA version")
    return data


def build(source: Path, rom: Path, output: Path, python: str, pythonpath: str | None,
          asset_cache: Path | None = None) -> Path:
    if not (source / "src/main.c").is_file():
        raise ValueError("--source must be a snesrev/zelda3 checkout")
    if output.exists():
        raise ValueError("--output must name a new directory")
    shutil.copytree(source, output, ignore=shutil.ignore_patterns(
        ".git", "*.o", "zelda3", "zelda3_assets.dat", "zelda3.sfc", "__pycache__"))
    (output / "zelda3.sfc").write_bytes(rom_bytes(rom))

    # The GUI source undefines main for SDL's entry point. Keep main renamed
    # when the adapter includes that file, without changing the source checkout.
    main_source = output / "src/main.c"
    content = main_source.read_text()
    original = "#undef main\nint main"
    if original in content:
        main_source.write_text(content.replace(original, "/* room adapter entry point */\nint main", 1))
    elif "/* room adapter entry point */\nint main" not in content:
        raise ValueError("reference engine main.c has an unexpected entry point")

    # An earlier local renderer can also serve as the source. Remove its PPU
    # callback before the ordinary engine links; the adapter reinstalls it
    # after compiling the standalone object files.
    ppu_source = output / "snes/ppu.c"
    ppu_text = ppu_source.read_text()
    old_declaration = "extern void AlttpCaptureLine(Ppu *ppu, unsigned line);\n"
    old_call = "  AlttpCaptureLine(ppu, y - 1);\n"
    if (old_declaration in ppu_text) != (old_call in ppu_text):
        raise ValueError("reference PPU has only part of the linked capture hook")
    if old_declaration in ppu_text:
        ppu_source.write_text(ppu_text.replace(old_declaration, "", 1).replace(old_call, "", 1))
    dungeon_source = output / "src/dungeon.c"
    dungeon_text = dungeon_source.read_text()
    if "AlttpTraceRoomObjectBegin" in dungeon_text:
        dungeon_text = re.sub(
            r"(?m)^.*AlttpTraceRoom(?:Reset|Phase|ObjectBegin|ObjectEnd|Loaded)\([^\n]*\);\n",
            "", dungeon_text)
        dungeon_source.write_text(dungeon_text)

    environment = os.environ.copy()
    if pythonpath:
        environment["PYTHONPATH"] = pythonpath
    if asset_cache:
        if hashlib.sha256((asset_cache / "zelda3.sfc").read_bytes()).hexdigest() != SUPPORTED_US_SHA256:
            raise ValueError("cached engine has a different ROM")
        shutil.copy2(asset_cache / "zelda3_assets.dat", output / "zelda3_assets.dat")
    else:
        subprocess.run([python, "assets/restool.py", "--rom", "zelda3.sfc",
                        "--extract-from-rom"], cwd=output, env=environment, check=True)

    sdl_flags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).split()
    sdl_libs = subprocess.check_output(["sdl2-config", "--libs"], text=True).split()
    cflags = ["-O2", "-std=gnu11", "-Wno-error", "-I", ".", *sdl_flags,
              "-DSYSTEM_VOLUME_MIXER_AVAILABLE=0"]
    subprocess.run(["make", "-j8", "CFLAGS=" + " ".join(cflags), "zelda3"],
                   cwd=output, check=True, stdout=subprocess.DEVNULL)

    # The linked inspection export needs the PPU's winning BG priority words
    # before the next scanline overwrites them. Keep the hook in this ignored
    # local engine copy, after its ordinary executable has already linked.
    ppu_text = ppu_source.read_text()
    signature = "static NOINLINE void PpuDrawWholeLine(Ppu *ppu, uint y) {"
    if ppu_text.count(signature) != 1:
        raise ValueError("reference PPU has an unexpected whole-line renderer")
    if "extern void AlttpCaptureLine(Ppu *ppu, unsigned line);" not in ppu_text:
        ppu_text = ppu_text.replace(signature,
                                    "extern void AlttpCaptureLine(Ppu *ppu, unsigned line);\n" +
                                    signature, 1)
    endpoint = "  // Clear out stuff on the sides.\n"
    if ppu_text.count(endpoint) != 1:
        raise ValueError("reference PPU has an unexpected line endpoint")
    if "AlttpCaptureLine(ppu, y - 1);" not in ppu_text:
        ppu_text = ppu_text.replace(endpoint,
                                    "  AlttpCaptureLine(ppu, y - 1);\n" + endpoint, 1)
    ppu_source.write_text(ppu_text)
    subprocess.run(["clang", *cflags, "-c", "snes/ppu.c", "-o", "snes/ppu.o"],
                   cwd=output, check=True)

    # Compare the room tilemaps around each object dispatch. The exported
    # last-writer IDs let geometry reconstruction distinguish identical art
    # placed by different wall/floor/door objects.
    dungeon_text = dungeon_source.read_text()
    if "AlttpTraceRoomObjectBegin" not in dungeon_text:
        patches = (
            ("void Dungeon_LoadRoom() {  // 81873a",
             "extern void AlttpTraceRoomReset(void);\n"
             "extern void AlttpTraceRoomPhase(unsigned phase);\n"
             "extern void AlttpTraceRoomObjectBegin(unsigned offset, unsigned raw, unsigned door);\n"
             "extern void AlttpTraceRoomObjectEnd(void);\n"
             "void Dungeon_LoadRoom() {  // 81873a\n  AlttpTraceRoomReset();"),
            ("  RoomDraw_DrawAllObjects(cur_p1);", "  AlttpTraceRoomPhase(0);\n  RoomDraw_DrawAllObjects(cur_p1);"),
            ("  RoomDraw_DrawAllObjects(cur_p0);  // Draw Layer 1 objects to BG2",
             "  AlttpTraceRoomPhase(1);\n  RoomDraw_DrawAllObjects(cur_p0);  // Draw Layer 1 objects to BG2"),
            ("  RoomDraw_DrawAllObjects(cur_p0);  // Draw Layer 2 objects to BG2",
             "  AlttpTraceRoomPhase(2);\n  RoomDraw_DrawAllObjects(cur_p0);  // Draw Layer 2 objects to BG2"),
            ("  RoomDraw_DrawAllObjects(cur_p0);  // Draw Layer 3 objects to BG2",
             "  AlttpTraceRoomPhase(3);\n  RoomDraw_DrawAllObjects(cur_p0);  // Draw Layer 3 objects to BG2"),
            ("    RoomData_DrawObject(d, level_data);",
             "    AlttpTraceRoomObjectBegin(dung_load_ptr_offs, d, 0);\n"
             "    RoomData_DrawObject(d, level_data);\n    AlttpTraceRoomObjectEnd();"),
            ("    RoomData_DrawObject_Door(d);",
             "    AlttpTraceRoomObjectBegin(dung_load_ptr_offs, d, 1);\n"
             "    RoomData_DrawObject_Door(d);\n    AlttpTraceRoomObjectEnd();"),
        )
        for old, new in patches:
            if dungeon_text.count(old) != 1:
                raise ValueError(f"reference dungeon source changed near {old!r}")
            dungeon_text = dungeon_text.replace(old, new, 1)
    if "AlttpTraceRoomLoaded();" not in dungeon_text:
        declaration = "extern void AlttpTraceRoomLoaded(void);\n"
        if dungeon_text.count("void Dungeon_LoadRoom() {  // 81873a") != 1:
            raise ValueError("reference dungeon room loader changed")
        dungeon_text = dungeon_text.replace("void Dungeon_LoadRoom() {  // 81873a",
                                            declaration + "void Dungeon_LoadRoom() {  // 81873a", 1)
        ending = "  dung_load_ptr_offs = 0x120;\n}"
        if dungeon_text.count(ending) != 1:
            raise ValueError("reference dungeon room loader ending changed")
        dungeon_text = dungeon_text.replace(ending,
                                            "  dung_load_ptr_offs = 0x120;\n  AlttpTraceRoomLoaded();\n}", 1)
    dungeon_source.write_text(dungeon_text)
    subprocess.run(["clang", *cflags, "-c", "src/dungeon.c", "-o", "src/dungeon.o"],
                   cwd=output, check=True)

    adapter = Path(__file__).with_name("alttp_room_preview_adapter.c").resolve()
    subprocess.run(["clang", *cflags, "-I", str(output), "-c", str(adapter),
                    "-o", str(output / "room_adapter.o")], cwd=output, check=True)
    objects = ([file for file in (output / "src").glob("*.o") if file.name != "main.o"] +
               list((output / "snes").glob("*.o")) +
               [output / "third_party/gl_core/gl_core_3_1.o",
                output / "third_party/opus-1.3.1-stripped/opus_decoder_amalgam.o",
                output / "room_adapter.o"])
    result = output / "alttp-room-preview"
    subprocess.run(["clang", *(str(file) for file in objects), *sdl_libs,
                    "-lm", "-o", str(result)], cwd=output, check=True)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True,
                        help="local snesrev/zelda3 source checkout")
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="ignored directory for engine and ROM-derived assets")
    parser.add_argument("--python", default=sys.executable,
                        help="Python with PyYAML and Pillow for Zelda3 asset extraction")
    parser.add_argument("--pythonpath", help="optional package path for asset extraction")
    parser.add_argument("--asset-cache", type=Path,
                        help="reuse extracted assets from a local engine with the same verified ROM")
    args = parser.parse_args()
    print(build(args.source.resolve(), args.rom.resolve(), args.output.resolve(),
                args.python, args.pythonpath,
                args.asset_cache.resolve() if args.asset_cache else None))


if __name__ == "__main__":
    main()
