# ROM-only visual context (height-blind)

`alttp_visual_context.py` gives a fresh agent its own complete-area artwork,
collision lookup, offline crops, pan steps, and zoomed-out overview. No manual
gameplay pan/capture is needed. Python 3.11+ and Pillow are sufficient. Run from
the repository root. No core/UI/runtime files are changed.

For this checkout's generated bundle, `commands.json` already names the tested
Pillow-enabled interpreter at
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visual-context-venv/bin/python`.
The default Homebrew `python3` currently lacks Pillow: use the descriptor's
interpreter (or install Pillow in your own virtual environment). This temporary
environment is not required by the portable tool or committed to the project.

## Prepare Hyrule Castle, then browse

```sh
python3 -B tools/ALttP/alttp_visual_context.py prepare --area 0x1b --output build/remaster-geometry/castle-visual-context
python3 -B tools/ALttP/alttp_visual_context.py overview --bundle build/remaster-geometry/castle-visual-context --max-size 512
python3 -B tools/ALttP/alttp_visual_context.py view --bundle build/remaster-geometry/castle-visual-context --x 384 --y 512 --width 256 --height 224 --scale 3
python3 -B tools/ALttP/alttp_visual_context.py step --bundle build/remaster-geometry/castle-visual-context --dx 128 --dy 0 --clamp
python3 -B tools/ALttP/alttp_visual_context.py inspect --bundle build/remaster-geometry/castle-visual-context --x 512 --y 640
```

Preparation defaults to `~/Downloads/Zelda_ALTTP.zip`; use `--rom PATH` for a
ZIP (one ROM member), SFC or SMC. Copier headers are stripped. Only the pinned
USA SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`
is accepted. The output must be new/empty: no stale artifact reuse.

Read `commands.json` first for machine-readable argument arrays. Invoke them
with the shell tool, then open printed absolute `artwork_png` and
`collision_png` paths with the agent's image/read tool. These are real PNGs,
not an HTML browser requirement. `view` produces 768x672 PNGs with the defaults.
`overview --max-size 1024` displays the full castle at native resolution;
`--max-size 512` zooms out. Every crop also writes `view.json` provenance.
Use `--output DIRECTORY` to retain several named views rather than overwrite
the last view of that command. An agent can issue any sequence of `view` or
`step` calls itself; it does not need the user to capture pans.

Coordinates are **area-local pixels**, origin at the upper left. Castle area
0x1b is the complete 1024x1024 area: quadrants 0x1b, 0x1c, 0x23, 0x24, including
surrounding ground. Small area heads produce 512x512 instead. Satellite IDs and
special areas 128+ are rejected. No adjacent-area stitching is implemented.
Out-of-range views fail unless `--clamp` is explicit. Requested/actual rectangles
and clamping are recorded; there is never modulo wrapping or black padding.
`inspect` always rejects out-of-range points. Non-cell-aligned crops work.

`view` stores offline pan state in the sibling `<bundle>-pan.json`; `step`
requires that state and validates its bundle digest. Override with `--state`.
Overview does not move pan state. Images/provenance default to the sibling
`<bundle>-views/{view,step,overview}/`; output/state inside the bundle is rejected.

## Strict data boundary

Reference-guided generation is a separate consumer workflow: see
`AIDocs/alttp/09-reference-guided-world-authoring.md` for user-corrected object
examples, transferable shape/normal rules and a copyable agent brief. References
must be explicitly allowed and supplied **outside** the sealed ROM-only bundle.
This does not loosen existing height-blind experiment restrictions or change what
the visual-context tool reads/writes. Label runs using references accordingly.

The sealed input bundle contains exactly four files:

* `artwork.png`: complete base ROM BG artwork (no sprites or annotations).
* `collision.json`: row-major Map8 words, raw and adjusted attribute bytes,
  complete 512-byte ROM lookup table at `$8E:9459`, dimensions and provenance.
* `bundle.json`: ROM/state, quadrant, graphics/animation/palette context,
  limitations, decode denominators and integrity digests.
* `commands.json`: machine-readable agent invocation descriptor.

