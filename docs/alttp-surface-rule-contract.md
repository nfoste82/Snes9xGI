# ALTTP deterministic surface rule contract (first executable slice)

Status: conservative offline solver and whole-room audit, not approved room
geometry. This document describes what **exists** and what remains necessary
before the profile/runtime can consume any solved heights.

## Run the full-room audit

Using the local ROM-matched reference renderer built by
`tools/build_alttp_room_renderer.py`:

```sh
python3 -B tools/alttp_surface_audit.py \
  --renderer build/remaster-geometry/room-preview-engine-v20/alttp-room-preview \
  --atlas build/remaster-geometry/alttp-atlas.sqlite --rise 50 \
  --floor-profile build/remaster-geometry/alttp-profile-wall-review.toml \
  --floor-review-anchors tools/alttp-floor-review-anchors.json \
  --rules tools/alttp-surface-rules-v1.json \
  --rooms 0x55 0x61 0x60 \
  --output build/remaster-geometry/surface-audit-room-local
```

The tool generates a checked linked room export and provenance sidecar for each
room, then writes a deterministic JSON report and a manifest with denominators.
The checked-in rules file intentionally has **no accepted templates**. Current
reports mark 8,192 BG1/BG2 cells per room `unknown`; this is an audit of missing
semantic evidence, not a claim that these rooms have no geometry. The renderer
uses one entrance/default state per room. Source state and ROM pinning are not
yet encoded in the rule contract; do not publish a height map from these runs.
With the local ROM atlas, the audit inventories oriented stair objects as
constraints: one in 0x055, one in 0x060, and three in 0x061. They currently
report unclassified footprints/landings instead of being silently ignored.
The audit generates standalone `room-XYZ-stair-review.svg` files from six
linked PPU viewports stitched in room-local coordinates. The earlier SVGs
embedded the separately generated gallery, which copied side padding from
the PPU's wider row: its art was displaced relative to tilemap coordinates.
The linked capture's colors must also be read at the PPU's **fixed screen
origin** (`kPpuExtraLeftRight`), not shifted by its variable extra-left-art
width. The variable offset caused a duplicated/cut doorway and an entire
vertical seam in the earlier room 0x061 review; the BG ownership capture was
already at the correct screen origin. A ROM-backed horizontal viewport-overlap
test guards the color alignment across that seam.
The SVGs now show full-room art with light-green high-side landing samples, dark-green low-side samples,
orange stair-handler tiles, and pink cells where another object is the final
writer. Hover to see tile hashes and writer provenance. Their colors show the
**handler-relative** elevation direction only, not verified playable heights.
The local run observed 16/16 stair-writer cells in 0x055 and in each of the
three 0x061 stairs, but only 4/16 in 0x060. An apparent stair tile painted by
another writer is not accepted as proved stair structure without support
provenance. The scene reports remain unresolved; these are artifacts to review,
not a replacement for the existing live room masks.

With `--floor-profile`, the review SVG additionally shades **candidate** flat
art connected to both sampled stair landings: light green upper and dark green
lower. The corresponding `room-XYZ-floor-coverage.json` lists the tile cells,
component IDs, stair links, unanchored components, and limits. This reuses the
geometry prototype's flat-art candidate rule (authored wall-top group plus
sufficiently frequent zero-height +Z art). The linked renderer also exports
`ALTPAT1` alongside each raw capture: two complete 64×64 BG2 collision-attribute
planes, selected by Link's current gameplay layer. Captures from renderer v20
and earlier omitted `Init_LoadDefaultTileAttr()` (normally called by
`Module05_LoadFile`, which the headless startup bypasses). Their collision
inventories are invalid: most wall attributes incorrectly became `00`.
Renderer v21 initializes the default lookup before loading the dungeon.
The audit records corrected values as cross-checks, not standalone visual
floor classifications. Flat BG2 cells written
by a later overlay where BG1 differs are shaded faint amber and excluded from
floor propagation; they can represent walls, a bridge, or covered ground. This
removes the incorrect lower-wall fill in 0x060, but its stair landing is covered
as well, so its two levels now remain unassigned until support is identified.
The user-reviewed disconnected north walkway and chest-side platform in 0x055
are anchored as **lower** using `tools/alttp-floor-review-anchors.json`, without
applying the anchors to other rooms. Their candidate floor fills are lighter
than stair-linked fills, and each tile records `review_anchor` as evidence.
Other disconnected flat-art candidates are outlined in gold with their
collision attributes on hover. Covered BG2 candidates appear faint amber with both BG tile IDs
and collision values on hover. Border cells need verified
assembly semantics before coloring. The rule file still has no accepted
templates; the surface reports remain unknown and no live height data is updated.
The audit also emits `room-XYZ-collision-00.svg`: side-by-side linked room art
with `00` highlighted on each collision plane (cyan on both, orange on only
that plane). This is a tile-attribute inventory, not a reachability map.

