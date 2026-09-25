"""Decode the 160 US ALTTP overworld Map32 quadrants into Map8 tile words.

The high/low compressed stream pointer tables at $82:F94D/$82:FB2D and the
Map16-to-Map8 table at $8F:8000 are documented by snesrev/zelda3
``assets/compile_resources.py`` (print_overworld, print_misc). The four packed
Map32-to-Map16 tables and their 6-byte groups come from
``assets/extract_resources.py`` (print_map32_to_map16). The layout order is
confirmed by ``src/overworld.c`` (Overworld_DecompressAndDrawOneQuadrant,
Overworld_ParseMap32Definition, OverworldCopyMap16ToBuffer). The compression
format and big-endian Map32 copy offsets come from ``assets/util.py`` (decomp).

https://github.com/snesrev/zelda3/blob/master/assets/compile_resources.py
https://github.com/snesrev/zelda3/blob/master/assets/extract_resources.py
https://github.com/snesrev/zelda3/blob/master/src/overworld.c
https://github.com/snesrev/zelda3/blob/master/assets/util.py

These references are MIT licensed, copyright (c) snesrev contributors. This
module reads base ROM quadrants. Overlays and gameplay changes are separate
runtime layers and are not baked into the returned tile words.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterator, Protocol


class RomReader(Protocol):
    def get_byte(self, address: int) -> int: ...
    def get_word(self, address: int) -> int: ...
    def get_24(self, address: int) -> int: ...
    def read_bytes(self, address: int, length: int) -> bytes: ...


QUADRANT_COUNT = 160
MAP32_SIDE = 16
MAP16_SIDE = 32
MAP8_SIDE = 64
MAP32_DEFINITION_COUNT = 2218 * 4
MAP16_DEFINITION_COUNT = 3752
HIGH_POINTER_TABLE = 0x82F94D
LOW_POINTER_TABLE = 0x82FB2D
MAP32_TO_MAP16_TABLES = (0x838000, 0x83B400, 0x848000, 0x84B400)
MAP16_TO_MAP8_TABLE = 0x8F8000


@dataclass(frozen=True, slots=True)
class OverworldTile:
    quadrant_index: int
    x: int  # 8x8 tile coordinates within the 512x512-pixel quadrant
    y: int
    map32_x: int
    map32_y: int
    map32_id: int
    map16_x: int
    map16_y: int
    map16_id: int
    word: int

    @property
    def tile_number(self) -> int:
        return self.word & 0x03FF

    @property
    def palette(self) -> int:
        return self.word >> 10 & 7

    @property
    def priority(self) -> bool:
        return bool(self.word & 0x2000)

    @property
    def flip_x(self) -> bool:
        return bool(self.word & 0x4000)

    @property
    def flip_y(self) -> bool:
        return bool(self.word & 0x8000)


@dataclass(frozen=True, slots=True)
class OverworldQuadrant:
    index: int
    high_stream_address: int
    low_stream_address: int
    high_stream_length: int
    low_stream_length: int
    high_decoded_length: int
    low_decoded_length: int
    map32_ids: tuple[int, ...]  # row-major 16x16
    map16_ids: tuple[int, ...]  # row-major 32x32
    map8_words: tuple[int, ...]  # row-major 64x64

    def tile_at(self, x: int, y: int) -> OverworldTile:
        """Return one tile word with the Map32 and Map16 IDs that produced it."""
        if not (0 <= x < MAP8_SIDE and 0 <= y < MAP8_SIDE):
            raise IndexError("Map8 tile coordinate is outside the quadrant")
        x16, y16 = x // 2, y // 2
        x32, y32 = x // 4, y // 4
        return OverworldTile(
            self.index, x, y, x32, y32,
            self.map32_ids[y32 * MAP32_SIDE + x32],
            x16, y16, self.map16_ids[y16 * MAP16_SIDE + x16],
            self.map8_words[y * MAP8_SIDE + x],
        )

    def iter_tiles(self) -> Iterator[OverworldTile]:
        for y in range(MAP8_SIDE):
            for x in range(MAP8_SIDE):
                yield self.tile_at(x, y)


def _next_address(address: int) -> int:
    address += 1
    if (address & 0xFFFF) == 0:
        address += 0x8000
    return address


def decode_map32_stream(rom: RomReader, address: int, *, max_input: int = 0x2000) -> tuple[bytes, int]:
    """Decode a quadrant plane; copy offsets are big-endian in these streams."""
    result = bytearray()
    consumed = 0

    def next_byte() -> int:
        nonlocal address, consumed
        if consumed >= max_input:
            raise ValueError("overworld stream exceeds compressed input limit")
        value = rom.get_byte(address)
        address = _next_address(address)
        consumed += 1
        return value

    while True:
        command_byte = next_byte()
        if command_byte == 0xFF:
            if len(result) < MAP32_SIDE * MAP32_SIDE:
                raise ValueError(f"overworld stream decoded only {len(result)} bytes")
            return bytes(result), consumed
        if (command_byte & 0xE0) == 0xE0:
            command = (command_byte << 3) & 0xE0
            length = (((command_byte & 3) << 8) | next_byte()) + 1
        else:
            command = command_byte & 0xE0
            length = (command_byte & 0x1F) + 1
        if len(result) + length > MAP32_SIDE * MAP32_SIDE + 2:
            raise ValueError("overworld stream expands beyond 258 bytes")
        if command == 0:  # literal
            result.extend(next_byte() for _ in range(length))
        elif command & 0x80:  # overlapping copy, big-endian offset
            offset = (next_byte() << 8) | next_byte()
            for i in range(length):
                if offset + i >= len(result):
                    raise ValueError("overworld stream has out-of-range back-reference")
                result.append(result[offset + i])
        elif not command & 0x40:  # repeated byte
            result.extend((next_byte(),) * length)
        elif not command & 0x20:  # alternating pair
            first, second = next_byte(), next_byte()
            result.extend((first, second)[i & 1] for i in range(length))
        else:  # ascending run
            value = next_byte()
            result.extend((value + i) & 0xFF for i in range(length))


def _decode_map32_definition(data: bytes, group: int, variant: int) -> int:
    offset = group * 6
    low = data[offset + variant]
    high = data[offset + 4 + variant // 2]
    return low | ((high >> 4 if variant % 2 == 0 else high & 0x0F) << 8)


def _read_definition_tables(rom: RomReader) -> tuple[tuple[bytes, ...], tuple[int, ...]]:
    packed_length = (MAP32_DEFINITION_COUNT // 4) * 6
    map32_tables = tuple(rom.read_bytes(address, packed_length)
                         for address in MAP32_TO_MAP16_TABLES)
    if any(len(table) != packed_length for table in map32_tables):
        raise ValueError("incomplete Map32 definition table")
    map8_bytes = rom.read_bytes(MAP16_TO_MAP8_TABLE, MAP16_DEFINITION_COUNT * 4 * 2)
    if len(map8_bytes) != MAP16_DEFINITION_COUNT * 8:
        raise ValueError("incomplete Map16 definition table")
    map16_to_map8 = tuple(map8_bytes[i] | (map8_bytes[i + 1] << 8)
                          for i in range(0, len(map8_bytes), 2))
    return map32_tables, map16_to_map8


def _expand_quadrant(map32_ids: tuple[int, ...], map32_tables: tuple[bytes, ...],
                     map16_to_map8: tuple[int, ...]) -> tuple[tuple[int, ...], tuple[int, ...]]:
    map16 = [0] * (MAP16_SIDE * MAP16_SIDE)
    for y32 in range(MAP32_SIDE):
        for x32 in range(MAP32_SIDE):
            definition_id = map32_ids[y32 * MAP32_SIDE + x32]
            if definition_id >= MAP32_DEFINITION_COUNT:
                raise ValueError(f"Map32 ID {definition_id} exceeds definition table")
            group, variant = divmod(definition_id, 4)
            x16, y16 = x32 * 2, y32 * 2
            for quadrant, table in enumerate(map32_tables):
                map16_id = _decode_map32_definition(table, group, variant)
                if map16_id >= MAP16_DEFINITION_COUNT:
                    raise ValueError(f"Map16 ID {map16_id} exceeds definition table")
                map16[(y16 + quadrant // 2) * MAP16_SIDE + x16 + quadrant % 2] = map16_id

    map8 = [0] * (MAP8_SIDE * MAP8_SIDE)
    for y16 in range(MAP16_SIDE):
        for x16 in range(MAP16_SIDE):
            definition_id = map16[y16 * MAP16_SIDE + x16]
            source = definition_id * 4
            x8, y8 = x16 * 2, y16 * 2
            for part in range(4):
                map8[(y8 + part // 2) * MAP8_SIDE + x8 + part % 2] = map16_to_map8[source + part]
    return tuple(map16), tuple(map8)


def extract_overworld(rom: RomReader) -> list[OverworldQuadrant]:
    """Return every base Map32 quadrant and its expanded Map16/Map8 contents."""
    map32_tables, map16_to_map8 = _read_definition_tables(rom)
    result = []
    for index in range(QUADRANT_COUNT):
        high_address = rom.get_24(HIGH_POINTER_TABLE + index * 3)
        low_address = rom.get_24(LOW_POINTER_TABLE + index * 3)
        if not (high_address & 0x8000 and low_address & 0x8000):
            raise ValueError(f"quadrant {index} has invalid LoROM stream pointers")
        try:
            high, high_length = decode_map32_stream(rom, high_address)
            low, low_length = decode_map32_stream(rom, low_address)
        except (IndexError, ValueError) as error:
            raise ValueError(f"quadrant {index}: {error}") from error
        if len(high) != (258 if index == 71 else 256) or len(low) != 256:
            raise ValueError(f"quadrant {index} has unexpected Map32 plane length")
        map32 = tuple(low[i] | high[i] << 8 for i in range(MAP32_SIDE * MAP32_SIDE))
        map16, map8 = _expand_quadrant(map32, map32_tables, map16_to_map8)
        result.append(OverworldQuadrant(index, high_address, low_address,
                                        high_length, low_length, len(high), len(low),
                                        map32, map16, map8))
    return result
