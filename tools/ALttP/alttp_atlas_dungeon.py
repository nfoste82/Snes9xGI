"""Partial ALTTP USA dungeon tilemap expansion with explicit omissions.

Sources: snesrev/zelda3 assets/compile_resources.py:145-152 and
src/dungeon.c:504-517, 2597-2618, 2682-2743, 2823-2827, 3461-3464.
https://github.com/snesrev/zelda3/blob/master/assets/compile_resources.py
https://github.com/snesrev/zelda3/blob/master/src/dungeon.c

The caller supplies a SHA256-verified USA ROM reader and room/object/door
records from alttp_atlas_world.catalog_world. The output is a *partial* map:
the game also draws default layouts, many other object handlers, doors and
state-dependent replacements. No returned room is marked complete.
"""

from __future__ import annotations

from typing import Any, Iterable, Mapping

from alttp_atlas_world import CatalogError, _long, _room_objects


MAP_SIDE = 64
MAP_WORDS = MAP_SIDE * MAP_SIDE
PREDEFINED_TILES = 0x009B52
PREDEFINED_WORD_COUNT = 6438
CEILING_TEMPLATE_OFFSET = 0x03D8
DEFAULT_LAYOUT_POINTERS = 0x84EF2F


class DungeonExpansionError(ValueError):
    """A required ROM tile word or catalog record is invalid."""


def catalog_default_layouts(
    reader: Any, *, max_stream_bytes: int = 0x8000
) -> dict[str, list[dict[str, Any]]]:
    """Catalog the USA ROM's eight default dungeon object streams.

    The pointer table is at ``$84EF2F``. Each pointer leads to one bounded
    object stream with a ``$FFFF`` terminator; the original extractor asserts
    that these streams contain no door section. Objects are *not* drawn here.
    Both result lists can be stored directly as atlas world records.

    This reuses alttp_atlas_world's checked decoder so room and default-layout
    object encodings cannot silently diverge.
    """
    if not isinstance(max_stream_bytes, int) or not 3 <= max_stream_bytes <= 0x8000:
        raise ValueError("max_stream_bytes must be an integer from 3 through 32768")
    layouts: list[dict[str, Any]] = []
    objects: list[dict[str, Any]] = []
    for layout_id in range(8):
        pointer_address = DEFAULT_LAYOUT_POINTERS + layout_id * 3
        source_address = _long(reader, pointer_address)
        end_address, raw_objects, doors = _room_objects(
            reader, layout_id, 1, source_address, max_stream_bytes
        )
        if doors:
            raise CatalogError(
                f"default layout {layout_id} unexpectedly contains {len(doors)} doors"
            )
        layouts.append({
            "id": f"dungeon:default:{layout_id}",
            "layout_id": layout_id,
            "source_address": source_address,
            "pointer_address": pointer_address,
            "end_address": end_address,
            "object_count": len(raw_objects),
        })
        for raw_object in raw_objects:
            item = {key: value for key, value in raw_object.items()
                    if key not in ("room_id", "layer")}
            item.update({
                "id": f"dungeon:default:{layout_id}:object:{raw_object['ordinal']:03d}",
                "layout_id": layout_id,
                "layout_source_address": source_address,
                "pointer_address": pointer_address,
            })
            objects.append(item)
    return {"dungeon_default_layouts": layouts, "dungeon_default_objects": objects}


def _template_words(reader: Any, offset: int, count: int) -> tuple[int, ...]:
    if offset < 0 or offset & 1 or count < 0 or offset // 2 + count > PREDEFINED_WORD_COUNT:
        raise DungeonExpansionError(f"predefined tile span out of bounds: {offset:#x}, {count}")
    words = []
    for i in range(count):
        address = PREDEFINED_TILES + offset + i * 2
        try:
            word = reader.get_word(address)
        except (AssertionError, IndexError, KeyError, ValueError) as exc:
            raise DungeonExpansionError(f"cannot read predefined tile at {address:06X}") from exc
        if not isinstance(word, int) or not 0 <= word <= 0xFFFF:
            raise DungeonExpansionError(f"invalid predefined tile at {address:06X}")
        words.append(word)
    return tuple(words)


def _floor_layer(reader: Any, floor_index: int) -> list[int]:
    """Repeat the runtime's 4x2 source pattern over one 64x64 BG map."""
    if not isinstance(floor_index, int) or not 0 <= floor_index < 16:
        raise DungeonExpansionError(f"invalid floor index: {floor_index!r}")
    pattern = _template_words(reader, floor_index << 4, 8)
    return [pattern[(y & 1) * 4 + (x & 3)]
            for y in range(MAP_SIDE) for x in range(MAP_SIDE)]


