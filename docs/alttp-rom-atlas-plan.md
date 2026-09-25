# ALTTP ROM atlas and profile generator

Status: hash-keyed first-pass candidate profile working, 2026-09-25. This is the durable working document for
the ROM-backed asset inventory and lighting-metadata generator. Update the
evidence ledger and milestone status as implementation proceeds.

## 2026-09-25 in-game correction pass

The first candidate exposed four distinct failures: generated sprite parts
restarted their height ramp per 8×8 tile; Link side poses could inherit a
poor partial normal match from the authored back pose; the HUD group listed
only captured glyphs; and one dungeon room used the same art on raised and
lower floor planes. The new local candidate is
`build/remaster-geometry/alttp-profile-v4-candidate.toml` (schema 13, 17,354
assets). The source `alttp-profile.toml` remains unchanged by the generator.

- The HUD group now contains all 216 unique decoded 2bpp tiles from background
  packs 113 and 114, rather than the 25 seen in the earlier capture. Its rule
  still requires background source index 2 (BG3), so sprite reuse does not
  acquire the HUD material.
- The Link detail transfer now requires a 0.94 symmetric artwork match. It
  transfers 7 tiles instead of 71; ambiguous side tiles keep explicit
  silhouette normals, avoiding mismatched patches copied from a back pose.
  At runtime, exact generated Link silhouettes in the Link VRAM range receive
  a modest 30-degree normal turn for the game's left/right facing state at
  `$7E:002F`. Detailed authored normals are left alone. This is deliberately
  conservative; pose-specific hand or model authored normals still require
  animation identity.
- Live frame packets identify generated sprite ramps by their exact pixel
  pattern and align visible OAM parts to the bottom of the assembled figure.
  This adds an instance height offset and leaves authored height arrays alone.
  It groups adjacent OAM slots only when their visible pieces overlap in X,
  touch or nearly touch in Y, and share sprite priority. Grouping can still be
  ambiguous when actors overlap; a semantic actor ID remains the long-term
  solution. The alignment pass averaged about 0.057 ms over 100 runs on the
  existing 2,021-instance recorded frame on this machine; that is a narrow
  CPU check, not a full game frame-rate measurement.
- The supported US ROM hash gates the indoor adapter. It raises OAM sprites
  at priority 2 by `lighting_space.upper_floor_height` (48 in the candidate),
  including in collision mode 0. Priority-1 sprites remain at their base
  height. The value is added per rendered instance, so a tile hash can be used
  at either elevation. The frame schema is 19 and stores each instance's OAM
  priority, applied height offset, and generated Link normal turn.

The floor state is grounded in Zelda3's `link_is_on_lower_level` at `$7E:00EE`,
`sprite_floor` at `$7E:0F20`, the collision mode at `$7E:046C`, and the
priority mappings in `player_oam.c` and `sprite.c`. The Snes9x renderer does
not include OAM priority in its tile word, so the priority is passed from
`PPU.OBJ[S].Priority` at draw time. The game also has collision attribute
planes offset by `$1000`; they are collision maps, not a ready-made physical
height map. Death Mountain's tall ladders remain outside this two-plane rule.

### Captured mixed-height room, 2026-09-25

The supplied pre-change frame is schema 17 and shows the requested raised
checkered floor, lower red floor, stairs, and Link. It has 55,238 main-screen
pixels from BG2 and 1,812 from BG3; **both** dungeon floors and their room
fixtures are drawn through BG2. The nearby auto-save is room `0x61`, with
indoor flag 1, collision mode 0, and Link's lower-level flag 0. Its Link world
position and BG2 scroll place Link at about `(88, 108)` in the captured frame,
matching the image. Thus collision modes 1–3 and BG2 membership cannot be
used as a height classifier. The earlier whole-BG2 rule would not activate in
this room; if made unconditional, it would raise the lower floor too. It has
been removed. OAM priority remains usable for Link and ordinary sprites.

The auto-save contains the BG1 and BG2 room tilemaps and their collision
attribute planes. At a checkered-floor sample `(80, 100)`, BG2 tile `0x0cec`
and BG1 tile `0x10ec` share artwork; at lower-floor sample `(175, 150)`, BG2
tile `0x1cc6` and BG1 tile `0x10ed` differ. The collision attributes at both
samples are zero on both planes. Tilemap comparison can seed a spatial floor
mask, but fixtures and walls need room-object context and a validation pass.
The next runtime step is a per-room, per-position floor mask derived from room
object layout or validated tilemap regions; background height must remain at
its authored base value until that mask is available.