No generated candidate, authored profile, semantic metadata, assembly, support,
placement base, tile content hash, or effective elevation is serialized or read.
Internal artwork content IDs are used only by the existing ROM graphics resolver
to select decoded pixels, never for classification. `Rom` is reused from
`alttp_atlas`; its imported profile code is not invoked and no profiles are read.
The only preparation input is the ROM. Additional files inside a prepared bundle
are rejected, and artwork/collision/descriptor digests are checked on each use.
The JSON schemas are tool-generated, not an interface for importing arbitrary
external captures or annotations. Do not add unrelated data to these files.

## Artwork and palette evidence

Reuses `extract_overworld`, `extract_graphics`, `BaseOverworldGraphics`, and
ordinary area-head ROM tables. Animation replacement is explicit
`--animation-phase 0|1|2` (default 0). Unknown graphics remain **magenta**, with
status counts, rather than invented blank cells.

Normal unfaded original RGB colors are reconstructed directly from the ROM:
area selector `$80:FD1C`, main colors `$9B:E6C8`, auxiliary colors `$9B:E86C` and
`$9B:E604`. Loader transcription follows snesrev/zelda3 commit
`fbbb3f967a51fafe642e6140d0753979e73b4090`, `load_gfx.c`:
`kOwBgPalInfo`, `OverworldLoadScreensPaletteSet`, `Overworld_LoadPalettes`,
`Palette_Load_OWBGMain/1/2/3`, and `Palette_GetOwBgColor`; asset addresses follow
`assets/compile_resources.py`. See `zelda3-MIT-LICENSE.txt`.

Negative auxiliary selectors require entry-state inheritance and are **not**
guessed. Unknown palette slots render diagnostic index grayscale; exact used
pixel/slot counts and `display` mode are recorded. `--grayscale` explicitly
forces diagnostic grayscale. Castle's ROM theme resolves every used palette
slot. These are normal-state game palette colors, not a claim of matching a live
rain/fade/time/event state. No palette is borrowed from a capture, and no authored
v4 input or generated image is used. There is no capture-palette import option.

## Collision is not floor identity or walkability

`inspect` returns Map8 word, low-nine-bit collision index, raw/adjusted attribute,
H/V flags, limited outdoor behavior summary and source state. Exact adjustment
matches `tile_detect.c:Overworld_GetTileAttributeAtLocation`: for raw
`0x10 <= a < 0x1c`, `a |= (word >> 14) & 1`. This is **OR**, not XOR; V flip
does not change the byte. Artwork is independently flipped in both axes.

Collision overlay colors are a **sample behavior inventory**, not geometry:
red blocking, amber directional slope, cyan `00` normal sample, blue deep water,
light blue shallow water, purple all other/unmodeled behaviors. Legend and
counts are included in each JSON. `00` is not evidence that the picture is floor.
All inspection results say `walkability: not_evaluated_state_and_actor_dependent`.
Even blocking/normal sample labels do not reproduce Link's multi-sample footprint,
equipment, movement phase, one-way edges, or special state handlers. No flood fill
or movement graph is synthesized. Raw base-ROM collision can differ after edits.

This tool does not execute the emulator. All outputs say
`base_rom_no_runtime_edits`, with null `runtime_capture_evidence`. Runtime
overlays, persistent edits, actors, rain, fades, color math, animation timing and
gameplay-plane overlap are outside its evidence. It is visual context, not a
geometry repair, runtime validation, or height-publication artifact.

## Tests

```sh
python3 -B -m unittest discover -s tools/ALttP -p test_alttp_visual_context.py -v
ALTTP_VISUAL_ROM="$HOME/Downloads/Zelda_ALTTP.zip" python3 -B -m unittest discover -s tools/ALttP -p test_alttp_visual_context.py -v
git diff --check
```

Tests cover all 256 collision bytes under all four H/V combinations, OR vs XOR,
explicit boundary/clamp behavior, artifact dimensions and crop quadrants, pan
state/bundle separation, palette loading and unresolved inheritance, integrity,
inspection field exclusions and no guessed walkability. The optional ROM test
builds all 16,384 castle cells, checks full original-color resolution, recursively
checks serialized fields, and guards file reads against non-ROM input artifacts.
This is a tooling-only change: no geometry/height output changes, no runtime
build or room-height validator needed. A later geometry change still needs its
own complete-room validator and matching gameplay evidence.
