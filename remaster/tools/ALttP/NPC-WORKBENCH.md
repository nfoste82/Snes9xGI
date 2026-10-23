# NPC pose workbench

The NPC workbench displays **assembled sprites**, grouped by logical actor,
ROM graphics context, facing and animation step. It uses a draw-only harness
against the local ROM-backed Zelda3 reference engine, so modeled poses can be
browsed without finding and recording that NPC in live gameplay.

## Start

### Native Snes9x window

In the rebuilt app, choose **Window > Remaster NPC Poses**. The window loads the
generated `build/remaster-geometry/npc-workbench/poses.json` automatically, or use
**Load Pose Library…** to choose another generated manifest. The last library
path is remembered. No browser server is needed.

Select an NPC from the top dropdown (names include pose counts across distinct
artwork/palette variants), then its context, facing filter and pose, or use
**Play Animation**. Context and pose menus contain only the selected NPC.
The native **Graphics/Palette Context** menu combines equivalent source contexts
into one variant. Equality checks every facing/action/animation step, canonical
tile identity and palette (including hidden pixels), tile placement, flips and
OAM slot provenance; source IDs and VRAM addresses do not create extra variants.
Hover a variant to see the combined source IDs and location witnesses. Animation
steps are retained even when two steps repeat artwork. For example, Blue Guard's
26 source contexts × 16 poses become **two variants and 32 browsable poses**,
with 19 source contexts in one variant and seven in the other.

**Ignore duplicate frames** (off by default, remembered across sessions) shows
only the first pose in each consecutive run of matching artwork within an
action/facing. Original frame numbers are retained; a pose that returns after
different artwork is retained again. Comparison includes body/effects, tile
identities, palettes, positions, flips and overlap order, not timing or OAM slot
allocation. Turning it on while viewing a repeat selects the start of that run.
This filters only the Pose menu: **Play Animation still plays every original
frame at its original timing**, with the menu displaying the run's first frame
number while duplicates play. Bulk authoring and the library remain unchanged.
Native regression checks cover run boundaries, frame-number preservation,
selection mapping and the unfiltered playback timeline across the 5,468-pose
corpus. Sample master-sword Link south charge reduces from 64 to 29 menu entries.

Playback cycles frames of the current facing. Click an opaque tile to stop
playback and inspect it. Both preview panes use one fixed canvas covering every
tile placement in all variants, facings and animation steps of the selected NPC,
with a shared origin and padding. Pixel scale and draw-coordinate positions stay
fixed across frames; only actual pose movement changes a tile's position. Inspector
Previous/Next uses the same canvas.

Click an opaque tile to stop
playback and open the existing **Remaster Pixel Inspector**. Selection matches
the game window: click an unselected tile to select only it; click a selected
tile (or empty artwork) to clear the selection. Command-click adds or removes
individual tile identities without changing the others. **Select all tiles**
selects every visible tile identity in the current pose. All occurrences of a
selected identity receive the same yellow highlight in every debug view.
The inspector's layer operations (including **Fill Tile** for normals) apply
to all selected identities. The inspector displays
canonical unflipped artwork, including full palette colors for hidden pixels;
the clicked pixel is mapped through the pose's H/V flips.

**Debug View** displays Artwork, Normals, Height, Occlusion, Emission or Material
on the left, alongside an always-visible **Original Artwork** reference on the
right at the same scale. Both panes follow the current pose/animation and support
tile selection. Selection tint appears only on the left so the reference stays
untinted. The debug pane shows the entire assembled pose, including unsaved edits from the loaded matching
profile. Height is raw authored local 0–255 grayscale; missing height/material
is magenta. Normals honor pose H/V flips and fall back to height-derived or flat
normals. Occlusion uses the Pixel Inspector's green-to-red scale (0 green,
255 red) and defaults to opaque; emission is RGB scaled by intensity.
Selected pixels use the same yellow tint as in-game selection; deselect to see
unmodified debug colors.

The assembled previews do not draw tile-instance boundary outlines. Those faint
white lines previously looked like vertical/horizontal geometry seams on large
actors such as Ganon, despite not being present in the authored maps. Ownership
picking and yellow selected-identity highlighting remain unchanged.