The same capture exposes four distinct tread tiles and four rail tiles on the
staircase. The profile now groups them as `stair_treads` and `stair_rails`.
The candidate assigns tread heights near 39, 27, 15, and 3 from top to bottom,
with +Z normals on the broad top surfaces and small deterministic tilts on
darker stone flecks. The rail heights descend continuously from 48 toward 1;
the upper cap faces +Z and the remaining smooth rail faces halfway between +Y
and +Z. The tread rule uses the existing rough dungeon floor material; the
rail rule uses a smoother stair-rail material. These eight hashes occur only
in background graphics pack 1 and
are absent from the 102 previously authored assets. Replay against the
provided capture resolves all eight to the intended material groups and
samples the expected heights and normals on both rails and all four treads.
`build/remaster-geometry/stair-geometry-preview.png` shows the captured art,
height, and normals side by side for this 32×32 stair crop.
The geometry is global by tile hash for this first pass. The upper landing,
door, and fence still require a spatial floor mask so their heights connect
to the staircase consistently.
The app's selected generated profile path,
`build/remaster-geometry/alttp-profile-candidate.toml`, was updated to this
v4 candidate after confirming all 102 source-authored asset fields matched the
prior file. The earlier selected candidate is retained as
`build/remaster-geometry/alttp-profile-candidate-before-stairs.toml`.

## Goal

Build a standalone program that takes a user-supplied *A Link to the Past* ROM
and a remaster profile, identifies every room, overworld area, background tile,
sprite object, animation frame, and constituent decoded 8×8 tile it can prove is
reachable, then proposes per-pixel height, normal, occlusion, and emission data.
It must write a parser-valid profile and an audit report showing coverage,
provenance, uncertainty, and any asset it could not classify. It must never
silently replace detailed user-authored metadata.

The complete asset inventory and the quality of inferred geometry are separate
acceptance gates. A ROM can reveal pixels and their relationships, but it does
not uniquely specify 3D shape or lighting intent. The tool must expose those
choices, support authored overrides, and report uncertainty.

## Existing foundation

- `remaster/profile.h` defines the runtime tile-content ID and strict profile
  parser/serializer. The profile is bound to a ROM SHA-256.
- `remaster/frame.h` records decoded 8×8 palette-index artwork, visible pixels,
  and capture-local animation tracks. Its runtime frame is not an exhaustive ROM
  index.
- `tools/remaster_geometry.cpp` suggests Link geometry from captured artwork and
  detailed authored tiles. It already preserves detailed layers and emits a
  separate profile, but it has no ROM layout decoder or complete object model.
- `alttp-profile.toml` contains many hand-authored tiles, including one detailed
  Link pose. This is training/reference data and must remain an override.

