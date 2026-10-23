"""Resolve base overworld Map8 words to loaded background graphics.

This is the ordinary US-ROM overworld load, not a reconstruction of live VRAM.
The area graphics-theme bytes are in ``catalog_world`` records; Map8 words
come from ``extract_overworld`` and art from ``extract_graphics``. The loader
tables and conversion rule are transcribed from the pinned Zelda3 reference,
``src/load_gfx.c`` (kMainTilesets, kAuxTilesets, InitializeTilesets,
LoadBackgroundGraphics). Zelda3 is MIT licensed; see zelda3-MIT-LICENSE.txt.

The NMI animation upload replaces only the first 32 tiles at VRAM word $3C00;
special-area themes, overlays, and palette effects remain unresolved. A resolved content ID identifies the
canonical unflipped indices only, not a final displayed RGB color.
"""

from __future__ import annotations

from dataclasses import dataclass, replace
from typing import Iterator

from alttp_atlas_graphics import GraphicTile
from alttp_atlas_overworld import OverworldQuadrant, OverworldTile


# Zelda3 load_gfx.c kMainTilesets[0x20] and [0x21].
MAIN_PACKS = {
    0x20: (58, 59, 60, 61, 83, 77, 62, 91),
    0x21: (66, 67, 68, 69, 32, 43, 63, 89),
}

# Zelda3 load_gfx.c kAuxTilesets[32:67]. These are all auxiliary themes used
# by the verified ROM's 80 ordinary light/dark area heads. Other themes fail
# closed instead of being guessed from an adjacent table entry.
AUX_PACKS_32_TO_66 = (
    (0, 0, 0, 0), (0, 87, 76, 0), (0, 86, 79, 0), (0, 83, 77, 0),
    (0, 82, 73, 0), (0, 85, 74, 0), (0, 83, 84, 0), (0, 81, 78, 0),
    (0, 0, 0, 0), (0, 80, 75, 0), (0, 83, 77, 0), (0, 85, 84, 0),
    (0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0), (0, 71, 72, 0),
    (0, 0, 0, 0), (0, 87, 76, 0), (0, 86, 79, 0), (0, 83, 77, 0),
    (0, 82, 73, 0), (0, 85, 74, 0), (0, 83, 84, 0), (0, 81, 78, 0),
    (0, 0, 0, 0), (0, 80, 75, 0), (0, 83, 0, 0), (0, 53, 54, 0),
    (0, 96, 52, 0), (0, 43, 44, 0), (0, 45, 46, 0), (0, 47, 48, 0),
    (0, 55, 56, 0), (0, 51, 52, 0), (0, 49, 50, 0),
)

# Ascending VRAM positions $2000..$3C00 correspond to loader slot arguments
# 7..0. For overworld themes (main >= $20), loader slots 7,4,3,2 use the
# game's high 3-to-4bpp expansion; the others use low expansion.
HIGH_VRAM_SLOTS = frozenset((0, 3, 4, 5))
BASE_VRAM_WORD = 0x2000
WORDS_PER_4BPP_TILE = 16
TILES_PER_PACK = 64
STATIC_BG_SLOTS = 8
ANIMATED_VRAM_SLOT = 7
ANIMATED_TILES_IN_SLOT = 32
SPECIAL_ANIMATION_AREAS = frozenset((0x03, 0x05, 0x07, 0x43, 0x45, 0x47))
ANIMATION_PHASES = 3


@dataclass(frozen=True, slots=True)
class BaseBgTile:
    area_id: int
    quadrant_index: int
    x: int
    y: int
    word: int
    tile_number: int
    palette: int
    priority: bool
    flip_x: bool
    flip_y: bool
    vram_word_address: int
    vram_slot: int | None
    pack_index: int | None
    pack_tile_index: int | None
    conversion: str | None
    content_id: str | None
    status: str
    phase: int | None = None