Height view's **Min height / Max height** fields map the display gradient from
black at min to white at max, clamping values outside that range. The default
is **0–32**, and both fields are remembered across sessions. Enter values from
0–255 with max greater than min; press Return or leave the field to apply.
This only changes visualization, not authored heights.

In **Normals** debug view, choose a direction matching the Pixel Inspector's
per-pixel normal presets (+Z Front, -Z Back, +X Right, -X Left, +Y Down, -Y Up)
and click **Apply Normals to All Tiles**. It fills all 64 canonical pixels of each
tile, including transparent pixels. The **All animations/poses for this NPC**
checkbox includes every variant, facing and animation; unchecked, only the current
pose is used. Directions are canonical/unflipped, just like the inspector;
instance flips still transform the displayed normals. Requires the matching
profile and supports a single Undo.

Both normal fills and height ramps preserve existing authored per-pixel layers
by default. **Overwrite authored heights** / **Overwrite authored normals** starts unchecked; enable it to
replace the relevant layer. A tile with any existing per-pixel layer for that
operation is skipped in its entirety, even if only one pixel was edited. Heights
do not prevent adding normals, or vice versa. Other layers/options are preserved.
The status reports changed and preserved tile counts. These controls are visible
only in the corresponding Height/Normals debug views.
The action button and both checkboxes share one row; the normal direction or
height display-range controls appear immediately below it.

In **Height** debug view, **Generate Height Ramp** fills all 64 canonical pixels
of every pose tile, including transparent pixels. The lowest tile's bottom pixel
row is 0; each row upward increases by 1 across tile boundaries (clamped to 255).
Vertical flips are accounted for. **All animations/poses for this NPC** includes
all facings, animation steps and graphics/palette contexts of the selected NPC;
each pose uses its own lowest tile as the base. Requires the matching loaded
profile, supports a single Undo, and is saved with **Save Profile**. Other layers
and existing height sampling are preserved. If a reused identity requires
different canonical ramps across its placements, the operation reports the
conflict without writing inconsistent hash-keyed heights.
The conflict dialog lists tile hashes, contexts, frames, tile XY, OAM slots,
flips and required canonical top/bottom heights in selectable text.
**Show Conflicting Tiles** opens an affected pose, highlights conflicting
identities in yellow and copies the full placement report into the inspector.
Only visible pixels are highlighted, so a one-pixel edge tile has a one-pixel
highlight. **Apply Compatible Tiles** generates the nonconflicting ramps in one
Undo action while preserving all conflicting identities' existing height data.
Left/right mirrors at the same vertical position are compatible. Agahnim's
`v1:4bpp:959bc138ca495b1a` moves up 8 pixels in frames 8/12 versus 7/11;
individual poses are consistent, but all-pose ramps conflict for that identity.

Load the matching profile using **View > Load Remaster Profile…** to edit normal,
height, occlusion, material, emission and other metadata with the existing
painting, copy/paste, Undo/Redo and **Save Profile** controls. These native edits
support **Tile: Ignore flips/rotation for normals** in the Normals inspector.
Unchecked by default, it keeps authored vectors fixed across H/V artwork flips
and placement normal yaw when enabled. Pixel locations still follow the artwork;
only vector direction is held fixed. The option applies to every occurrence of
the tile hash, supports multi-selection/Undo and persists in profile schema 23
and frame schema 31. For bow tile `v1:4bpp:d4863e268dedd8d6`, enabling it lets
both mirrored halves retain +Y normals. Save the profile to retain the option.

For weapons needing independent geometry by tile flips, enable **Tile:
Geometry varies by tile flips** in Height or Normals. The adjacent menu selects
**Default / fallback**, **Normal**, **Flipped horz**, **Flipped vert**, or
**Flipped both**. Both inspector images match the selected flips, and clicking or
painting maps the displayed pixel back to canonical artwork coordinates. Paint, pixel
updates, fills, height increments, sampling, layer flips, copy/paste and resets
target that set independently. A new flip layer starts at zero height or flat
+Z normals; the inspector shows absence rather than silently editing fallback
data. The assembled NPC preview and live gameplay select overrides by each tile's
actual H/V flip flags, with per-layer default fallback when an override is absent.
Clicking a tile initially selects its flip state for authoring; the menu then
permits any set. Explicit flip-set normals are gameplay-space vectors: authored
+Y stays +Y even for vertically flipped or double-flipped bow tiles, without
requiring the separate Ignore flips/rotation option. Height and normal pixel
positions follow the artwork flips; the normal vectors are not reflected again.
Disabling the designation preserves overrides for later reuse. Save Profile
persists the designation and arrays in schema 25; frame schema 33 records the
arrays. Existing schema-24 cardinal-direction overrides remain preserved
separately for compatibility; they are not relabeled or guessed into flip sets.

