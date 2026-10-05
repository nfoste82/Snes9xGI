"""Bounded, provenance-preserving catalog of ALTTP USA world records.

The address layout follows snesrev/zelda3's ROM extractor:
https://github.com/snesrev/zelda3/blob/master/assets/extract_resources.py
The caller supplies a validated USA ROM and a reader of SNES addresses with
``get_byte``, ``get_word``, ``get_24``, and ``read_bytes`` methods. This module
does not expand dungeon objects into rendered tilemaps or infer reachability.
"""

from __future__ import annotations

from typing import Any


class CatalogError(ValueError):
    """A ROM pointer or record stream was malformed or exceeded its bound."""


ROOM_COUNT = 320
AREA_COUNT = 160
AREA_HEAD_TABLE = 0x82A5EC
AREA_SIZE_TABLE = 0x82F88D
ROOM_DATA_POINTERS = 0x1F8000
ROOM_META_POINTERS = 0x04F502
ROOM_SPRITE_POINTERS = 0x89D62E
ROOM_META_FALLBACK = 0x82EDC5


def _byte(reader: Any, address: int) -> int:
    try:
        value = reader.get_byte(address)
    except (AssertionError, IndexError, KeyError, ValueError) as exc:
        raise CatalogError(f"unreadable ROM byte at {address:06X}") from exc
    if not isinstance(value, int) or not 0 <= value <= 0xFF:
        raise CatalogError(f"invalid ROM byte at {address:06X}: {value!r}")
    return value


def _word(reader: Any, address: int) -> int:
    try:
        value = reader.get_word(address)
    except (AssertionError, IndexError, KeyError, ValueError) as exc:
        raise CatalogError(f"unreadable ROM word at {address:06X}") from exc
    if not isinstance(value, int) or not 0 <= value <= 0xFFFF:
        raise CatalogError(f"invalid ROM word at {address:06X}: {value!r}")
    return value


def _long(reader: Any, address: int) -> int:
    try:
        value = reader.get_24(address)
    except (AssertionError, IndexError, KeyError, ValueError) as exc:
        raise CatalogError(f"unreadable ROM pointer at {address:06X}") from exc
    if not isinstance(value, int) or not 0 <= value <= 0xFFFFFF:
        raise CatalogError(f"invalid ROM pointer at {address:06X}: {value!r}")
    return value


def _record(reader: Any, address: int, count: int, end: int) -> tuple[int, ...]:
    if address + count > end:
        raise CatalogError(f"record at {address:06X} exceeds stream bound {end:06X}")
    try:
        data = reader.read_bytes(address, count)
    except (AssertionError, IndexError, KeyError, ValueError) as exc:
        raise CatalogError(f"unreadable ROM record at {address:06X}") from exc
    if len(data) != count:
        raise CatalogError(f"short ROM record at {address:06X}")
    values = tuple(data)
    if any(not isinstance(v, int) or not 0 <= v <= 0xFF for v in values):
        raise CatalogError(f"invalid ROM record at {address:06X}")
    return values


def _stream_end(start: int, max_stream_bytes: int) -> int:
    if start & 0x8000 == 0:
        raise CatalogError(f"stream starts outside LoROM ROM window: {start:06X}")
    # These sources are bank-local. A missing sentinel must not walk into the
    # next table or an unrelated bank. The caller may impose a tighter cap.
    return min(start + max_stream_bytes, (start & 0xFF0000) + 0x10000)


def _room_objects(
    reader: Any, room_id: int, layer: int, start: int, max_stream_bytes: int
) -> tuple[int, list[dict[str, Any]], list[dict[str, Any]]]:
    """Read object and door records using extract_resources.py:243-282."""
    end = _stream_end(start, max_stream_bytes)
    pos = start
    objects: list[dict[str, Any]] = []
    doors: list[dict[str, Any]] = []
    in_doors = False
    while True:
        a, b = _record(reader, pos, 2, end)
        word = a | b << 8
        if word == 0xFFFF:
            return pos + 2, objects, doors
        if not in_doors and word == 0xFFF0:
            in_doors = True
            pos += 2
            continue
        if in_doors:
            doors.append({
                "id": f"dungeon:{room_id:03d}:layer:{layer}:door:{len(doors):03d}",
                "room_id": room_id,
                "layer": layer,
                "ordinal": len(doors),
                "source_address": pos,
                "raw_a": a,
                "raw_b": b,
                "position": a >> 4,
                "direction": word & 3,
                "door_type": b,
            })
            pos += 2
            continue

        a, b, c = _record(reader, pos, 3, end)
        word = a | b << 8
        if (word & 0xFC) == 0xFC:
            subtype = 2
            x = (a << 4 | b >> 4) & 0x3F
            y = (b << 2 | c >> 6) & 0x3F
            object_id = c & 0x3F
            width = height = None
        else:
            x, y = a >> 2, b >> 2
            width, height = a & 3, b & 3
            if c < 0xF8:
                subtype, object_id = 0, c
            else:
                subtype = 1
                object_id = ((c & 7) << 4) | (height << 2) | width
        objects.append({
            "id": f"dungeon:{room_id:03d}:layer:{layer}:object:{len(objects):03d}",
            "room_id": room_id,
            "layer": layer,
            "ordinal": len(objects),
            "source_address": pos,
            "raw_a": a,
            "raw_b": b,
            "raw_c": c,
            "subtype": subtype,
            "object_id": object_id,
            "x": x,
            "y": y,
            "width_code": width,
            "height_code": height,
        })
        pos += 3


