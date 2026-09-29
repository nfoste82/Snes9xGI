#!/usr/bin/env python3
"""Compile a successful validation-v2 placement report to plane-aware ALTPHM2."""

import argparse
import json
import struct
from pathlib import Path

SIZE = 64
UNKNOWN = 255


def compile_map(report: dict) -> bytes:
    if report.get("format") != "alttp-room-height-validation-v2" or not report.get("success"):
        raise ValueError("input must be a successful validation-v2 report")
    room = report.get("room_id")
    if not isinstance(room, int) or not 0 <= room < 320:
        raise ValueError("invalid room ID")
    visible = [UNKNOWN] * (SIZE * SIZE)
    planes = [[UNKNOWN] * (SIZE * SIZE) for _ in range(2)]
    for placement in report.get("placements", []):
        x, y, base = placement.get("x"), placement.get("y"), placement.get("floor_base")
        if not all(isinstance(value, int) for value in (x, y, base)) or not (0 <= x < SIZE and 0 <= y < SIZE and 0 <= base < UNKNOWN):
            raise ValueError("invalid placement coordinate or floor base")
        placement_planes = [placement["plane"]] if "plane" in placement else placement.get("planes", [])
        if not placement_planes or any(plane not in (0, 1) for plane in placement_planes):
            raise ValueError("placement has no valid gameplay plane")
        index = y * SIZE + x
        for plane in placement_planes:
            old = planes[plane][index]
            if old != UNKNOWN and old != base:
                raise ValueError(f"conflicting plane base at ({x},{y}) plane {plane}")
            planes[plane][index] = base
        visible[index] = base if visible[index] == UNKNOWN else max(visible[index], base)
    return b"ALTPHM2\0" + struct.pack("<H", room) + bytes(visible + planes[0] + planes[1])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("validation", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_bytes(compile_map(json.loads(args.validation.read_text())))


if __name__ == "__main__":
    main()