**Apply to Animation** is disabled for designated source tiles and skips designated
destinations. Whole-tile Apply to Animation also preserves their geometry and
designation. NPC-wide normal fills/height ramps skip designated tiles even when
overwrite is enabled. Author those tiles explicitly through the inspector.

Flip-specific geometry works directly in live gameplay without owning-actor
annotation. For Blue Archer's east-facing bow, author **Flipped vert** and
**Flipped both** independently; each occurrence selects its matching set.

These native edits
write the loaded profile directly, unlike the HTML draft/export workflow.
The inspector has separate layer and tile action rows. **Flip Layer Horiz.**
and **Flip Layer Vert.** mirror the current 8×8 pixel layer for each selected
tile identity, support Undo/Redo, and leave other layers and tile options intact.
They move normal/RGBA pixel positions without changing component values. Artwork,
nonpixel Metadata and missing layers cannot be flipped.
Artwork can be inspected with no ROM running; select a pose tile first to enable
offline profile loading. Selecting a tile during gameplay pauses emulation.
Resume clears the inspector selection, and a subsequent scene click returns the
inspector to captured scene data.

The game window retains its scene; browsing/inspecting poses uses a separate
packet. Synthetic poses have no gameplay placement datum, so **Effective Room**
height inspection is disabled. The inspector's **Previous/Next** controls browse
corresponding tile parts across animation steps of the same actor context,
facing and action, and update the assembled NPC pose. Frame counts include
repeated artwork identities (a static head can have the same hash in multiple
phases). Matching uses OAM slot and canonical subtile ordinal, so movement and
flips do not lose the tile part; phases with a different slot subdivision are
excluded. Animation-copy/apply controls target these counterparts, with artwork
and palettes retained for every phase, rather than unrelated parts or directions.
The native browser uses the same partial pose library and coverage boundaries
described below. Automatic suggestions remain in the HTML workbench.

### Generate / HTML workbench

From the repository root, generate the local pose data once:

```sh
python3 -B remaster/tools/ALttP/alttp_npc_workbench.py --generate \
  --engine build/remaster-geometry/room-preview-engine-v21
```

The engine must already have been built by `build_alttp_room_renderer.py` from
the supported US ROM. The harness verifies the local ROM hash. Neither engine
assets nor generated art is tracked. Generation also builds the runtime profile
validator used on export. SDL2 and its `sdl2-config` are required for the harness.

Open the workbench:

```sh
python3 -B remaster/tools/ALttP/alttp_npc_workbench.py --open \
  --profile remaster/profiles/ALttP/alttp-profile.toml
```

It serves `http://127.0.0.1:8766/` until Control-C. No app rebuild is needed.

## Author

1. Select an NPC and its graphics context in the sidebar.
2. Filter the facing direction or use **Play animations**. Fixed-facing actors
   are labeled `fixed`, rather than inventing unused directions.
3. Click a tile within an assembled pose to select its canonical, unflipped 8×8
   artwork. Transparent pixels do not hide an underlying part when picking.
4. Use Artwork, Height, Normals or Occlusion to inspect the complete pose.
   Purple opaque pixels mean the chosen metadata layer is missing.
5. **Suggest this pose / Suggest all poses** seeds missing draft height, normal
   and occlusion layers. Paint the canonical tile or fill its opaque pixels;
   normal brushes normalize the entered XYZ vector. Draft editing has Undo.
6. **Save draft** writes `build/remaster-geometry/npc-workbench/edits.json`.
7. **Export runtime profile** fills missing layers in a fresh copy of the current
   source profile, validates it using Snes9x's actual C++ parser and serializer,
   and writes `build/remaster-geometry/npc-workbench/npc-profile.toml`.
8. Load that TOML through **View > Load Remaster Profile…** in Snes9x. Further
   native edits can be made with the existing pixel inspector.