## Movement and support evidence required for floor coverage

The next floor classifier must answer a different question from the attribute
overlay: **where can Link's feet actually go, in a specified room state and
gameplay plane?** Treat `(room, state, actor plane, Link foot position)` as the
movement node, not `(BG tile, attribute)`; a tile with attribute `00` can be
part of a wall's picture without being a reachable place for Link's feet.
The room loader derives `dung_bg2_attr_table` from BG2 tile IDs using
`attributes_for_tile`, then applies staircase, door, and other object/state
overrides. In `TileDetect_ExecuteInner`, `00` adds no blocking collision bit,
while `01`/`02` do; other attribute classes trigger doors, ledges, pits,
liftable objects, and other interactions. `TileDetect_Movement_X/Y` reads
three attributes along Link's leading edge, and the movement handlers consume
the resulting collision bits. A single nonblocking sample is not an admissible
movement edge. These *completed, per-state collision-attribute planes*, not
raw tile IDs or an isolated `00`, are the fast input for traversal.

For ordinary static ground, precompute legal Link-foot positions and cardinal
transitions directly from those completed arrays: use the engine's actual
leading-edge sample offsets, applicable plane, blocking-behavior lookup,
footprint, and room collision mode. Cache the state/plane-specific result as
bitsets or a compact movement graph. A room-wide graph traversal then requires
no rendered frames and no per-edge calls into the full Zelda3 frame loop.
The earlier 0x060 flood into walls used the uninitialized v20 collision LUT;
it is **not evidence that Zelda3 lacks usable collision data**. The corrected
doorway/stair graph below stays inside the connected room regions. Visual
support and occlusion still need separate evidence.
Keep special attributes and transitions (doors, stairs, ledges, moving blocks,
pots, switches) explicit: either model their rules and state changes or mark
their edges unresolved. Validate sampled ordinary edges *and* representative
special edges by instrumenting attempted moves in the reference engine and
comparing start/end position, layer, room, and state against matching Snes9x
gameplay. Preserve one-way and transition edges rather than replacing the
movement graph with an undirected flood fill.

Seed each reachable region from a verified door/entrance or another observed
Link position on that plane; record seeds and the exact room/door state used.
Compute allowable positions at the pixel phases needed to reproduce the
leading-edge samples and avoid falsely bridging a narrow wall or missing a
narrow passage; reduce visited foot positions to 8×8 placements only
**after** exploration. Verify the compiled traversal against representative
doorways, obstacle bases, stair transitions, and the under-bridge passage in
0x060. An unexplored
position, or a position unreachable from the chosen seeds, is `unknown` for
ground classification; it is not automatically a wall or nonexistent floor.

Store support geometry separately from actor traversal and visibility. A
reachable foot position demonstrates occupied space and a supporting surface
at Link's feet, but does not classify every 8×8 pixel behind his sprite as
floor. Use room-object assembly/write history, BG compositing, and actor
priority/occlusion evidence to identify the **support below** Link and the
**artwork above/in front** of him. Under a bridge there can be lower support
and an upper walkable bridge at the same `(x, y)`; both must survive, with
separate contacts/heights and an occluding bridge span. A pillar's collidable
base and nonblocking top likewise have different roles. Walkable floor borders
that Link's center cannot occupy still need support/assembly evidence, not
inference from reachable tile centers or from `00` alone. Keep the present
flat-art and manual review fills separate from this validated traversal map.

Accept 100% coverage **only relative to an enumerated room-state/entrance
scope** and an accounted-for denominator: every ground-support region is
classified with evidence or reported unresolved, every reachable movement
edge is reproduced, and overlapping supports and occluders are represented
independently. Movable pots/blocks, switches, doors, stairs, and sprites can
change the answer; no single default-state capture proves all states. Once
supported floor regions have justified elevations, use their contacts with
authored wall assemblies to constrain wall bases, facings, normals, and
occlusion. Traversal alone never supplies a wall's visual height.