The exact local ROM hash is also accepted by the [zelda3 US resource extractor](https://github.com/snesrev/zelda3/blob/master/assets/util.py).
Its extractor and reimplementation provide independently inspectable game-data
paths. The source is MIT licensed; cite it when deriving table locations and
retain its notice if copying a substantial part of code or tables. The atlas
must still validate against *this emulator's* decoded tile bytes and runtime
tile IDs. The reviewed local checkout is commit
`fbbb3f967a51fafe642e6140d0753979e73b4090`.

## Non-negotiable identity rules

1. Use the project's versioned hash over canonical decoded palette indices,
   source kind, and bit depth. Never confuse ROM address, VRAM tile number,
   palette, or screen position with the content ID.
2. Preserve provenance alongside the ID: ROM revision and offset, decompression
   path, graphics pack, logical object/frame/part, map/room/placement, palette,
   flips, and runtime validation evidence.
3. A repeated 8×8 pattern may mean different surfaces in different contexts.
   The first pass intentionally uses one asset per content hash, as authorized
   for an in-game trial. Record ambiguous shared uses; add context variants
   where review shows the hash-level choice is inadequate.
4. Keep the ROM local. The generated database/report may contain extracted art;
   treat it as local output and do not add it to the repository.

## Program contract

```text
python3 tools/alttp_atlas.py --rom USER_ROM --profile PROFILE
    --database LOCAL_DATABASE --report LOCAL_REPORT
    [--output-profile CANDIDATE_PROFILE] [--inventory CAPTURED_INVENTORY]
```

The tool checks ROM revision/hash, normalizes a headered/headerless ROM if
supported, inventories content deterministically, validates the profile's ROM
binding, infers metadata under explicit rules, preserves authored layers,
validates with the project's parser, and writes outputs atomically. It does not
launch gameplay or depend on visiting rooms manually.

The database needs distinct records for: ROM revision; graphics pack and decoded
tile; palette; map and room; placed object; logical sprite identity; animation
state and frame; 8×8 tile part; tile-content ID; usage context; proposed metadata
layer; provenance and confidence. Use deterministic stable keys for logical
objects and frames, while retaining the runtime content hash as a join key.

### Source universes to cover

The inventory must keep these families separate even when they share artwork:

| Family | Static data path | Runtime composition path |
| --- | --- | --- |
| Dungeon backgrounds | 320 room pointers, headers, three object layers and doors | Room object handlers expand records into tilemaps |
| Overworld backgrounds | Area heads plus 160 high/low Map32 quadrant streams, Map32/16/8 tables | Area/theme/state chooses palettes, graphics, and overlays |
| Placed sprites | Dungeon and staged overworld sprite records | Sprite type routines choose poses and OAM parts |
| Link | Raw 4bpp graphics at `$10:8000` | Player handler, facing, gear, sword/shield, shadow, DMA selection |
| Ancilla, overlords, tagalongs, effects | Family-specific data and routines | State, events, and spawned visuals |
| UI and special PPU paths | Fonts, HUD graphics, other graphics loads | 2bpp/Mode 7/hires and color math as applicable |

The [zelda3 graphics loader](https://github.com/snesrev/zelda3/blob/master/src/load_gfx.c)
selects four sprite packs by graphics index and expands many ROM 3bpp packs to
runtime 4bpp. The high-bit conversion sets bit 3 only for nonzero source pixels;
hash the actual runtime indices, not the 3bpp bytes or a palette-colored sheet.
The [graphics stream pointers](https://github.com/snesrev/zelda3/blob/master/assets/tables.py)
and [resource extraction](https://github.com/snesrev/zelda3/blob/master/assets/compile_resources.py)
give a starting pack inventory: 108 sprite entries, with the first 12 raw, plus
compressed background packs. The atlas must record VRAM conversion and graphics
context because the same pack can be loaded into different slots.

The [dungeon extractor](https://github.com/snesrev/zelda3/blob/master/assets/extract_resources.py)
iterates 320 room records and parses headers, three object layers, doors, and
sprite placements. The [overworld extractor](https://github.com/snesrev/zelda3/blob/master/assets/extract_resources.py)
uses area-head filtering and staged sprite lists. The 160 compressed high and
160 compressed low streams are **map quadrants**, not 160 distinct logical areas.
The [runtime overworld expansion](https://github.com/snesrev/zelda3/blob/master/src/overworld.c)
goes Map32 → four Map16 values → four Map8 tile words. Map objects and tile
placements must be kept distinct: a room record is not itself a rendered tile.

For a base overworld area, the [graphics loader](https://github.com/snesrev/zelda3/blob/master/src/load_gfx.c)
selects a light/dark main theme and an area auxiliary theme, then loads eight
64-tile BG packs into consecutive VRAM slots starting at word `$2000`.
For a Map8 word with tile number 0–511, the candidate slot is
`tile_number // 64` and the slot-local tile is `tile_number % 64`.
Each slot uses either the loader's low or high 3bpp-to-4bpp conversion.
This is a route from map placement to a decoded content ID, but it needs a
full tile-number audit and validation against Snes9x BG tile bytes. Animated
uploads can replace the final slot; transitions, overlays, and save-state
edits change map or graphics state. Palette selection is a separate path via
the area's palette index and overworld palette tables.

The [sprite dispatch table](https://github.com/snesrev/zelda3/blob/master/src/sprite_main.c)
contains 243 active routine entries, and multipart sprite drawing selects OAM
parts from runtime state. [Link's OAM code](https://github.com/snesrev/zelda3/blob/master/src/player_oam.c)
uses a separate pose and equipment path. [Ancilla](https://github.com/snesrev/zelda3/blob/master/src/ancilla.c)
and [overlord](https://github.com/snesrev/zelda3/blob/master/src/overlord.c)
are additional visual families. A graphic pack or sprite type is therefore
**not** a complete logical animation. The atlas needs an explicit state/frame
coverage ledger, not an inferred 100% claim from decoded packs.

### ROM, RAM, and PPU identity chain

The game does not draw directly from a single static ROM sprite sheet. The
extractor must model or observe this chain:

```text
ROM graphics stream + room/area/object tables
    -> decompressed graphics pack + logical placement
    -> game WRAM state (module, area/room, actor type/state, facing, gear, timers)
    -> DMA/VRAM tile bytes + CGRAM palette + OAM sprite parts / BG tilemaps
    -> Snes9x decoded canonical 8x8 indices and draw context
    -> remaster content ID, logical usage ID, per-pixel profile metadata
```

ROM offsets establish source provenance, not the full displayed object. WRAM
state can select a different pack, pose, palette, or script outcome; VRAM tile
numbers are temporary slots; OAM entries are rendered parts, not persistent
NPC IDs. A future ALTTP adapter should expose a small versioned read-only
snapshot of relevant WRAM/PPU state to the atlas validator, plus a logical
placement/actor ID when the game code loads or spawns it. Instrument the DMA,
VRAM, CGRAM, OAM, and BG tilemap boundaries to prove that the offline atlas
reproduces what the emulator actually draws. Do not infer all of these joins
from a screenshot or VRAM number alone.

### Animation enumeration without manual room recording

1. Enumerate the finite source families and dispatch entries from game code:
   player, normal sprites, ancilla, overlords, tagalongs, garnish/effects.
   Record each family's draw routine and all graphics/palette selectors.
2. For table-driven OAM draw routines, extract the frame-part tables and
   predicates. For stateful routines, build a bounded drawing harness or
   instrument the game/reimplementation to step controlled state vectors:
   actor type/subtype, action state, direction, animation counter, gear,
   world/room graphics context, event flags, and relevant random values.
3. Trace chosen OAM parts and VRAM source for each distinct result, deduplicate
   identical frames, and retain the state predicate that produced each frame.
   Use static read-set inspection and runtime branch coverage to choose test
   vectors; brute-forcing all WRAM combinations is neither feasible nor a
   completeness proof.
4. Compare representative states against Snes9x captures and, where useful,
   the Zelda3 port's documented side-by-side original-machine-code checking.
   Capture comparison validates behavior; it is not the primary means of
   discovering all content.
5. Mark each routine/state family `verified`, `modeled_unverified`,
   `dynamic_unresolved`, or `unreachable_proven`. An unknown branch or DMA load
   remains an explicit gap in the report. This is the route to high coverage
   without asking the user to manually visit every room and attack pose.

#### Concrete Link frame-enumeration harness

The [Link OAM routine](https://github.com/snesrev/zelda3/blob/fbbb3f967a51fafe642e6140d0753979e73b4090/src/player_oam.c#L762-L1126)
selects pose category `yt`, animation step `rt`, and facing `dir`, then computes
`r2 = kPlayerOamOtherOffs[dir * 40 + yt] + rt`. Its tables choose one of 12
body layouts and a DMA artwork index. A source-table scan gives 511 candidate
`r2` entries and 268 distinct referenced body-art indices; neither count
proves reachability. Auxiliary body parts, sword, shield, shadow, visibility,
and equipment add conditional OAM entries.

Build a local harness around the pinned Zelda3 implementation and its assets
extracted from the user's ROM. Restore a known gameplay checkpoint for each
coherent test vector, set direction/action/frame/equipment/context state,
invoke `LinkOam_Main`, then perform sprite preparation and NMI uploads.
Record `(yt, rt, dir, r2, art index, layout)` at the branch point, and capture
Link-owned OAM positions, sizes, flips, palettes, DMA source, resulting VRAM
tile bytes, and CGRAM. Deduplicate by complete assembled frame and retain every
state witness. Drive cases from the actual branches for walking, running,
stairs, swimming, items, sword/spin, bunny, falling, and other actions, then
use scripted gameplay to verify reachability. Compare representative results
with Snes9x captures. [Sprite preparation](https://github.com/snesrev/zelda3/blob/fbbb3f967a51fafe642e6140d0753979e73b4090/src/misc.c#L328-L415)
and [NMI uploads](https://github.com/snesrev/zelda3/blob/fbbb3f967a51fafe642e6140d0753979e73b4090/src/nmi.c#L172-L217)
are necessary to turn a pose-table index into final tile pixels. Ancilla such
as arrows, rods, beams, and spells need a separate enumeration pass.

### Database and coverage semantics

Use SQLite locally with separate `decoded_tile`, `graphics_source`, `map_area`,
`room`, `map_object`, `sprite_placement`, `entity_family`, `logical_entity`,
`animation_state`, `frame`, `frame_part`, `tile_usage`, `metadata_proposal`,
`authored_override`, `conflict`, and `finding` tables. A `frame_part` carries
relative XY, 8/16 size, tile number, flips, priority, palette, and chosen
graphics context. `tile_usage` joins each part or background placement to the
canonical tile content ID. A graphics tile lacking a proven usage remains in
the database as **decoded but unassigned**, never mislabeled as an object.

Each stage reports its own numerator and denominator. Examples: graphics
streams decoded / pointer entries expected; dungeon rooms parsed / 320; map
quadrants expanded / 160 per plane; logical sprite routines modeled / known
routine entries; rendered frame variants verified / discovered frame variants;
tile usages with resolved metadata / all usages. Keep counts for missing,
ambiguous, dynamic, and intentionally unsupported content. Do not combine these
into one misleading completion percentage.

### Geometry proposal pipeline

1. Build a semantic object or surface from *all* its tile parts before
   estimating geometry. Retain screen-space offsets, palette, flips, and the
   ground-contact point. Estimate an object-scale height field once, then cut it
   back into canonical 8×8 layers. This prevents tile seams and pose-to-pose
   height drift.
2. Use authored examples as shape priors and validate 2.5D geometry against
   silhouettes, palette regions, and animation correspondence. Distinguish
   body, head, equipment, shadows, and effects. Record matched reference and
   confidence for every generated layer. A tile used on two wall orientations
   or in two unrelated objects may need separate semantic variants.
3. Derive normals from a fitted surface plus semantic shape regions. Curved
   heads/body sides can tilt into X/Y while surfaces facing the viewer retain
   positive Z; preserve authored vectors and explicit opposite-facing behavior.
   Visible RGB alone does not uniquely determine a normal.
4. Estimate occlusion from coverage, material, and object role; transparent
   palette index alone is insufficient for shadows/effects. Emit light only for
   explicit semantic sources or strong authored references. A bright paint
   color does not prove emission.
5. Write layer-level proposals into the local atlas with provenance and
   uncertainty. Generate a candidate profile only for compatible usages.
   Preserve every authored layer unless an explicit, reviewable override is
   selected.

### Required profile/runtime change for conflicting uses

`profile.assets` is keyed only by content hash. Runtime geometry lookup in
`macosx/mac-render.mm` also uses only that hash. Existing material rules can
match source/source-index/palette, but no room, logical object, animation, or
placement. One hash needing two height/normal/emission maps **cannot be fully
represented today**. OAM index and screen cell are frame-local and are not
stable semantic keys.

Proposed migration (version numbers are provisional until implementation):

1. Atlas v0 records all uses and conflicts; it writes current-schema additions
   only for IDs with one compatible geometry. Conflicts remain explicit.
2. Add profile `[[asset_variants]]` keyed by `(tile_hash, semantic_usage_id)`.
   Keep existing `[[assets]]` as fallback. Resolve each layer in order:
   exact semantic variant, then hash-only asset, then missing-data sentinel.
   Reject duplicate variant keys and report unresolved semantic IDs.
3. Add the stable semantic usage ID to frame packets and the ALTTP runtime
   adapter. For BG, derive it from ROM room/area plus logical placement and
   state; for characters, from actor family/type/state/frame/part rather than
   OAM slot. Verify IDs across camera motion and OAM reordering before using
   them to select metadata.
4. Teach profile saving, captures, replay, and Metal lighting to resolve the
   same variant. Preserve older profile/frame schemas and authored hash-only
   assets unchanged.

Before declaring this complete, create a same-hash/two-usage fixture with
different geometry and emission, confirm both render correctly under flips,
confirm unknown usage falls back, and verify deterministic profile/frame
round trips and no accidental authored-layer replacement.

## Work breakdown

| Stage | Deliverable | Acceptance evidence |
| --- | --- | --- |
| 0. Evidence | ROM revision, references, current runtime hash and profile semantics documented | Source citations and checked local ROM fingerprints |
| 1. Decoder | ROM mapper/header handling, compression and graphics decode, palette decode | Known tiles match live captured 8×8 indices and IDs |
| 2. World maps | Overworld areas, dungeon room headers/layouts, object and tilemap expansion | Counts, coordinate maps, rendered previews, spot checks against gameplay |
| 3. Sprites | Sprite definitions, graphics packs, multipart layouts, animation state graph, frame assembly | Link and representative NPC/enemy frame atlases agree with game captures |
| 4. Atlas DB | Deduplicated artwork plus provenance and every usage context | Deterministic rebuild and explicit unresolved/missing records |
| 5. Metadata | Surface classification, object-scale height models, normals, occlusion, emission | Representative lighting previews and annotated confidence |
| 6. Profile writer | Context-aware mapping, conflict handling, authored override precedence | Runtime parser round trip and no loss of authored data |
| 7. Audit | Automated coverage and regression checks across ROM revisions and captures | No silent gaps; counts and exceptions reviewed |

The working first pass writes a candidate profile keyed by tile hash. Stage 6
still describes the richer context-aware writer for exceptions found in game.

## Immediate proof of concept

Extract one known graphics stream directly from the ROM, decode it to canonical
8×8 palette indices, and reproduce one or more tile-content hashes observed in
the existing captures. This proves the identity bridge needed before attempting
large-scale world or sprite metadata generation. A separate small ROM-backed
executable should own this path; the capture-driven geometry tool can later be
reused as a metadata suggestion module.

**Verified local bridge:** The user-supplied ZIP contains a headerless 1 MiB
USA ROM with SHA-256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Its internal title is `THE LEGEND OF ZELDA`, mapper byte is `0x20`, and ROM-size
byte is `0x0a`. A local exhaustive scan decoded every ROM offset as an SNES
4bpp planar tile and compared the resulting v1 hashes with profile assets.
It found 34 matches (counting repeated content), concentrated in the region
starting at file offset `0x80000`. Examples:

| ROM file offset | Runtime tile-content ID | Existing use |
| --- | --- | --- |
| `0x80000` | `v1:4bpp:508933e0ccff1752` | Link sprite |
| `0x80080` | `v1:4bpp:3a780468c669a912` | detailed Link pose |
| `0x80200` | `v1:4bpp:20a0ddd4c93719a7` | older Link variant |
| `0x80840` | `v1:4bpp:24d533619cc5ad09` | detailed Link pose |

This proves that at least one Link graphics block is stored uncompressed as
canonical 4bpp tiles. It does **not** prove that every tile in that region is
Link, that all Link frames are present there, or that unrelated graphics are
uncompressed. The standalone extractor should first encode this confirmed
bridge and then add address/pack discovery from game code.

## Working ROM inventory program (stage 1 and record-level stage 2)

The current `tools/alttp_atlas.py` accepts a user-supplied USA ROM (`.sfc`,
`.smc`, or one-ROM ZIP), `alttp-profile.toml`, and local SQLite/report paths.
It strips a 512-byte copier header when present, checks the normalized ROM
SHA-256 against both the profile and the one supported USA revision, decodes
graphics via `tools/alttp_atlas_graphics.py`, parses world records via
`tools/alttp_atlas_world.py`, expands all base Map32 quadrants via
`tools/alttp_atlas_overworld.py`, partially expands dungeon floor grids via
`tools/alttp_atlas_dungeon.py`, resolves ordinary overworld base tile content
via `tools/alttp_atlas_overworld_gfx.py`, and compares optional Snes9x inventory
art byte for byte. With `--output-profile`, `tools/alttp_atlas_profile.py`
fills all four metadata layers for every decoded content hash, preserves each
authored field, and writes a separate candidate for an in-game trial.

```sh
python3 tools/alttp_atlas.py \
  --rom "$HOME/Downloads/Zelda_ALTTP.zip" \
  --profile alttp-profile.toml \
  --database build/remaster-geometry/alttp-atlas.sqlite \
  --report build/remaster-geometry/alttp-atlas-candidate-report.json \
  --output-profile build/remaster-geometry/alttp-profile-candidate.toml \
  --inventory "$HOME/Library/Application Support/Snes9x/Remaster/tile-inventory.json"
```

For the first in-game trial, open the matching ROM in Snes9x, choose
**View > Load Remaster Profile...**, and select
`build/remaster-geometry/alttp-profile-candidate.toml`. The candidate is a
separate file so the authored `alttp-profile.toml` remains available for
comparison. The candidate is about 39 MB; initial parsing can take several
seconds. The companion candidate report records the generated methods and
ambiguous hashes.

These exact locally measured results are **data inventory counts**, not full
scene/animation coverage:

| Result | Count / check |
| --- | ---: |
| Decoded graphics records | 23,616 |
| Unique decoded tile-content IDs | 17,354 |
| Content IDs with multiple ROM graphics sources | 1,662 |
| Content IDs shared across graphics families | 237 |
| Existing profile asset IDs recovered | 102 / 102 |
| Captured inventory tiles matching decoded bytes and ID | 85 / 85 |
| Dungeon room records | 320 |
| Dungeon object records | 15,251 |
| Dungeon door records | 714 |
| Dungeon default layout streams / object records | 8 / 210 |
| Dungeon rooms with partial floor/ceiling tilemaps | 320 / 320 |
| Dungeon objects expanded / explicitly omitted | 130 / 15,121 |
| Dungeon doors explicitly omitted | 714 |
| Dungeon sprite/key/overlord records | 1,489 |
| Overworld area heads | 112 |
| Overworld staged sprite lists / placements | 176 / 890 |
| Distinct placed dungeon sprite / overworld sprite / overlord type IDs | 124 / 96 / 20 |
| Overworld Map32 quadrants expanded | 160 / 160 |
| Overworld Map8 tile words recovered | 655,360 |
| Base overworld positions resolved to decoded tile IDs | 504,383 (2,080 distinct IDs) |
| Base overworld positions with three decoded animation variants | 19,905 (59,715 phase-specific usages) |
| Ordinary base overworld positions with artwork across phases | 524,288 / 524,288 |
| Base overworld positions in special areas without static theme | 131,072 |
| Candidate profile assets with all four pixel layers | 17,354 / 17,354 |
| Authored assets preserved / new assets | 102 / 17,252 |
| Link tiles receiving strong authored-reference transfer | 71 |

The base overworld quadrants each contain 256 Map32 cells, 1,024 Map16 cells,
and 4,096 Map8 tile words. The atlas stores all three grids as little-endian
16-bit arrays with source stream pointers and compressed/decompressed lengths.
Quadrant 71's high plane decompresses to 258 bytes; the game uses its first
256 bytes for this grid, and the atlas records the exception. A map word alone
does not identify a rendered pixel; graphics selection and runtime state also
matter.
For the 80 ordinary overworld area heads, the initial graphics-theme slot
mapping now resolves 504,383 tile positions to canonical artwork IDs. The
SQLite `overworld_base_tile_usage` table retains area, quadrant, XY, raw tile
word, pack/slot, conversion, content ID, and resolution status at every
position, including unresolved positions. All Map8 tile numbers are within
the expected 0–511 BG range. The pack selections matched the pinned loader
tables for all 80 ordinary heads; sampled tile assignments and an independent
ROM planar decode matched. Special areas 128–159 remain unresolved as one
static theme. The lower half of the animated VRAM slot has all three explicit
phase variants in `overworld_animated_tile_usage`; the active phase is runtime
state, and a resolved ID still does not determine final palette
color or state-dependent overlays. This is a *base* map assignment, not an
exhaustive rendering of every state.

The graphics loader initially fills the final 64-tile BG slot from a static
pack. [NMI animation uploads](https://github.com/snesrev/zelda3/blob/master/src/nmi.c)
replace only its first 32 tiles, so the upper 32 are resolved from the static
pack. This refinement reduced the animated-position gap from 52,695 to 19,905.
The [overworld animation loader](https://github.com/snesrev/zelda3/blob/fbbb3f967a51fafe642e6140d0753979e73b4090/src/load_gfx.c#L518-L524)
then supplies three 32-tile low-converted frames from pack `$58`/`$59` or
`$5A`/`$5B`, selected by screen. The atlas records each of these 59,715
position-plus-phase assignments separately. It cannot select the currently
displayed phase without observing the game state.

Special quadrant IDs 128–159 mix entered areas and overlay source maps.
The [special-entry path](https://github.com/snesrev/zelda3/blob/fbbb3f967a51fafe642e6140d0753979e73b4090/src/overworld.c#L1867-L1908)
overrides the auxiliary theme to 47, while direct dungeon exits can load a
different theme. Overlay maps borrow the host scene's graphics context.
Therefore these quadrants need distinct entry/overlay usage contexts and a
live VRAM comparison before content IDs are assigned; filling one fixed
special-area theme would mislabel tiles.
Those 131,072 cataloged tile words are **not** a denominator for reachable
special-scene coverage: some indices are placeholder copies, and overlays can
be rendered under several host themes. Reachability and context must be
classified before a special-area coverage fraction is meaningful.
The placed sprite type counts describe ROM placement records only. They do not
include every type a script can spawn and do not enumerate any animation frames.
Art reuse counts are identity reuse, not proof that those usages need different
metadata. The semantic usage stage must detect actual geometry conflicts.

The current dungeon expansion fills each room's two 64×64 floor grids and
applies the Type 1/Subtype 1 `$00` ceiling handler. Each room is marked
`complete=false`; the SQLite `dungeon_expansion_gap` table records all other
object handlers, doors, default layouts, and stateful updates that remain to
be modeled. All eight default layout streams and their 210 object records are
cataloged with source addresses, but their handlers are not yet applied.
The partial grids are diagnostic intermediate data, **not final
scene tilemaps**. See [dungeon expansion notes](alttp-dungeon-expansion-notes.md)
for the exact handler, bounds, omissions, and acceptance fixtures.

SQLite integrity and deterministic ID checks pass. The same extraction passes
with a 512-byte headered copy of the ROM. The report's `coverage_gates`
explicitly marks incomplete dungeon tilemaps, missing logical animation frames,
and uncertainty in the first-pass metadata. The candidate passes the game's
profile parser and a serialization round trip; the original profile is not
overwritten.

The first-pass generator uses a neutral background estimate (height zero,
camera-facing normal, no occlusion or emission). Link and sprite tiles use an
8×8 silhouette, a descending height ramp matching the typical authored scale,
side-tilted silhouette normals, and occlusion from nonzero palette indices.
Seven Link tiles with strong pixel matches inherit detailed authored
height/normal values. Emission is zero unless an authored asset already emits;
there is no reliable way to identify a new light source from palette indices
alone. All 237 hashes shared across graphics families are listed for review,
with neutral fallback for un-authored ambiguous hashes. These are trial
estimates, not claims of recovered 3D truth or complete scene coverage.

### Large-profile runtime path

The candidate is about 39 MB and contains 17,354 asset records. A live frame
formerly copied every asset record and the lighting renderer searched that
full list once per tile instance. This made CPU field construction grow with
the complete ROM atlas even in a small room. The frame now carries metadata
only for hashes present in its observed artwork or tile instances. The lighting
renderer indexes that frame-local list once, then looks up instances by hash.
Captured frames also stay below the format's 16,384 metadata-record limit.

Scene Controls now send only scalar scene settings to the emulation thread and
keep small undo snapshots; changing a setting no longer serializes, reparses,
or copies the entire asset atlas. Profile load still parses the whole TOML,
then builds a canonical saved snapshot without reparsing the already validated
profile. Byte-array parsing now scans each list once rather than copying its
remaining text for every pixel. The full serializer and file writer retain
validation before saving.
The on-disk candidate format has not changed, so existing profiles remain
compatible. A compact or binary disk representation remains an option if
loading this candidate is still too slow in the actual app.

Local checks on the candidate: portable tests and the arm64 macOS build pass;
parsing took about 0.41 seconds and making the canonical snapshot about 0.66
seconds in an optimized command-line benchmark. In a debug command-line build,
those stages took about 2.58 and 0.85 seconds. A synthetic 100-tile scene
copied its metadata from the 17,354-asset profile in about 0.05 ms per frame.
These measurements do not establish the new in-game Scene Fields time; measure
that with the same room and metrics panel after rebuilding the app.

### Next gates toward object-aware coverage

1. Assign scene-state context to the 19,905 positions with three known
   animation variants and resolve the 131,072 special quadrant positions for
   each valid entry or overlay context. Then add overlays,
   persistent map changes, palette variants, and runtime BG byte comparison.
   The stored base positions are a starting point; these state variants add
   further usages rather than replacing the base rows.
2. Expand default dungeon layouts, the 15,121 remaining room objects, 714
   doors, and room-state changes. A handler is complete only after comparing
   the resulting BG arrays with matching runtime states; floors alone do not
   establish final visible geometry.
3. Build the Link OAM harness above, then enumerate normal sprite, overlord,
   ancilla, and effect draw routines with state witnesses and VRAM/OAM proof.
   Convert their 8×8 parts to stable semantic usage IDs.
4. Review the first-pass profile in representative scenes and compare generated
   values with the authored examples. Add context-specific profile variants
   and an ALTTP runtime usage adapter for actual conflicts, then improve
   height/normal/occlusion/emission proposals from composed objects rather
   than isolated 8×8 artwork.

## Open questions and risks

- How should ROM variants, patches, and randomizers be matched to source
  layouts? The current verified decoder supports only the exact USA hash.
- How should each object's state choose a graphics pack, palette, and frame?
  Static pack locations alone do not reveal state-dependent runtime loading.
- Which map/object definitions are data-driven, and which are assembled by game
  code or scripted state? Exhaustiveness needs a declared definition of
  reachable content and an exceptions list.
- How can the profile distinguish one content hash used as two different
  surfaces? A logical-object and placement-aware rule key may be needed.
- How should object-scale height stay consistent across tiles and animation
  frames, including body parts that move in screen Y but not world Z?
- Which colors are genuinely emissive, and which merely appear bright in the
  baked artwork? Emission should require explicit semantic rules or review.

## Evidence ledger

| Claim | Status | Evidence |
| --- | --- | --- |
| Runtime IDs hash decoded unflipped 8×8 palette indices | Verified in project code/docs | `remaster/remaster.h`, `AIDocs/03-phase-2-tile-identity.md` |
| Captures hold only encountered content | Verified in project code/docs | `remaster/frame.h`, `AIDocs/07-phase-4-authoring-workbench.md` |
| Local USA ROM is 1 MiB, headerless, and bound to the profile SHA-256 | Verified locally | ZIP member size/hash and internal ROM header bytes |
| Uncompressed 4bpp Link tiles at `0x80000` reproduce runtime IDs | Verified locally | Exhaustive planar-decode/hash scan against `alttp-profile.toml` |
| US graphics and map stream layouts and decompression paths | Verified for the supported ROM and implemented in atlas | `zelda3/assets/{tables,extract_resources,compile_resources}.py`, `tools/alttp_atlas_{graphics,world,overworld}.py` |
| Runtime 3bpp conversion and ordinary overworld pack selection | Source-traced and implemented; capture validation still needed for scene-state joins | `zelda3/src/load_gfx.c`, `tools/alttp_atlas_overworld_gfx.py` |
| 320 dungeon room records and 160 Map32 stream pairs | Extracted and checked locally; dungeon object rendering incomplete | `tools/alttp_atlas_{world,overworld,dungeon}.py` |
| Complete sprite object/frame enumeration method | Not a static table; runtime state modeling needed | `zelda3/src/{sprite_main,player_oam,sprite,ancilla,overlord}.c` |
| Same hash can represent different semantic surfaces | Confirmed architectural limitation | `remaster/profile.h`, `macosx/mac-render.mm` |

## Progress log

- 2026-09-25: Document created before ROM research and implementation. Existing
  profile/capture capabilities and hard identity constraints recorded.
- 2026-09-25: Identified the exact local ROM and reproduced 34 profile tile
  matches from raw 4bpp ROM bytes, including Link tiles at `0x80000`.
- 2026-09-25: Verified US ROM source paths for graphics, rooms, overworld
  quadrants, staged sprite placements, and stateful OAM composition. Added
  context-aware profile migration and separate coverage gates.
- 2026-09-25: Implemented local ROM atlas inventory. Recovered every existing
  profile asset ID and matched every tile in the available Snes9x inventory;
  parsed all 320 room records and the static overworld area/stage records.
- 2026-09-25: Expanded all 160 base overworld quadrants to 655,360 Map8 tile
  words with Map32/Map16 and stream provenance. Verified a known Lost Woods
  quadrant sample and retained quadrant 71's 258-byte high-plane exception.
- 2026-09-25: Added partial dungeon tilemaps for all 320 room records, one
  verified ceiling object handler, and database rows for every omitted object,
  door, default layout, and runtime-state pass. Added a focused dungeon
  expansion design note and fixtures.
- 2026-09-25: Joined static overworld tile positions to their area graphics
  packs and decoded content IDs. Resolved 504,383 of 655,360 base positions;
  added 59,715 animation phase assignments, and retained special-area/context
  gaps. Cataloged all eight dungeon default layout streams and their 210
  object records.
- 2026-09-25: Following the user's hash-level first-pass preference, added a
  candidate profile generator. It creates all four pixel layers for 17,354
  decoded tile hashes, preserves 102 authored assets field by field, and
  transfers strong authored Link geometry to 71 related hashes. The resulting
  profile passed the project's parser and serialization round trip; remaining
  uncertainty is visible in the generation report.
- 2026-09-25: Removed full-atlas metadata from every live/captured frame and
  indexed frame-local metadata for lighting. Scene Controls now update just
  scalar settings with small undo records; load avoids redundant parse during
  saved-snapshot creation, and byte-array parsing is linear in the pixel data.
  Portable tests and the macOS app build pass.