The source profile is reread on export. Existing authored layers/options,
materials, rules and groups are preserved. Existing profile layers are read-only
in the browser; this first browser version edits missing layers/drafts. Newly
classified NPC hashes receive an OBJ-only prop-material fallback rule. A profile
without an `npc` material receives a conservative default prop material. Draft
changes update every pose using that canonical hash. Stale browser revision
writes are rejected. Export is explicit; browser draft saving does not hot-reload
the emulator.

## Automatic suggestions

This is deterministic **silhouette analysis**, not a learned vision model and
not anatomical recognition. Contiguous opaque spans produce rounded,
viewer-facing normals; silhouette top/bottom edges receive small Y tilts.
Separate spans avoid treating a detached weapon and torso as one broad cylinder.
Palette brightness does not become geometry. Instance flips are inverted when
writing canonical normal components.

Height uses opaque local `13-row` ramps and zero-transparent/255-opaque
occlusion, compatible with the existing runtime sprite-part alignment in
`S9xRemasterAlignGeneratedSpriteParts`. The browser Height view displays local
tile values, not assembled world heights. Editing that ramp changes the exact
generated-height signature and can disable automatic part alignment; custom
whole-body geometry needs explicit actor placement support. The existing
alignment still groups nearby OAM parts heuristically, not by logical NPC ID.

Conflicting inferred normals for a reused hash are omitted from all-pose
suggestions. Heights/occlusion remain eligible because they use a common local
contract. Hashes also present in background graphics are excluded from automatic
suggestions: current profile geometry is hash-keyed, so an OBJ-only material rule
does not make its height/normal maps context-specific. A context with inherited
graphics slots remains browsable but suggestions are disabled until its entry
state is reconstructed.

## Coverage boundary

### Synchronized Link sword effects (2026-10-21)

Link's **sword charge / charged movement** and **spin attack / charged release**
actions now include their independent ancilla effects in the assembled pose.
Click the visible sparkles just like body/sword tiles to author them in the native
Pixel Inspector, including emission. No gameplay capture is required. Metadata
remains tile-identity-based: sparkle artwork is shared with other game effects,
so editing it can affect those uses too. The source profile is never modified by
library generation.

Charging contains 64 chronological ticks, with sampled six-step body movement,
random-position charge sparkles and the full-charge flash. Release contains 41
ticks, covering all 12 timed body phases, the initial flash, rotating trail and
ending. A 64-tick charge warmup preserves residual charging effects at release.
Both sequences run the reference's actual ancilla dispatcher/timers and draw
routines; body spin timing follows the checked player tables without running
collision/room/AI logic. Every pose restores RAM/VRAM; charge RNG uses a fixed
seed. This is one reproducible charge trajectory, not all random positions or
every movement/charge timing combination. Other independent Link effects (e.g.
ordinary sword-swing sparkles, wall-hit sparks, item-use effects) remain unmodeled.

Native and HTML playback honor `duration_frames` (one 60 Hz tick here), retaining
the legacy 180 ms delay for older/un-timed poses. Body repeats are intentional:
effects continue moving between body transitions. The manifest pins `ancilla.c`
alongside the player sources and marks effect parts with `effect: true` for audit.
Coverage remains partial/modelled-unverified until compared with state-pinned
gameplay. The regenerated library contains 242 contexts and 5,468 poses.

Validation: 14 Python tests including ROM-backed timeline/equipment/RNG/effect
artwork checks; native decoder, counterpart and selection corpus over all 5,468
poses / 1,918,414 opaque picks; portable pose assembly; profile export/preservation
check (2,403 existing assets preserved in a separate candidate); Debug and Release
app builds; `git diff --check`. Reviewed the four-facing release contact sheet at
`build/remaster-geometry/npc-workbench/link-spin-effects.png`. Interactive native
playback, clicking/painting sparkles and state-pinned live parity remain pending.

### Source inventory and action expansion (2026-10-19)

Generation now writes `coverage.json` and embeds the same inventory in
`poses.json`. It inventories **all 243 main sprite dispatch entries**, including
unplaced/dynamically used types, with dispatcher symbols, direct calls, reachable
draw/animation candidates, state-input names, generated models and explicit gaps.
This is a static review work queue, **not an exhaustive branch/reachability proof**.
Current inventory: 83 partially modeled sprite types, 159 unmodeled and one null
dispatcher. Player, followers, ancilla, garnish, overlords, mixed-BG actors and
cutscenes are separately listed systems; a sprite dispatch scan cannot cover them.