### Room 0x060 doorway/stair graph (initialized capture)

`tools/alttp_collision_reachability.py --state ... --stair-trace ...` starts
from entrance 3's actual Link state: `(x=376,y=472,plane=0)`, collision mode 0.
It uses unit-pixel cardinal transitions and the three leading-edge samples
from `TileDetect_Movement_X/Y`. Plain `00` and axis-aligned `80..8f` door
behaviors are modeled; other behaviors stop graph expansion. Gameplay plane
numbers are deliberately not labeled visual upper/lower.

The adapter's optional route argument plays real reference-engine frames and
exports per-frame positions, room, plane and submodule. `N160,S120` records
both complete stair traversals. Directed graph edges connect the last ordinary
position before each stair handler to its completed destination:

- `(plane 0,376,352)` -> `(plane 1,376,309)` in 62 frames.
- `(plane 1,376,319)` -> `(plane 0,376,359)` in 58 frames.

An edge activates only when its source is reached from the doorway; the
destination is then queued for normal graph expansion. Both edges activate
in 0x060. There are 14,973 plane-0 positions and 11,974 plane-1 positions.
Projecting the reachable 16×16 collision-body span (`x..x+15`, `y+8..y+23`)
onto nonblocking 8×8 attributes covers 437 and 274 cells respectively. This
avoids the one-cell erosion immediately below or right of a collider produced
by the earlier south-center-only projection. The same
`(376,144)` position exists on both planes, preserving the upper bridge and
lower underpass independently. `N300` is a separate reference-engine playback
check through the lower north passage: all ordinary positions occur in the
graph. The room's collision arrays remain unchanged during the stair round
trip. The unit-pixel traversal alone took approximately 45 ms locally in
Python; capture, reporting and artwork rendering cost extra.

Generated review: `build/remaster-geometry/surface-audit-room-local/room-060-doorway-graph.svg`.
Its JSON stores lossless row runs of graph nodes, directed stair edges,
capture state, attribute SHA-256 and playback comparison. Green cells mark
nonblocking cells touched by a reachable collision body; `south_center_cells`
is retained separately as a diagnostic. Neither is a whole-tile visual height
classification. Missing ledge
hops, other door behaviors, dynamic sprites/objects, and Snes9x parity remain
outside this pass. Old v20 `collision-00` and `collision-reachability` outputs
must not be used as current evidence.

Reproduce with the initialized adapter (run the renderer in its engine directory):

```sh
./alttp-room-preview --linked 0x60 ../surface-audit-room-local/room-060-door-input.raw -1 0 0 0
./alttp-room-preview --linked 0x60 ../surface-audit-room-local/room-060-stair-route.raw -1 0 0 0 N160,S120
```

Then from the repository root:

```sh
python3 tools/alttp_collision_reachability.py --room 0x60 \
  --attributes build/remaster-geometry/surface-audit-room-local/room-060-door-input.raw.attr \
  --state build/remaster-geometry/surface-audit-room-local/room-060-door-input.raw.state.json \
  --stair-trace build/remaster-geometry/surface-audit-room-local/room-060-stair-route.raw.route.jsonl \
  --background build/remaster-geometry/room-gallery-room-local/images/room-060.png \
  --output build/remaster-geometry/surface-audit-room-local/room-060-doorway-graph.json
```

The same initialized doorway/body projection is available for the next review
rooms:

- `room-055-doorway-graph.svg`: entrance 50 at `(120,472)`, gameplay plane 0;
  14,013 graph positions and 439 body-covered cells. Its single wet-stair
  transition `(120,433) -> (120,390)` is reference-engine playback evidence.
  The subtype-1 `0x1d` wet stair has its higher landing to the south. Its `[S]`
  label and the reference engine's `upsouth` water-stair table agree: the north
  landing is lower.
- `room-061-doorway-graph.svg`: entrance 4 at `(248,448)`, gameplay plane 0;
  64,114 graph positions and 1,570 body-covered cells. Center and west stair
  traversals are playback-backed and activate from the doorway graph. All
  three room stair assemblies are same-plane (`stair_kind=2`); the east region
  is already included through ordinary connectivity, but its direct stair
  playback route remains unresolved because room sprite interactions disrupt
  the scripted cardinal route.

