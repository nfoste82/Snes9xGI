"""ROM-backed 8x8 graphics inventory for the unmodified US A Link to the Past.

``extract_graphics(rom)`` accepts a reader whose ``get_byte``, ``get_word``,
``get_24``, and ``read_bytes`` methods take SNES LoROM addresses. It returns
unflipped palette indices and the same v1 content IDs used by remaster.h.

The graphics addresses below are transcribed from snesrev/zelda3
``assets/tables.py`` (kCompSpritePtrs and kCompBgPtrs):
https://github.com/snesrev/zelda3/blob/master/assets/tables.py
The stream format and 3bpp expansion follow ``assets/util.py`` (decomp) and
``src/load_gfx.c`` (Do3To4Low/High, LoadSpriteGraphics,
LoadBackgroundGraphics):
https://github.com/snesrev/zelda3/blob/master/assets/util.py
https://github.com/snesrev/zelda3/blob/master/src/load_gfx.c
Those source files are MIT licensed; the notice is in zelda3-MIT-LICENSE.txt.

Background packs can be loaded into either a low or high palette half,
depending on the graphics theme and VRAM slot. Uncompressed sprite packs
0..11 also have special loading paths with different conversions. Both
decoded variants are returned for these packs. This module inventories
artwork, not the state-dependent mapping from packs to object animation
frames or world placements.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


class RomReader(Protocol):
    def get_byte(self, address: int) -> int: ...
    def get_word(self, address: int) -> int: ...
    def get_24(self, address: int) -> int: ...
    def read_bytes(self, address: int, length: int) -> bytes: ...


@dataclass(frozen=True, slots=True)
class GraphicTile:
    source_kind: str
    pack_index: int | None
    tile_index: int
    snes_address: int
    conversion: str
    bit_depth: int
    indices: bytes
    content_id: str


# US ROM tables; index is the game's graphics pack number.
SPRITE_PACK_POINTERS = (
    0x10F000, 0x10F600, 0x10FC00, 0x118200, 0x118800, 0x118E00, 0x119400, 0x119A00,
    0x11A000, 0x11A600, 0x11AC00, 0x11B200, 0x14FFFC, 0x1585D4, 0x158AB6, 0x158FBE,
    0x1593F8, 0x1599A6, 0x159F32, 0x15A3D7, 0x15A8F1, 0x15AEC6, 0x15B418, 0x15B947,
    0x15BED0, 0x15C449, 0x15C975, 0x15CE7C, 0x15D394, 0x15D8AC, 0x15DDC0, 0x15E34C,
    0x15E8E8, 0x15EE31, 0x15F3A6, 0x15F92D, 0x15FEBA, 0x1682FF, 0x1688E0, 0x168E41,
    0x1692DF, 0x169883, 0x169CD0, 0x16A26E, 0x16A275, 0x16A787, 0x16AA06, 0x16AE9D,
    0x16B3FF, 0x16B87E, 0x16BE6B, 0x16C13D, 0x16C619, 0x16CBBB, 0x16D0F1, 0x16D641,
    0x16D95A, 0x16DD99, 0x16E278, 0x16E760, 0x16ED25, 0x16F20F, 0x16F6B7, 0x16FA5F,
    0x16FD29, 0x1781CD, 0x17868D, 0x178B62, 0x178FD5, 0x179527, 0x17994B, 0x179EA7,
    0x17A30E, 0x17A805, 0x17ACF8, 0x17B2A2, 0x17B7F9, 0x17BC93, 0x17C237, 0x17C78E,
    0x17CD55, 0x17D2BC, 0x17D82F, 0x17DCEC, 0x17E1CC, 0x17E36B, 0x17E842, 0x17EB38,
    0x17ED58, 0x17F06C, 0x17F4FD, 0x17FA39, 0x17FF86, 0x18845C, 0x1889A1, 0x188D64,
    0x18919D, 0x189610, 0x189857, 0x189B24, 0x189DD2, 0x18A03F, 0x18A4ED, 0x18A7BA,
    0x18AEDF, 0x18AF0D, 0x18B520, 0x18B953,
)

BACKGROUND_PACK_POINTERS = (
    0x11B800, 0x11BCE2, 0x11C15F, 0x11C675, 0x11CB84, 0x11CF4C, 0x11D2CE, 0x11D726,
    0x11D9CF, 0x11DEC4, 0x11E393, 0x11E893, 0x11ED7D, 0x11F283, 0x11F746, 0x11FC21,
    0x11FFF2, 0x128498, 0x128A0E, 0x128F30, 0x129326, 0x129804, 0x129D5B, 0x12A272,
    0x12A6FE, 0x12AA77, 0x12AD83, 0x12B167, 0x12B51D, 0x12B840, 0x12BD54, 0x12C1C9,
    0x12C73D, 0x12CC86, 0x12D198, 0x12D6B1, 0x12DB6A, 0x12E0EA, 0x12E6BD, 0x12EB51,
    0x12F135, 0x12F6C5, 0x12FC71, 0x138129, 0x138693, 0x138BAD, 0x139117, 0x139609,
    0x139B21, 0x13A074, 0x13A619, 0x13AB2B, 0x13B00C, 0x13B4F5, 0x13B9EB, 0x13BEBF,
    0x13C3CE, 0x13C817, 0x13CB68, 0x13CFB5, 0x13D460, 0x13D8C2, 0x13DD7A, 0x13E266,
    0x13E7AF, 0x13ECE5, 0x13F245, 0x13F6F0, 0x13FC30, 0x1480E9, 0x14863B, 0x148A7C,
    0x148F2A, 0x149346, 0x1497ED, 0x149CC2, 0x14A173, 0x14A61D, 0x14AB5D, 0x14B083,
    0x14B4BD, 0x14B94E, 0x14BE0E, 0x14C291, 0x14C7BA, 0x14CCE4, 0x14D1DB, 0x14D6BD,
    0x14DB77, 0x14DED1, 0x14E2AC, 0x14E754, 0x14EBAE, 0x14EF4E, 0x14F309, 0x14F6F4,
    0x14FA55, 0x14FF8C, 0x14FF93, 0x14FF9A, 0x14FFA1, 0x14FFA8, 0x14FFAF, 0x14FFB6,
    0x14FFBD, 0x14FFC4, 0x14FFCB, 0x14FFD2, 0x14FFD9, 0x14FFE0, 0x14FFE7, 0x14FFEE,
    0x14FFF5, 0x18B520, 0x18B953,
)

HIGH_SPRITE_PACKS = frozenset((0x52, 0x53, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F))
LINK_GRAPHICS_ADDRESS = 0x108000
LINK_GRAPHICS_LENGTH = 0x7000
PACK_3BPP_LENGTH = 0x600


def _next_address(address: int) -> int:
    address += 1
    if (address & 0xFFFF) == 0:
        address += 0x8000
    return address


def decode_stream(rom: RomReader, address: int, *, max_output: int = 0x4000,
                  max_input: int = 0x8000) -> tuple[bytes, int]:
    """Decode one graphics stream; return (data, consumed compressed bytes)."""
    result = bytearray()
    consumed = 0

    def next_byte() -> int:
        nonlocal address, consumed
        if consumed >= max_input:
            raise ValueError("compressed graphics stream exceeds input limit")
        value = rom.get_byte(address)
        address = _next_address(address)
        consumed += 1
        return value

    while True:
        command_byte = next_byte()
        if command_byte == 0xFF:
            return bytes(result), consumed
        if command_byte & 0xE0 == 0xE0:
            command = (command_byte << 3) & 0xE0
            length = (((command_byte & 3) << 8) | next_byte()) + 1
        else:
            command = command_byte & 0xE0
            length = (command_byte & 0x1F) + 1
        if len(result) + length > max_output:
            raise ValueError("decompressed graphics stream exceeds output limit")
        if command == 0:  # literal
            result.extend(next_byte() for _ in range(length))
        elif command & 0x80:  # overlapping copy; graphics use little-endian offset
            offset = next_byte() | (next_byte() << 8)
            for i in range(length):
                if offset + i >= len(result):
                    raise ValueError("graphics back-reference is out of range")
                result.append(result[offset + i])
        elif not command & 0x40:  # repeated byte
            result.extend((next_byte(),) * length)
        elif not command & 0x20:  # alternating pair
            first, second = next_byte(), next_byte()
            result.extend((first, second)[i & 1] for i in range(length))
        else:  # ascending run
            value = next_byte()
            result.extend((value + i) & 0xFF for i in range(length))


def decode_2bpp_tile(data: bytes) -> bytes:
    if len(data) != 16:
        raise ValueError("2bpp tile must be 16 bytes")
    return bytes((data[2*y] >> x & 1) | ((data[2*y+1] >> x & 1) << 1)
                 for y in range(8) for x in range(7, -1, -1))


def decode_3bpp_tile(data: bytes, *, high: bool = False) -> bytes:
    """Decode 24-byte 3bpp art as the game's runtime 4bpp indices."""
    if len(data) != 24:
        raise ValueError("3bpp tile must be 24 bytes")
    result = bytearray()
    for y in range(8):
        low, middle, top = data[2*y], data[2*y+1], data[16+y]
        for x in range(7, -1, -1):
            value = (low >> x & 1) | ((middle >> x & 1) << 1) | ((top >> x & 1) << 2)
            # Do3To4High sets plane 3 to the OR of source planes. Zero stays zero.
            result.append(value | (8 if high and value else 0))
    return bytes(result)