The expanded library has **242 contexts and 3,380 poses**. Link contributes 1,264
poses across seven sampled equipment/palette configurations. Walking includes
all nine steps per direction. Additional bounded actions: sword swings (9),
charged movement (6), wall taps (3), chronological spin release (12), bunny
movement (4), swimming idle (2), recoil, electrocution, one/two-handed item holds
and sword held overhead. Sword/shield and item DMA are applied independently of
body DMA; body shadows are excluded. Three tunics and all sword/shield levels
are sampled, **not every equipment combination or glove variant**. Independent
charge/spin sparkles were originally excluded; the synchronized extension above
adds those effects and supersedes the historical action counts here.

`alttp_npc_coverage.py` pins the reference source digests, checks the movement
bound and compares the adapter's chronological spin table with `player.c`.
Other newly added bounds remain explicitly inspected state contracts, not
automatic symbolic derivations. The native and HTML browsers have an Action
filter; native playback stays within the selected action/facing. Bulk authoring
is labeled **All catalogued poses** and coverage remains visibly PARTIAL.

Read-only capture artwork audit:

```sh
python3 -B remaster/tools/ALttP/alttp_npc_workbench.py \
  --audit-capture "$HOME/Library/Application Support/Snes9x/Remaster/frame_overworld_x07f4_y072a.s9xrmf"
```

Repeat `--audit-capture` for more packets. `capture-audit.json` separates missing
library artwork from missing captured height/normal layers. It uses the actual
C++ frame decoder, observes OBJ identities and expands intersecting capture
animation groups. Groups without a representative OBJ member cannot be assigned;
the report is an artwork lower bound, not logical-actor attribution or assembled
pose/state parity. Captured authoring is historical, not the current disk profile.
The castle fixture reports 46 observed identities, zero absent from the expanded
library, and ten without captured height/normals. No profile is changed.

Remaining work toward the exhaustive target: review each inventory candidate,
derive valid state/action vectors and table limits for the other families, model
dynamic contexts and sequence-dependent assemblies, complete Link item/carry/
pit/stair/swim/medallion/death/cutscene states and equipment combinations, and add
state-pinned live actor/assembly parity observations. **No family is marked fully
verified by this first milestone.** Historical counts/boundaries below describe
the previous catalog, not the expanded generation.

Validation for this milestone: 13 Python tests; native decoder/counterpart/
selection corpus over all 3,380 poses and 1,227,524 opaque pixel picks; portable
NPC assembly test; profile export/idempotence check preserving all 2,058 existing
assets; full arm64 Release app build. Export check writes a separate candidate,
not the source profile. Artwork contact sheets were reviewed; interactive Action
menu, playback, painting/Undo and live sword-state parity still need confirmation.

The expanded catalog contains **99 named character/enemy families, 236
graphics/palette contexts and 2,140 draw poses** (2026-10-13). The original
11-family slice omitted most NPC draw routines. `alttp_npc_catalog.py` now
describes bounded draw states for villagers, shop/game operators, swordsmiths,
Syrup/assistant, storytellers, fairies, Zelda, the sage/uncle, old man, Kiki,
Catfish/King Zora, Agahnim, crystal-maiden shared artwork and additional guards.
The native NPC dropdown is alphabetized and counts these generated poses.

Dungeon additions include Keese (Bat), Rat, Rope (Snake), blue/red Armos Knights,
Armos Statue, Hardhat Beetle, Mini Helmasaur, Stalfos, Gibdo, Wizzrobe, Freezor,
Pengator, Wallmaster, Zol, red/blue Bari, Popo, green/red Eyegore and Beamos.
Boss adapters include Lanmolas, Moldorm, Helmasaur King (mask stages), Arrghus,
Mothula, Blind, Kholdstare and Vitreous. New dungeon contexts load the room-header
palette using `GetDungPalInfo`, keeping distinct palettes even for a shared
graphics set. Wallmaster derives contexts from the actual overlord-09 spawner
placements instead of requiring a statically placed hand. Segmented bosses use
bounded straight-travel history buffers and the reference's assembled draw
routines; these sample configurations are not recorded gameplay trajectories.
Ground shadows are excluded. Trinexx, remaining enemy families, boss projectiles,
minions, and additional attack/death/phase states still need bounded adapters.