Both rooms remain on gameplay plane 0 in these captures, so the empty plane-1
panels are expected and do not assign a visual elevation.

## Runtime floor-level reconstruction

The accepted collision-body graph now has a programmatic elevation stage in
`tools/alttp_collision_reachability.py`. Removing the observed stair edges from
the movement graph leaves cardinal floor components. Each oriented in-room stair
adds a signed low-to-high constraint; consistent connected constraints are
normalized to floor-rise units. The JSON review records these under
`elevation.cells_plane_x_y_level`. Unconnected or contradictory components are
not assigned an elevation and retain their authored tile heights.

The live ALTTP adapter applies the same policy from the current room's completed
WRAM collision and stair tables. `S9xRemasterBuildDungeonHeightMap` runs the
Link-body movement predicate, solves relative stair levels, and produces a
session-cached 64x64 placement map. Its cache key includes room, collision
attributes, stair records, collision mode, and configured floor rise, so room
state changes rebuild the map without recomputing it every frame.

Runtime application follows these rules:

- BG2 floor placements add the solved level offset to their authored per-tile
  height; the same tile identity may therefore appear on either floor.
- Stair tread and rail groups keep their authored absolute ramps.
- Authored wall faces keep their absolute transition profiles. Flat wall-top
  caps keep their authored local profiles and receive the solved placement
  offset when they occupy a solved upper cell.
- Visible OAM pieces use the solved cell beneath their assembled OAM slot as a
  support point. This replaces the older OAM-priority-only floor shortcut.
- Unknown cells, rooms without a consistent in-room stair relation, and
  unsupported collision modes keep the baked defaults.

This is intentionally dungeon-only. Overworld elevation still needs an
equivalent source of runtime transition constraints before it can use the same
solver safely.

Current local review files (generated under the ignored `build/` directory):

- `build/remaster-geometry/surface-audit-room-local/room-055-stair-review.svg`
- `build/remaster-geometry/surface-audit-room-local/room-061-stair-review.svg`
- `build/remaster-geometry/surface-audit-room-local/room-060-stair-review.svg`

The SVGs embed their linked-view images and can be opened directly in a browser.
Inspect whether the light- and dark-green *sample cells* are on the intended upper
and lower walking surfaces. For 0x060, inspect the pink cells as a separate
question: the later writer may be a door or overlay that still belongs to the
stair assembly; identifying its support relationship requires actual write
history/handler semantics rather than hash matching.

## Inputs, outputs, and applicability

`tools/alttp_surface_rules.py` accepts a `alttp-surface-scene-v1` JSON scene
with `width`, `height`, **both complete BG maps** in `placements`, and an
explicit `anchors` list. Each cell has `layer` (0 or 1), `x`, `y`, `tile_hash`,
`hflip`, `vflip`, `writer`, and optionally `template_id`. The linked-room
adapter also supplies room ID, tilemap digest, and decoded-art digest. Full
room/state/ROM provenance and reliable support relationships still need
additional extraction; a last value-changing writer is *not* proof of support.

