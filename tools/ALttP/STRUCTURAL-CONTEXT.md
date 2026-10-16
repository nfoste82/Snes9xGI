# Height-blind structural ROM context

This standalone evidence browser expands the ROM-only visual context. It does
**not** generate a heightmap, geometry proposal, semantic classification, object
ownership, legal movement graph, or runtime validation. A fresh agent needs only
this document, the tool, and the generated `commands.json`/bundle. The parent
solver is independent; no earlier experiment or profile is an input.

## Prepare and command API

Python 3.11+ with Pillow. Run from repository root. Read the generated
`commands.json` first: it contains executable argument arrays using the working
interpreter and absolute tool/bundle paths. In this checkout the tested interpreter
is `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visual-context-venv/bin/python`.
Use that in place of `python3` below if the system Python lacks Pillow.

```sh
python3 -B tools/ALttP/alttp_structural_context.py prepare --rom "$HOME/Downloads/Zelda_ALTTP.zip" --visual-bundle build/remaster-geometry/castle-visual-context --output build/remaster-geometry/castle-structural-context
python3 -B tools/ALttP/alttp_structural_context.py overview --bundle build/remaster-geometry/castle-structural-context --max-size 1024
python3 -B tools/ALttP/alttp_structural_context.py view --bundle build/remaster-geometry/castle-structural-context --x 384 --y 512 --width 256 --height 224 --scale 3
python3 -B tools/ALttP/alttp_structural_context.py inspect --bundle build/remaster-geometry/castle-structural-context --x 208 --y 648
python3 -B tools/ALttP/alttp_structural_context.py examples --bundle build/remaster-geometry/castle-structural-context --x 208 --y 648 --unit 16 --match flipped-words --limit 24 --output build/remaster-geometry/castle-structural-context-views/example-208-648
python3 -B tools/ALttP/alttp_structural_context.py examples --bundle build/remaster-geometry/castle-structural-context --x 320 --y 776 --unit 16 --match id --limit 24 --output build/remaster-geometry/castle-structural-context-views/example-320-776
python3 -B tools/ALttP/alttp_structural_context.py metatiles --bundle build/remaster-geometry/castle-structural-context --unit 32 --offset 0 --limit 24
```