class BaseOverworldGraphics:
    """Index ROM records once, then resolve tiles for ordinary area heads.

    ``area_records`` is ``catalog_world(rom)['overworld_areas']``. Areas 128+
    are special areas with no ordinary graphics-theme table entry. The caller
    must supply the active area head; its satellite quadrants inherit it.
    """

    def __init__(self, graphics: list[GraphicTile], area_records: list[dict],
                 quadrants: list[OverworldQuadrant]):
        self.areas = {record["area_id"]: record for record in area_records}
        self.quadrants = {quadrant.index: quadrant for quadrant in quadrants}
        self.graphics: dict[tuple[int, str, int], GraphicTile] = {}
        for record in graphics:
            if record.source_kind != "background" or record.pack_index is None:
                continue
            key = (record.pack_index, record.conversion, record.tile_index)
            if key in self.graphics and self.graphics[key].content_id != record.content_id:
                raise ValueError(f"conflicting background graphics record {key}")
            self.graphics[key] = record

    def area_quadrants(self, area_id: int) -> tuple[int, ...]:
        area = self.areas[area_id]
        if area_id >= 128:
            return (area_id,)
        return ((area_id,) if area["is_small"] else
                (area_id, area_id + 1, area_id + 8, area_id + 9))

    def slots_for_area(self, area_id: int) -> tuple[int, ...] | None:
        """Return the eight initial BG pack IDs in ascending VRAM order."""
        area = self.areas[area_id]
        if area_id >= 128 or area["gfx_index"] is None:
            return None
        aux_index = area["gfx_index"]
        if not 32 <= aux_index <= 66:
            return None
        main = MAIN_PACKS[0x21 if area_id & 0x40 else 0x20]
        aux = AUX_PACKS_32_TO_66[aux_index - 32]
        selected_aux = tuple(aux[i] or main[i + 3] for i in range(4))
        return main[:3] + selected_aux + (main[7],)

    def resolve_tile(self, area_id: int, tile: OverworldTile) -> BaseBgTile:
        if area_id not in self.areas:
            raise KeyError(f"unknown overworld area head {area_id}")
        if tile.quadrant_index not in self.area_quadrants(area_id):
            raise ValueError("Map8 tile does not belong to the given area head")
        number = tile.tile_number
        address = BASE_VRAM_WORD + number * WORDS_PER_4BPP_TILE
        slot, local = divmod(number, TILES_PER_PACK)
        pack = conversion = content_id = None
        status = "resolved"
        if number >= STATIC_BG_SLOTS * TILES_PER_PACK:
            status = "tile_number_outside_background_slots"
        else:
            packs = self.slots_for_area(area_id)
            if packs is None:
                status = "special_or_unsupported_area_theme"
            elif slot == ANIMATED_VRAM_SLOT and local < ANIMATED_TILES_IN_SLOT:
                status = "animated_vram_slot"
            else:
                pack = packs[slot]
                conversion = "high" if slot in HIGH_VRAM_SLOTS else "low"
                graphic = self.graphics.get((pack, conversion, local))
                if graphic is None or graphic.bit_depth != 4:
                    status = "background_graphics_record_missing"
                else:
                    content_id = graphic.content_id
        return BaseBgTile(area_id, tile.quadrant_index, tile.x, tile.y,
                          tile.word, number, tile.palette, tile.priority,
                          tile.flip_x, tile.flip_y, address,
                          slot if slot < STATIC_BG_SLOTS else None,
                          pack, local if pack is not None else None,
                          conversion, content_id, status)

    def iter_area_tiles(self, area_id: int) -> Iterator[BaseBgTile]:
        for quadrant_index in self.area_quadrants(area_id):
            quadrant = self.quadrants[quadrant_index]
            for tile in quadrant.iter_tiles():
                yield self.resolve_tile(area_id, tile)

    def resolve_animated_phase(self, base_tile: BaseBgTile, phase: int) -> BaseBgTile:
        """Resolve the NMI replacement at tile numbers 448..479 for one phase.

        Phase 0/1 use the first/second 32 tiles of pack $58 or $5A; phase 2
        uses the first 32 tiles of the following pack. The phase is explicit
        because it can persist across area transitions.
        """
        if base_tile.status != "animated_vram_slot" or not 0 <= phase < ANIMATION_PHASES:
            raise ValueError("expected an animated overworld tile and phase 0..2")
        first_pack = 0x58 if base_tile.area_id in SPECIAL_ANIMATION_AREAS else 0x5A
        pack_index = first_pack + (phase == 2)
        pack_tile_index = base_tile.tile_number % TILES_PER_PACK + (32 if phase == 1 else 0)
        graphic = self.graphics.get((pack_index, "low", pack_tile_index))
        if graphic is None or graphic.bit_depth != 4:
            raise ValueError(f"missing overworld animation graphics {pack_index}:{pack_tile_index}")
        return replace(base_tile, pack_index=pack_index, pack_tile_index=pack_tile_index,
                       conversion="low", content_id=graphic.content_id,
                       status="resolved_animated_phase", phase=phase)

    def audit_all(self) -> dict[str, object]:
        """Count every base Map8 word under its head's initial graphics set."""
        statuses: dict[str, int] = {}
        tile_numbers: dict[str, int] = {}
        max_tile_number = 0
        for area_id in sorted(self.areas):
            for tile in self.iter_area_tiles(area_id):
                statuses[tile.status] = statuses.get(tile.status, 0) + 1
                max_tile_number = max(max_tile_number, tile.tile_number)
                if tile.tile_number >= STATIC_BG_SLOTS * TILES_PER_PACK:
                    key = str(tile.tile_number)
                    tile_numbers[key] = tile_numbers.get(key, 0) + 1
        return {"area_heads": len(self.areas), "base_map8_words": sum(statuses.values()),
                "statuses": statuses, "maximum_tile_number": max_tile_number,
                "out_of_slot_tile_numbers": tile_numbers,
                "unresolved_runtime_layers": ["animated lower half of VRAM slot 7", "overlays and persistent edits",
                                              "palette changes, fades, and color math"]}