def decode_4bpp_tile(data: bytes) -> bytes:
    if len(data) != 32:
        raise ValueError("4bpp tile must be 32 bytes")
    return bytes((data[2*y] >> x & 1) | ((data[2*y+1] >> x & 1) << 1) |
                 ((data[16+2*y] >> x & 1) << 2) | ((data[17+2*y] >> x & 1) << 3)
                 for y in range(8) for x in range(7, -1, -1))


def tile_content_id(indices: bytes, bit_depth: int) -> str:
    if len(indices) != 64 or bit_depth not in (2, 4):
        raise ValueError("expected 64 canonical 2bpp or 4bpp palette indices")
    if any(index >= 1 << bit_depth for index in indices):
        raise ValueError("palette index exceeds bit depth")
    hash_value = 14695981039346656037
    for value in bytes((1, 1, bit_depth, 8, 8)) + indices:
        hash_value = ((hash_value ^ value) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"v1:{bit_depth}bpp:{hash_value:016x}"


def _records(source_kind: str, pack_index: int | None, address: int, data: bytes,
             *, bit_depth: int, conversion: str) -> list[GraphicTile]:
    bytes_per_tile = {2: 16, 3: 24, 4: 32}[bit_depth]
    if len(data) % bytes_per_tile:
        raise ValueError(f"{source_kind} pack {pack_index}: incomplete tile")
    result = []
    for tile_index in range(len(data) // bytes_per_tile):
        raw = data[tile_index * bytes_per_tile:(tile_index + 1) * bytes_per_tile]
        if bit_depth == 2:
            indices = decode_2bpp_tile(raw)
            runtime_depth = 2
        elif bit_depth == 3:
            indices = decode_3bpp_tile(raw, high=conversion == "high")
            runtime_depth = 4
        else:
            indices = decode_4bpp_tile(raw)
            runtime_depth = 4
        tile_address = address + tile_index * bytes_per_tile if source_kind == "link" else address
        result.append(GraphicTile(source_kind, pack_index, tile_index, tile_address,
                                  conversion, runtime_depth, indices,
                                  tile_content_id(indices, runtime_depth)))
    return result


def extract_graphics(rom: RomReader) -> list[GraphicTile]:
    """Extract Link, sprite, and background tile art from the known US ROM layout.

    Pack 103..107 contains 2bpp art rather than ordinary 3bpp sprites. The
    caller must validate ROM identity before relying on these US-only addresses.
    """
    records = _records("link", None, LINK_GRAPHICS_ADDRESS,
                       rom.read_bytes(LINK_GRAPHICS_ADDRESS, LINK_GRAPHICS_LENGTH),
                       bit_depth=4, conversion="native")
    for pack_index, address in enumerate(SPRITE_PACK_POINTERS):
        data = (rom.read_bytes(address, PACK_3BPP_LENGTH) if pack_index < 12 else
                decode_stream(rom, address)[0])
        if pack_index < 103:
            if len(data) != PACK_3BPP_LENGTH:
                raise ValueError(f"sprite pack {pack_index}: expected 0x600 bytes, got {len(data):#x}")
            conversions = (("low", "high") if pack_index < 12 else
                           ("high",) if pack_index in HIGH_SPRITE_PACKS else ("low",))
            for conversion in conversions:
                records.extend(_records("sprite", pack_index, address, data,
                                        bit_depth=3, conversion=conversion))
        else:
            records.extend(_records("sprite", pack_index, address, data,
                                    bit_depth=2, conversion="native"))
    for pack_index, address in enumerate(BACKGROUND_PACK_POINTERS):
        data, _ = decode_stream(rom, address)
        if pack_index >= 113:
            # These entries alias sprite packs 106 and 107 (2bpp HUD art).
            if len(data) != 0x800:
                raise ValueError(f"background pack {pack_index}: expected 0x800 bytes, got {len(data):#x}")
            records.extend(_records("background", pack_index, address, data,
                                    bit_depth=2, conversion="native"))
        else:
            if len(data) != PACK_3BPP_LENGTH:
                raise ValueError(f"background pack {pack_index}: expected 0x600 bytes, got {len(data):#x}")
            for conversion in ("low", "high"):
                records.extend(_records("background", pack_index, address, data,
                                        bit_depth=3, conversion=conversion))
    return records
