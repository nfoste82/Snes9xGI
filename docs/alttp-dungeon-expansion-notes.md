# ALTTP dungeon object expansion: implementation boundary and next steps

## Current result

`tools/alttp_atlas_dungeon.py` expands the two 64×64 BG floor tilemaps for a
verified USA ROM, then draws the room-stream Type 1/Subtype 1 object `$00`
(the repeated 2×2 ceiling) in layer order. Its `expand_dungeon_room(reader,
room, objects, doors)` arguments are the ROM reader and records from
`alttp_atlas_world.catalog_world`. The return value has `bg1` and `bg2` arrays
of SNES tilemap words, ordered 64 words per row; supported object writes and
all omitted handlers have stable IDs and ROM source addresses. `complete` is
always `false` because the room map is partial.

`catalog_default_layouts(reader)` separately catalogs all eight default
layout streams. It returns `dungeon_default_layouts` and
`dungeon_default_objects` lists ready for the atlas's generic world-record
storage. No default-layout handler is applied to the partial tilemaps yet.

The catalog's numeric `subtype == 0` corresponds to the runtime's Type 1,
Subtype 1 handler. The catalog's `subtype == 1` corresponds to the runtime's
Type 1, Subtype 3 encoding, and `subtype == 2` to runtime Subtype 2. Preserve
both names in a later schema migration to avoid mixing these taxonomies.