def _ceiling_writes(reader: Any, obj: Mapping[str, Any]) -> list[tuple[int, int]]:
    """Draw runtime Type 1/Subtype 1 ID 00, a repeated 2x2 ceiling.

    The world catalog calls this ``subtype == 0``; runtime size bits are
    ``(raw_a & 3) << 2 | (raw_b & 3)``.
    """
    x, y = obj.get("x"), obj.get("y")
    width, height = obj.get("width_code"), obj.get("height_code")
    if any(not isinstance(v, int) for v in (x, y, width, height)) or not (
        0 <= x < MAP_SIDE and 0 <= y < MAP_SIDE and
        0 <= width <= 3 and 0 <= height <= 3
    ):
        raise DungeonExpansionError(f"invalid ceiling object position/size: {obj.get('id')}")
    repeats = (width << 2) | height
    repeats = repeats or 32
    if x + repeats * 2 > MAP_SIDE or y + 2 > MAP_SIDE:
        raise DungeonExpansionError(f"ceiling object exceeds 64x64 tilemap: {obj.get('id')}")
    top_left, bottom_left, top_right, bottom_right = _template_words(
        reader, CEILING_TEMPLATE_OFFSET, 4
    )
    writes = []
    for repeat in range(repeats):
        position = y * MAP_SIDE + x + repeat * 2
        writes.extend(((position, top_left), (position + MAP_SIDE, bottom_left),
                       (position + 1, top_right), (position + MAP_SIDE + 1, bottom_right)))
    return writes


def expand_dungeon_room(
    reader: Any,
    room: Mapping[str, Any],
    objects: Iterable[Mapping[str, Any]],
    doors: Iterable[Mapping[str, Any]],
) -> dict[str, Any]:
    """Expand floors and ceiling ID 00; enumerate all missing draw work.

    ``bg1`` and ``bg2`` are 4096 SNES BG tilemap words, row major. They are
    *partial* room maps, never a complete rendering. The ordered ``unsupported``
    entries make the missing default-layout pass, object/door handlers, and
    dynamic updates inspectable. Runtime layers 1 and 2 write BG2; layer 3
    writes BG1. Supported object writes retain their catalog source address.
    """
    room_id = room.get("room_id")
    if not isinstance(room_id, int) or not 0 <= room_id < 320:
        raise DungeonExpansionError(f"invalid dungeon room ID: {room_id!r}")
    source_address = room.get("source_address")
    if not isinstance(source_address, int):
        raise DungeonExpansionError("room is missing source_address")
    bg1 = _floor_layer(reader, room.get("floor1"))
    bg2 = _floor_layer(reader, room.get("floor2"))
    layout = room.get("layout")
    if not isinstance(layout, int) or not 0 <= layout < 8:
        raise DungeonExpansionError(f"invalid default layout: {layout!r}")
    default_pointer_address = DEFAULT_LAYOUT_POINTERS + 3 * layout
    unsupported: list[dict[str, Any]] = [{
        "kind": "default_layout", "id": f"dungeon:default:{layout}",
        "source_address": _long(reader, default_pointer_address),
        "pointer_address": default_pointer_address,
        "reason": "default layout objects are not expanded",
    }]
    object_writes: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for obj in sorted(objects, key=lambda item: (item["layer"], item["ordinal"])):
        if obj.get("room_id") != room_id or obj.get("layer") not in (1, 2, 3):
            raise DungeonExpansionError(f"object belongs to another room/layer: {obj.get('id')}")
        object_id = obj.get("id")
        if not isinstance(object_id, str) or object_id in seen_ids:
            raise DungeonExpansionError(f"duplicate or invalid object ID: {object_id!r}")
        seen_ids.add(object_id)
        if not isinstance(obj.get("source_address"), int):
            raise DungeonExpansionError(f"object has no source address: {object_id}")
        if obj.get("subtype") == 0 and obj.get("object_id") == 0:
            writes = _ceiling_writes(reader, obj)
            layer = "bg1" if obj["layer"] == 3 else "bg2"
            destination = bg1 if layer == "bg1" else bg2
            for index, word in writes:
                destination[index] = word
            object_writes.append({
                "id": object_id,
                "source_address": obj["source_address"],
                "layer": obj["layer"],
                "bg": layer,
                "handler": "type1_subtype1_00_ceiling",
                "tile_writes": [[index, word] for index, word in writes],
            })
        else:
            unsupported.append({
                "kind": "object", "id": object_id,
                "source_address": obj["source_address"],
                "layer": obj["layer"], "subtype": obj.get("subtype"),
                "object_id": obj.get("object_id"),
                "reason": "object handler is not implemented",
            })
    for door in doors:
        if door.get("room_id") != room_id or not isinstance(door.get("source_address"), int):
            raise DungeonExpansionError(f"invalid door record: {door.get('id')}")
        unsupported.append({
            "kind": "door", "id": door.get("id"),
            "source_address": door["source_address"],
            "layer": door.get("layer"), "door_type": door.get("door_type"),
            "reason": "door handler and save-state remap are not implemented",
        })
    unsupported.append({
        "kind": "runtime_state", "id": f"dungeon:{room_id:03d}:runtime_state",
        "source_address": source_address,
        "reason": "push blocks, torches, tags and save-state replacements are not applied",
    })
    return {
        "id": f"dungeon:{room_id:03d}:partial_tilemap",
        "room_id": room_id, "source_address": source_address,
        "complete": False, "width": MAP_SIDE, "height": MAP_SIDE,
        "floor_sources": {
            "bg1_pattern_address": PREDEFINED_TILES + room["floor1"] * 16,
            "bg2_pattern_address": PREDEFINED_TILES + room["floor2"] * 16,
            "pattern_words": 8,
        },
        "bg1": bg1, "bg2": bg2,
        "object_writes": object_writes,
        "unsupported": unsupported,
    }