One sprite type can dispatch several visually distinct characters: the adult,
storyteller and shopkeeper models filter source room contexts. World variants
use explicit light/dark draw state. Special overworld areas 0x80/0x81 use the
reference's special-exit graphics/palette tables instead of being dropped for
null atlas selectors. Follower-backed actors load the appropriate graphics and
apply their draw-selected DMA before hashing; Uncle's equipment DMA is also
applied. Every pose restores both RAM and VRAM checkpoints.

Guards enumerate table-driven walking phases with body, head and weapon parts;
the NPC tables provide their bounded animation phases where applicable.
Ground shadows are excluded from body artwork/geometry.

The manifest lists every remaining placed sprite type as unmodeled and records
missing catalog models separately. Its character count is a visual-family count,
not a claim that every entry on the Zelda Dungeon character page is covered.
Shared artwork is grouped (Sahasrahla/Aginah, older villager/Human Cucco and the
crystal maidens); guards and light/dark variants are distinguishable entries.
The crystal-maiden model uses explicitly loaded shared follower graphics in a
marked borrowed context; individual rescue palettes remain unverified.

Link now has a player-OAM adapter with 24 walking poses (four facings × six
steps), green tunic and no equipment. It applies the draw-selected body DMA from
the ROM's Link graphics. Ganon has 34 body poses (two facings × 17 draw frames),
using room 000's sprite sheets and room-header dungeon palette; independent
trident/attack effects are excluded. King Zora's special screen 0x81 resolves
through special exit 0x182 to graphics set 0x0e, rather than incorrectly using
exit 0x181's fairy graphics set 0x0c. Reviewed assembled previews show distinct
Link, Ganon, King Zora and Cursed Fairy artwork.

Remaining character-page gaps: Link equipment/nonwalking actions, Ganon attack
effects, the travel Duck, Venus,
Pikku, Ghostly Proprietor and the Talking Tree's background-composited body.
These need player, boss, travel/cutscene or mixed-background adapters. Maple is
GBA-only and does not exist in the supported SNES ROM. Special attack/recoil/
death states, independent guard head turns and additional effects remain open.
These poses are `modeled_unverified`: invoking a draw routine proves its output
for the recorded vector, not gameplay reachability or parity with Snes9x.
Older character models still use provisional dungeon palettes; the new dungeon
enemy models use explicit room-header palettes. Inherited zero graphics slots
use the harness checkpoint's previous loads and remain marked as inherited.
Draw-only previews still need interactive/in-game parity review.
The manifest pins the reference draw-source digest, ROM hash, graphics/palette
selectors and source placement witnesses. Additional routines should be added
with bounded valid draw-state vectors, never arbitrary RAM brute force.

## Verification

```sh
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_npc_workbench.py'
python3 -B remaster/tools/ALttP/alttp_npc_workbench_check.py \
  --output build/remaster-geometry/npc-workbench \
  --profile remaster/profiles/ALttP/alttp-profile.toml
git diff --check
```

The corpus check verifies canonical hashes, nonempty tile art, byte layers,
shared-hash ambiguity, runtime profile round trips, preservation of every field
of every existing authored asset, and export idempotence. It emits a contact
sheet and `check-report.json`; `npc-profile-check.toml` is an explicitly generated
candidate, not the user's edited source. Browser interaction and in-game visual
lighting still require interactive review.

Native integration checks:

```sh
c++ -std=c++17 -O2 -DNDEBUG remaster/tests/npc_pose_test.cpp \
  -o build/remaster-tests/npc_pose_test && build/remaster-tests/npc_pose_test
clang++ -std=c++17 -O2 -fobjc-arc -framework Cocoa \
  macosx/remaster-npc-browser-test.mm -o build/remaster-tests/npc_browser_test
build/remaster-tests/npc_browser_test build/remaster-geometry/npc-workbench/poses.json
```

The native decoder corpus check covers all generated poses and canonical opaque
picks. Portable checks cover foreground transparency, flips, RGB/BGR conversion,
VRAM address units, hash rejection and profile refresh/reset. Interactive
window/pose/pixel painting/Undo/save/reload and scene-resume verification remains
pending.
