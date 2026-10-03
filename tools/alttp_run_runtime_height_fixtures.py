#!/usr/bin/env python3
"""Run the exact C++ runtime solve and mandatory validation-v2 on pinned local fixtures."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.is_dir():
        parser.error("output directory must already exist")
    # Shared authoring may regenerate the selected candidate while a fixture
    # is solving. Pin one immutable byte snapshot for ALL rooms and validator
    # calls, and record whether the original changed; never accept mixed inputs.
    source_profile = args.profile
    pinned_profile = args.output / "fixture-input-profile.toml"
    if pinned_profile.resolve() == source_profile.resolve():
        parser.error("profile must be distinct from the fixture output copy")
    profile_bytes = source_profile.read_bytes()
    pinned_profile.write_bytes(profile_bytes)
    args.profile = pinned_profile
    failed = False
    for room in (0x55, 0x60, 0x61):
        prefix = args.fixtures / f"room-{room:03x}"
        raw = Path(str(prefix) + "-door-input.raw")
        state_path = Path(str(raw) + ".state.json")
        state = json.loads(state_path.read_text())
        command = [str(args.solver), str(raw), str(raw) + ".attr", str(args.profile),
                   *map(str, state["link_xy"]), str(state["plane"])]
        for stair in state["stairs"]:
            command += [*map(str, stair["tile_xy"]), str(int(stair["high_side"] == "north")),
                        str(int(state["stair_kind"] == 1))]
        solution = args.output / f"room-{room:03x}-runtime.json"
        with solution.open("w") as output:
            subprocess.run(command, stdout=output, check=True)
        solved = json.loads(solution.read_text())
        planes = [[255] * 4096 for _ in range(2)]
        visible = [255] * 4096
        for plane, x, y, base in solved["cells_plane_x_y_base"]:
            index = y * 64 + x
            planes[plane][index] = base
            visible[index] = base if visible[index] == 255 else max(visible[index], base)
        for placement in solved["structural_placements"]:
            visible[placement["y"] * 64 + placement["x"]] = placement["floor_base"]
        compiled = args.output / f"room-{room:03x}-runtime.bin"
        compiled.write_bytes(b"ALTPHM2\0" + struct.pack("<H", room) +
                             bytes(visible) + bytes(planes[0]) + bytes(planes[1]))
        report = args.output / f"room-{room:03x}-validation-v2.json"
        validation = [sys.executable, "-B", "tools/alttp_validate_room_heights.py",
                      "--graph", str(prefix) + "-doorway-graph.json", "--snapshot", str(raw),
                      "--attributes", str(raw) + ".attr", "--state", str(state_path),
                      "--profile", str(args.profile), "--runtime-solution", str(solution),
                      "--height-map", str(compiled),
                      "--output", str(report), "--diagnostic-svg", str(report.with_suffix(".svg"))]
        result = subprocess.run(validation, stdout=subprocess.DEVNULL)
        failed |= result.returncode != 0
        if report.exists():
            data = json.loads(report.read_text())
            print(f"room {room:03x}: exit={result.returncode} support={data['valid_walkable_placements']}/"
                  f"{data['required_walkable_placements']} missing={len(data['missing_placements'])} "
                  f"conflicts={len(data['conflicting_components'])} walls={data['solved_wall_cells']} "
                  f"unknown_walls={data['unknown_wall_cells']} planes={data['per_plane_walkable_denominator']}")
    changed = source_profile.read_bytes() != profile_bytes
    (args.output / "fixture-input-manifest.json").write_text(json.dumps({
        "source_profile": str(source_profile), "pinned_profile": str(pinned_profile),
        "profile_sha256": hashlib.sha256(profile_bytes).hexdigest(),
        "source_changed_during_run": changed,
        "validation_failed": failed}, indent=2) + "\n")
    if changed:
        print("Source profile changed during run; results use the recorded pinned copy.")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