* `prepare`: ROM ZIP (one SFC/SMC member), SFC or SMC, copier-header stripping
  inherited from the existing `Rom` reader. Only USA SHA256
  `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
  Output must be new/empty. Existing visual bundle is checked against a fresh ROM
  decode, including all RGB bytes and collision arrays. No capture import.
* `view`: arbitrary **area-local pixel** x/y (pan by changing x/y), width/height
  default 256/224, integer nearest-neighbor scale 1..16 (default 3). Nonaligned
  crops supported; out-of-bounds rejected unless `--clamp`. No wrapping/padding.
* `overview`: complete image; `--max-size` 1..4096 (default 1024). Downsampling
  is only a display choice: native RGB and full evidence remain available.
* `inspect`: exact cell, Map32 and Map16 IDs/origins, relative member coordinates,
  definition raw bytes/addresses, full Map8 word, tile number, collision index,
  H/V flags, palette, priority, raw/adjusted collision and cited detector effects.
  Reports area and world pixel coordinates and source quadrant.
* `examples`: align selected x/y to `--unit 8|16|32`; `--match id` matches the
  exact Map16/32 ID (full word for Map8). `--match flipped-words` compares the
  complete ordered Map8 word matrix under identity/H/V/HV reflection: reorder
  members AND XOR member artwork flips. Palette/priority/tile bits must agree.
  This is exact composition evidence, not artwork similarity or semantics.
  All possible transforms are reported when symmetric; none is chosen as owner.
* `metatiles`: sorted distinct ID/word catalog for `--unit 8|16|32`, showing its
  first actual placement plus surrounding RGB and whole-area repeated count.
* Both sheet commands: `--offset >= 0`, `--limit 1..64` (default 24), four columns,
  native context extends 32 pixels beyond each selected block, shown at 2x.
  Context clips explicitly at area edges. Yellow rectangle identifies selection.
  JSON records every member and context rectangle. `examples` JSON also keeps
  **all** matching placements even if the image page is limited; `total`, `shown`,
  `remaining` make paging explicit. Catalog counts are not viewport counts.

Every image command prints absolute PNG and `evidence.json` paths; open PNGs
with the image/read tool. `view`/`overview` write separate RGB, detailed collision,
Map8, Map16 and Map32 grid overlays. At scale >=3 overlays include hex words/IDs
and adjusted collision bytes. At smaller scales use inspection/JSON for detail.
Evidence JSON retains every intersecting Map8 cell with the full composition
chain and raw bytes: reduced overviews never replace native source detail.
Default outputs are sibling `<bundle>-views/<command>/`; choose `--output` to
retain named pans/pages. Outputs inside the immutable input bundle are rejected.
No hidden pan state or emulator/camera control is needed.

## Strict input and grouping boundary

Sealed bundle has exactly six files:

| File | Evidence |
| --- | --- |
| `artwork.png` | Lossless complete native ROM RGB, 1024x1024 for castle |
| `collision.json` | All Map8 words, raw/adjusted bytes, full 512-byte ROM LUT |
| `composition.json` | Complete row-major Map32 IDs, Map16 IDs, Map8 words; used definitions with packed six-byte groups, raw eight-byte Map16 records, addresses, variants, original compressed quadrant streams |
| `behaviors.json` | All 256 byte descriptions, literal handler effects or explicit ambiguity, pinned citations |
| `bundle.json` | ROM, area/state, quadrants, world origin, graphics/palette/animation pins, denominators, source references, payload digests and limitations |
| `commands.json` | Standalone machine-readable command argument arrays |

All payload digests and exact file set checked at every load; symlinks rejected.
Readers also verify LUT/flip adjustment, definitions against raw packed bytes,
member expansion against all placed children, array completeness, canonical
behavior descriptions and image dimensions. Preparation refuses noncanonical
visual artwork/manifest/collision even if their checksums were regenerated.
Recursive field guards reject authored/semantic keys in JSON inputs. This is a
tool-generated schema, not an arbitrary annotated JSON import interface; hashes
are integrity checks, not adversarial signatures. Never mix annotations into it.

Only existing actual ROM decoder functions are called: `Rom`,
`extract_overworld`, its packed definition decoder, and the visual tool's ROM
graphics/palette preparation. `alttp_atlas` imports profile-related Python code
as a side effect, but no profile function is called and no profile data is read.
No candidate, capture, prior result, asset semantic profile, tile-content identity,
authored local data, inferred grouping, or solver state is serialized or consumed.
There are no object-boundary claims: **mapblocks are not objects**. Equal IDs or
reflected word patterns do not establish shared material, structure, or level.

## Collision citations and interpretation limits

Descriptions were checked against snesrev/zelda3 commit
`fbbb3f967a51fafe642e6140d0753979e73b4090`,
[`src/tile_detect.c`, `TileDetect_ExecuteInner`, lines 256–526](https://github.com/snesrev/zelda3/blob/fbbb3f967a51fafe642e6140d0753979e73b4090/src/tile_detect.c#L256-L526).
Each byte has a switch-case locator, literal outdoor effect and citation. Existing
`tools/ALttP/zelda3-MIT-LICENSE.txt` applies to the reference transcription.

Outdoor lookup follows `Overworld_GetTileAttributeAtLocation`, lines 14–29:
LUT index `word & 0x1ff`; for `0x10 <= raw < 0x1c`, OR in `(word >> 14) & 1`.
Not XOR. V flip does not adjust the collision byte. Artwork flips independently.
Inner/outer diagonal types expose different flags and exact diagonal state
`{4,0,6,2}[byte & 3]`. They describe **diagonal collision, not elevation ramps**.
Deep water 08, high-nibble outdoor water 0b, shallow water 09, short ladder 0a,
grass, digging, liftable, hookshot, ice and conditional pit/spike detectors stay
distinct. Water behavior/material does not locate a water surface vertically.

Ledge 28/29, 2a/2b, 2c/2e, 2d/2f expose exact distinct detector flags. No compass
travel direction is assigned: the downstream movement/state handlers have not
been traced in this tool. Detector comments alone do not prove permitted motion
or signed relationships. Similarly stair flags do not establish a level change.
Untranscribed handlers (e.g. chest/door branches) are explicitly
`ambiguous_unmodeled`, not mislabeled normal. Raw bytes remain available for all.

The castle base area currently uses 31 distinct adjusted bytes, all covered by
the literal transcription. This does **not** mean legal movement is reproduced:
multi-sample Link footprint, equipment, approach direction, event edits, gameplay
planes, timing, rain/fades, actors and live collision remain unsupported.
Normal-sample flags and collision-free artwork are not floor identity evidence.

## Actual artifacts and tests

Bundle: `build/remaster-geometry/castle-structural-context/`, area 0x1b,
quadrants 0x1b/0x1c/0x23/0x24, world origin (1536,1536), base ROM no edits,
animation phase 0. **16,384 Map8 / 4,096 Map16 / 1,024 Map32 placements**.
RGB palette fully resolved, 15,385 ordinary and 999 explicit phase-resolved cells.
No source detail discarded. No alternative animation phases generated in this
bundle; create a separate visual bundle at phase 1/2 and a separate structural
bundle if needed. Full animation timing/state emulation is not provided.

Sibling `castle-structural-context-views/` contains native overview, the default
view, examples at (208,648) and (320,776), and Map32 catalog page. The former
coordinates were selected by viewing the ROM RGB, not a semantic profile; their
filenames/records carry no wall/shore role labels. Context is evidence for the
fresh agent to interpret independently, not a transferred classification.

```sh
python3 -B -m unittest discover -s tools/ALttP -p test_alttp_structural_context.py -v
ALTTP_STRUCTURAL_ROM="$HOME/Downloads/Zelda_ALTTP.zip" python3 -B -m unittest discover -s tools/ALttP -p test_alttp_structural_context.py -v
python3 -B -m unittest discover -s tools/ALttP -p test_alttp_visual_context.py -v
git diff --check
```

Tests cover raw composition/member coordinates, exact reflection/XOR including
rejection of palette/priority or reorder-only near matches, cross-quadrant global
repeat counts/world coordinates, detailed collision distinctions and all flip
adjustments, recursive leak guards, output separation, and optional pinned-ROM
end-to-end all CLI paths, immutable payloads, input read allowlist, resealed member
tampering and noncanonical RGB rejection. Portable run skips only ROM integration.

Verdict: **reviewable but incomplete as gameplay/geometry evidence**; complete
base-ROM composition context, not solved heights. No runtime or height changes,
so no runtime build or room-height publication gate applies. Runtime captures,
overlapping planes, geometry solving and complete-room validation remain the
independent solver's responsibility, not evidence supplied by this tool.