The primary references are the [ROM resource compiler](https://github.com/snesrev/zelda3/blob/master/assets/compile_resources.py#L145-L152), [room loader and drawing order](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L2597-L2618), [object decoder and floor routines](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L2682-L2743), [ceiling handler](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L504-L517), [2×2 tile ordering](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L2823-L2827), and [size decoding](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L3461-L3464).

## Default layout catalog provenance

The eight 24-bit pointer entries occupy SNES `$84EF2F` through `$84EF46`.
Each points to a bank-local object stream in bank `$04`, decoded by the same
bounded parser as room objects. The original extractor uses this table and
asserts that no default layout has doors
([`print_default_rooms`](https://github.com/snesrev/zelda3/blob/master/assets/extract_resources.py#L486-L497)).
Each layout row stores its pointer address, stream source address, end address
after the `$FFFF` terminator, and count. Every object row stores the layout
ID, stable ordinal, its own 3-byte ROM source address, the pointer address,
raw bytes, and decoded coordinates/type. For example, layout 0 points from
`$84EF2F` to `$04EF47` and ends at `$04EFAF`.

The verified ROM has 210 default objects: layout counts are
`[34, 21, 30, 30, 22, 30, 30, 13]`. All eight streams terminate inside their
LoROM banks and contain no door records. These objects can be inventoried,
but their rendered writes are still omitted from `expand_dungeon_room`.

## Exact data path

The ROM compiler extracts `kPredefinedTileData` as 6,438 16-bit words from
SNES `$009B52`. `SrcPtr(offset)` is a **byte offset** into that table; all
implemented offsets are even. The room's first byte selects floor patterns:
the high nibble supplies BG2, the low nibble BG1. Each index selects eight
words at byte offset `index * 16`. `RoomDraw_A_Many32x32Blocks` repeats those
words as a 4×2 tile pattern across four 32×32 quadrants, yielding one 64×64
array per BG. A 4×4 or 2×2 interpretation gives a different tilemap.

Room object passes occur in this order:

1. BG2 and BG1 floor fill.
2. One of eight default room layouts, indexed by `layout = header[1] >> 2`.
   The USA ROM's eight 24-bit pointer entries start at SNES `$84EF2F`; the
   [extractor reads them here](https://github.com/snesrev/zelda3/blob/master/assets/extract_resources.py#L486-L497).
3. Room layer 1 objects and doors on BG2, then layer 2 on BG2, then layer 3
   on BG1. The runtime swaps the destination pointer before layers 2 and 3.
4. Push blocks and torches, followed by other save-state and gameplay updates.

The implemented `$00` ceiling uses template offset `$03D8` and four words in
left-top, left-bottom, right-top, right-bottom order. The repeat count is
`(raw_a & 3) * 4 + (raw_b & 3)`, except zero means 32. Each repeat advances
two columns. The implementation bounds-checks every 2×2 footprint before
writing. A future implementation should model the game's linear RAM writes
for deliberately overflowing objects rather than silently clipping them.

## Why this is not yet a full room map

Every other object ID is currently reported as `unsupported.kind=object`;
every door is `unsupported.kind=door`. The default-layout pass and runtime
state pass each have their own unsupported record. Consequently a tile word
at a position may later be overwritten by an omitted pass. It cannot yet be
treated as the final visible tile or used as authoritative height/normal
ownership. Room metadata also controls BG compositing, collision, palette,
graphics blockset, tags, and lighting; these are separate from tilemap words.

The [runtime's dispatch table](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L27-L60) contains `kObjectSubtype1Params`,
`kObjectSubtype2Params`, and `kObjectSubtype3Params`, but a table entry alone
does not define an object's dimensions or its write behavior. Several handlers
write both BGs, test existing tiles, alter collision or door state, or branch
on dungeon save data. Door handlers use `kDoorTypeSrcData` and related tables,
position tables, direction, and opened-state remapping. The
[door dispatch](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L2670-L2679)
and [state remap](https://github.com/snesrev/zelda3/blob/master/src/dungeon.c#L3467-L3495)
must be modeled together.

## Completion path

1. Pass the now-cataloged default layout objects through the ordered draw
   engine before the room's own layer 1, after verifying each handler against
   runtime tilemaps. Until then, retain the default-layout omission marker.
2. Implement one handler family at a time from `LoadType1ObjectSubtype1`,
   `LoadType1ObjectSubtype2`, and `LoadType1ObjectSubtype3`. For every handler,
   record template source spans, touched BGs, output footprints, and whether
   it reads or changes room state. Keep an explicit unsupported record until
   the handler is exercised by a reference fixture.
3. Implement all four door directions and state variants. Model door slot
   order and opened bits, rather than assigning a single static image to each
   door type.
4. Apply initial push-block, torch, overlay, and tag/save-state effects as
   named variants. Store a state vector with each tilemap; a room has multiple
   legitimate appearances.
5. Resolve tilemap word attributes into BG graphics source and palette using
   the room blockset and graphics-loader rules. Keep this step separate from
   object expansion so provenance survives palette and VRAM changes.
6. Generate fixture captures from an unmodified ROM in Snes9x or the
   [zelda3](https://github.com/snesrev/zelda3) implementation. Compare both
   4,096-word BG arrays after room load, before rendering, under explicit save
   states. Attribute first differences to the specific handler and source
   object. Require zero mismatches for representative fixtures before moving
   a handler out of `unsupported`.

## Acceptance fixtures and measured scope

Using the USA ROM with SHA-256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`:

| Fixture | Expected result |
| --- | --- |
| Template index 0 | `$14EE,$14EF,$14EE,$14EF` then `$14FE,$14FF,$14FE,$14FF` |
| Room 0 BG1 first eight words | `$10EC,$10ED,$10EC,$10ED,$10EC,$10ED,$10EC,$10ED` |
| Room 1, layer 1, object ordinal 19 | Object `$00`, source `$0A911D`, at `(0,3)`, zero size code means 32 repeats; first four writes target tile indices 192, 256, 193, 257 with word `$3C15` |
| All room streams | 320 room expansions, two arrays of 4,096 words each; each result JSON encodes and has `complete=false` |
| Omission accounting | 130 supported `$00` room objects, 15,121 omitted room objects, 714 omitted doors, plus one default-layout and runtime-state marker per room |
| Default-layout streams | Eight rows; 210 objects total; first pointer `$84EF2F` to `$04EF47`, first stream end `$04EFAF`; no doors |

These fixtures check extraction and the implemented subset; they do **not**
validate a finished room image. The decisive future gate is comparison with
runtime BG tilemaps from matching ROM and save state.