def _dungeon_sprites(
    reader: Any, room_id: int, pointer_address: int, max_stream_bytes: int
) -> tuple[int, int, list[dict[str, Any]]]:
    """Read raw placements and key markers; see extract_resources.py:413-437."""
    start = 0x890000 | _word(reader, pointer_address)
    end = _stream_end(start, max_stream_bytes)
    sort_setting = _record(reader, start, 1, end)[0]
    pos = start + 1
    sprites: list[dict[str, Any]] = []
    last_placement_id: str | None = None
    while True:
        y = _record(reader, pos, 1, end)[0]
        if y == 0xFF:
            break
        y, x, kind = _record(reader, pos, 3, end)
        marker = kind == 0xE4 and y in (0xFD, 0xFE)
        overlord = kind != 0xE4 and x >= 0xE0
        record_id = f"dungeon:{room_id:03d}:sprite:{len(sprites):03d}"
        record = {
            "id": record_id,
            "room_id": room_id,
            "ordinal": len(sprites),
            "source_address": pos,
            "pointer_address": pointer_address,
            "raw_y": y,
            "raw_x": x,
            "raw_type": kind,
            "record_kind": "key_marker" if marker else ("overlord" if overlord else "sprite"),
            "type_id": None if marker else kind + (0x100 if overlord else 0),
            "x": None if marker else x & 0x1F,
            "y": None if marker else y & 0x1F,
            "floor": None if marker else y >> 7,
            "subtype": None if marker or overlord else (x >> 5) | ((y >> 5) & 3) << 3,
            "marker": ("big_key" if y == 0xFD else "key") if marker else None,
            "attached_to": last_placement_id if marker else None,
        }
        if marker and last_placement_id is None:
            raise CatalogError(f"orphan key marker at {pos:06X} in room {room_id}")
        if not marker:
            last_placement_id = record_id
        sprites.append(record)
        pos += 3
    return start, sort_setting, sprites


def _room(reader: Any, room_id: int, max_stream_bytes: int):
    """Read 320 room records; see extract_resources.py:375-484."""
    pointer_address = ROOM_DATA_POINTERS + 3 * room_id
    source_address = _long(reader, pointer_address)
    if source_address & 0x8000 == 0:
        raise CatalogError(f"room {room_id} has invalid data pointer {source_address:06X}")
    floor_layout = _record(reader, source_address, 2, _stream_end(source_address, max_stream_bytes))
    meta_pointer_address = ROOM_META_POINTERS + 2 * room_id
    meta_pointer = _word(reader, meta_pointer_address)
    metadata_address = ROOM_META_FALLBACK if meta_pointer == 0xFFEF else 0x040000 | meta_pointer
    metadata = _record(reader, metadata_address, 14, _stream_end(metadata_address, max_stream_bytes))
    sprite_pointer_address = ROOM_SPRITE_POINTERS + 2 * room_id
    sprite_stream_address, sort_setting, sprites = _dungeon_sprites(
        reader, room_id, sprite_pointer_address, max_stream_bytes
    )
    floor, layout = floor_layout
    room = {
        "id": f"dungeon:{room_id:03d}",
        "room_id": room_id,
        "source_address": source_address,
        "pointer_address": pointer_address,
        "metadata_address": metadata_address,
        "metadata_pointer_address": meta_pointer_address,
        "sprite_pointer_address": sprite_pointer_address,
        "sprite_stream_address": sprite_stream_address,
        "floor1": floor & 0x0F,
        "floor2": floor >> 4,
        "layout": layout >> 2,
        "start_quadrant": layout & 3,
        "metadata_raw_hex": bytes(metadata).hex(),
        "bg2_mode": metadata[0] >> 5,
        "collision": (metadata[0] >> 2) & 7,
        "lights_out": bool(metadata[0] & 1),
        "palette": metadata[1],
        "blockset": metadata[2],
        "enemy_blockset": metadata[3],
        "effect": metadata[4],
        "tag0": metadata[5],
        "tag1": metadata[6],
        "sort_sprites": sort_setting,
    }
    pos = source_address + 2
    objects: list[dict[str, Any]] = []
    doors: list[dict[str, Any]] = []
    for layer in (1, 2, 3):
        pos, layer_objects, layer_doors = _room_objects(
            reader, room_id, layer, pos, max_stream_bytes
        )
        objects.extend(layer_objects)
        doors.extend(layer_doors)
    room["objects_end_address"] = pos
    return room, objects, doors, sprites