`alttp-surface-rules-v1` has `templates`: each entry contains `id`, `tile_hash`,
and `regions`. A region contains `id`, `role`, 64-bit row-major pixel `mask`
(hex string), `height` (integer 0..255 or 64 such integers), and `contacts`.
All masks in a template must be disjoint. Pixels outside the masks are audited
as unknown. A complete region marked `excluded` is an explicit non-geometric
classification; it cannot claim contacts or receive a height anchor. A contact
is keyed by `north|south|east|west` with `role` (the
neighbor's role), signed integer `rise`, and explicit boolean `optional`.
For each claimed contact, its neighbor must claim the reciprocal contact and
opposite rise. A required contact without a match is an issue; an optional
unmatched contact is a legitimate boundary. Required unmatched contacts are
reported separately as `unmatched_contacts` and invalidate the connected
solution; a physically adjacent claimed neighbor with incompatible contact
rise also fails, even if the edge was marked optional. Isolated surfaces
without anchors remain `relative_only`. Both masks and contacts transform
with the placement's H/V flip. Each matching edge pixel must agree exactly:

```text
offset(neighbor) = offset(current)
                 + local_height(current, edge pixel)
                 - local_height(neighbor, matching pixel)
                 + declared_rise
```

This is a constraint on **modeled surfaces at actual shared edges**, not a
claim that every adjacent pair of visible tile pixels is physically connected.
Templates must be authored from known object structures and contacts. A
`rise` applies at the declared edge, not automatically to every pixel in a
wall face or stair. Multiple semantic usages of one hash require explicit
placement `template_id`; ambiguous uses remain unknown. This avoids silently
giving incompatible wall/floor usages the same local shape.

An anchor identifies one `(layer, x, y, region)` plus an integer height and
nonempty evidence. A stair relation only establishes a difference; a separate
anchor is required for an absolute offset. Dungeon floor height 0 and the
floor-to-floor rise of 50 are user-approved *remaster conventions*. For a
supported north/south stair handler, its high-side direction establishes which
landing is higher regardless of which doorway Link used to enter the room.
The solver requires all 16 8×8 footprint cells to have `stair` regions, plus
two full flat `floor` cells on each landing. Only then does it connect opposing
landing cells with the declared rise and place the full stair assembly relative
to the lower landing; missing cells fail the entire stair relationship. An
anchor on the upper landing at 50 solves the lower landing at 0. Entry position
alone is **never** an anchor or a lower-floor hint; a disconnected raised area
still needs independent evidence. The 4×4 handler footprint assumption and
orientation mapping require handler-by-handler validation against gameplay.
Anchors referring to absent regions
are reported as unconsumed evidence. Disconnected groups remain
`relative_only`, conflicts or broken required contacts are not exported as
solved values, and absent templates/region pixels remain `unknown`. Results
include each region's template, connected-group identity, anchor evidence,
relative and absolute offset where justified, all issues, and the full-scene
status denominator. The `publishable` flag requires all input BG placements
to have complete classified and anchored regions, with no constraint issues,
and an input ROM fingerprint, room-state key, and geometry-evidence identifier.
The current linked-room adapter lacks those state/evidence attestations and
cannot accidentally mark its default-state reference capture publishable.
The solver neither generates normals/emission/occlusion nor changes the game
profile. It does not infer floor semantics from +Z normals or generated zero
heights. Values that cannot fit the current 0..255 height representation are
reported, not clamped. Rule and placement ordering do not affect the canonical
report.

## Scale-up gates (not yet implemented)

1. Instrument actual writes and assembly/support roles in the room loader;
   identify stair footprints, landings, caps/feet, continuation edges, door
   jambs, and overlays from handlers. Export collision as a cross-check. Record
   room-state, graphics-context, and ROM fingerprints, and compare the complete
   constructed maps against Snes9x at matching states.
2. Author reusable handler/assembly templates with complete region masks and
   named contact edges. Generate placed templates from verified handler IDs,
   not a new screenshot-based classifier or per-room rectangle. Explicitly
   review same-hash incompatible usages. Add distinct high/low contacts and
   split regions where one tile-wide base is insufficient. Extend the current
   edge solver to named intra-assembly, cross-layer, high/low, and non-adjacent
   *verified* support constraints; don't use nearest-floor attachment as proof.
3. Resolve an independently reviewed base elevation, then test complete
   structural accounting for 0x055 and 0x061 across camera positions and
   relevant gameplay states. Hold 0x060 out as a generalization fixture. No
   production rule may select those rooms by ID. A defect requires a rule and
   audit-denominator fix covering every occurrence, not a patch for one crop.
4. Derive heights and normals from accepted local surface profiles and solved
   placement bases. Separately author occlusion and stateful emission by
   assembly/material role. Preserve authored fields, track per-layer evidence,
   and add semantic-usage variants to profile/frame/runtime for hashes whose
   metadata differs by context. Account for actors at their feet and for
   hidden/offscreen lighting geometry. No array's existence proves correctness.
5. Enumerate room states and all 320 IDs; report unsupported handlers, states,
   missing anchors, conflicts, and unclassified placements with separate
   denominators. Only after a state passes structural and matching-gameplay
   validation should its compact placement map be considered for live use.

The legacy `tools/alttp_room_geometry.py` proposals remain an independent
review path. Its frequent-neutral-art floor heuristic, median seam vote,
unanchored minimum normalization, and object-wide offsets are **not** accepted
evidence under this contract.