def _overworld_sprites(
    reader: Any, area_id: int, stage: str, stage_index: int,
    pointer_address: int, max_stream_bytes: int,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    """Read one staged sprite list; see extract_resources.py:195-228."""
    start = 0x890000 | _word(reader, pointer_address)
    end = _stream_end(start, max_stream_bytes)
    gfx_address = palette_address = None
    gfx = palette = None
    if area_id < 128:
        info_stage = stage_index if area_id < 64 else 3
        gfx_address = 0x80FA41 + (area_id & 63) + info_stage * 64
        palette_address = 0x80FB41 + (area_id & 63) + info_stage * 64
        gfx, palette = _byte(reader, gfx_address), _byte(reader, palette_address)
    stage_record = {
        "id": f"overworld:{area_id:03d}:stage:{stage}",
        "area_id": area_id,
        "stage": stage,
        "stage_index": stage_index,
        "source_address": start,
        "pointer_address": pointer_address,
        "gfx_address": gfx_address,
        "palette_address": palette_address,
        "gfx_index": gfx,
        "palette_index": palette,
    }
    sprites: list[dict[str, Any]] = []
    pos = start
    while True:
        y = _record(reader, pos, 1, end)[0]
        if y == 0xFF:
            break
        y, x, kind = _record(reader, pos, 3, end)
        sprites.append({
            "id": f"overworld:{area_id:03d}:stage:{stage}:sprite:{len(sprites):03d}",
            "area_id": area_id,
            "stage": stage,
            "stage_index": stage_index,
            "ordinal": len(sprites),
            "source_address": pos,
            "pointer_address": pointer_address,
            "raw_y": y,
            "raw_x": x,
            "raw_type": kind,
            "type_id": kind,
            "x": x,
            "y": y,
            "gfx_index": gfx,
            "palette_index": palette,
        })
        pos += 3
    stage_record["end_address"] = pos + 1
    stage_record["sprite_count"] = len(sprites)
    return stage_record, sprites


def catalog_world(reader: Any, *, max_stream_bytes: int = 0x8000) -> dict[str, list[dict[str, Any]]]:
    """Enumerate raw room objects and staged world sprite placements.

    Only the verified US ROM address layout is supported. The caller must check
    the ROM hash before invoking this function. Every returned row has a stable
    ``id`` and SNES ``source_address`` for SQLite/JSON storage. A stream must
    terminate within its LoROM bank and ``max_stream_bytes``.
    """
    if not isinstance(max_stream_bytes, int) or not 3 <= max_stream_bytes <= 0x8000:
        raise ValueError("max_stream_bytes must be an integer from 3 through 32768")
    result: dict[str, list[dict[str, Any]]] = {
        "dungeon_rooms": [],
        "dungeon_objects": [],
        "dungeon_doors": [],
        "dungeon_sprites": [],
        "overworld_areas": [],
        "overworld_stages": [],
        "overworld_sprites": [],
    }
    for room_id in range(ROOM_COUNT):
        room, objects, doors, sprites = _room(reader, room_id, max_stream_bytes)
        result["dungeon_rooms"].append(room)
        result["dungeon_objects"].extend(objects)
        result["dungeon_doors"].extend(doors)
        result["dungeon_sprites"].extend(sprites)

    # A head covers a large area plus its satellite map cells. The 64-byte
    # table is mirrored for light and dark worlds; areas 128-159 are heads.
    # Source: extract_resources.py:233-238.
    heads = tuple(_byte(reader, AREA_HEAD_TABLE + i) for i in range(64))
    for area_id in range(AREA_COUNT):
        head_entry = None if area_id >= 128 else heads[area_id & 63]
        if head_entry is not None and head_entry != area_id & 63:
            continue
        size_address = AREA_SIZE_TABLE + area_id
        gfx_address = 0x80FC9C + area_id if area_id < 128 else None
        palette_address = 0x80FD1C + area_id if area_id < 136 else None
        result["overworld_areas"].append({
            "id": f"overworld:{area_id:03d}",
            "area_id": area_id,
            "source_address": size_address,
            "head_table_address": AREA_HEAD_TABLE + (area_id & 63) if area_id < 128 else None,
            "head_entry": head_entry,
            "is_small": bool(_byte(reader, size_address)),
            "gfx_address": gfx_address,
            "gfx_index": _byte(reader, gfx_address) if gfx_address is not None else None,
            "palette_address": palette_address,
            "palette_index": _byte(reader, palette_address) if palette_address is not None else None,
        })
        if area_id < 64:
            stages = (("beginning", 0, 0x89C881),
                      ("first_part", 1, 0x89C901),
                      ("second_part", 2, 0x89CA21))
        elif area_id < 144:
            stages = (("default", 2, 0x89CA21),)
        else:
            stages = ()
        for name, stage_index, base in stages:
            stage, sprites = _overworld_sprites(
                reader, area_id, name, stage_index,
                base + area_id * 2, max_stream_bytes,
            )
            result["overworld_stages"].append(stage)
            result["overworld_sprites"].extend(sprites)
    return result
