# Reconstructing ALTTP elevation from game data

Status: object-provenance solver with opt-in live review maps for rooms
`0x055` and `0x061`, 2026-09-26. A single RAM flag, tile hash, BG layer, or
collision byte cannot identify every raised background pixel. The generated
maps are review artifacts. Keep the current hand-checked runtime room masks
for normal profiles until a generated placement map passes the gates below.

## Implemented first slice

`tools/alttp_room_geometry.py` reads a complete linked room export from the
reference renderer. It decodes both 64×64 BG tilemaps and canonical tile
hashes, retains each placed tile's H/V flips, and extracts separately facing
pixel regions from authored wall normals. A corner tile can therefore produce
two transformed wall-face regions. It proposes flat art from the profile's
wall-top group plus repeated zero-height, +Z-facing art; the latter is a
candidate rule, not proof that a tile is walkable. Connected flat cells form
components. ROM stair object handlers identify the higher side; the displayed
tread height order must agree before the connection is accepted. Each connected stair
graph gets relative heights using `upper_floor_height`. Unanchored components
and contradictory stair loops remain explicitly unresolved. The program
writes `geometry.json` and a 512×512 `geometry.png` review map. The optional
`--geometry-atlas` argument on `tools/alttp_make_room_inspection.py` produces
those two files alongside a normal linked capture.

The renderer now traces each object and door dispatch, compares both full BG
tilemaps before and after each dispatch, and writes `ALTPRV1` provenance beside
the raw room export. It records the final writer of each 8×8 BG placement and
checks that the exported tilemaps match the linked PPU maps exactly. The
geometry command requires this sidecar so the weaker artwork-only flood fill
cannot silently produce a misleading room map. The
geometry solver uses writer boundaries to avoid joining wall-top artwork
across distinct room objects. It also compares the authored per-pixel heights
along each wall/floor tile seam after the placement flips. A proposed object
offset must make a seam meet a solved floor within four height units. Contacts
that do not meet either candidate offset are reported, so a face that spans a
level change can still be anchored by its lower wall top. Door thresholds may
use the nearest solved flat art up to three cells away when no direct seam is
available. The `attachment_proposals` records include the writer, role,
neighbor levels, seam votes, unmatched seam count, and a status. A separate
`height-proposals.png` colors proposed BG2 placement offsets; its lighter
colors are wall/door suggestions and darker colors are flat-floor estimates.
The inspection producer also writes a **proposed** `.s9xrmf` variant using a
compact `ALTPHM1` 64×64 placement-offset map. The converter adds that offset
to a selected BG2 tile instance while retaining its authored per-pixel heights,
normals, occlusion, and emission. It leaves unknown placements at their base
height. The ordinary capture continues to use the existing runtime room mask;
the proposed variant is a review artifact. A separate opt-in profile sidecar
can now apply the same BG2 map during live play; it does not rewrite tile
metadata in the TOML.

With the current candidate profile and ROM-matched atlas, room `0x055` has one
usable stair relation: 170 flat placements receive a relative height; 12 wall
cells contain two face directions. Room `0x061` has three consistent stair
relations and 1,380 flat placements receive a relative height; its known
checkered upper floor resolves to 50 and lower red floor to 0. Provenance
prevents the room `0x055` proposal from spreading through the right-side wall
top art, but the relative landing assignment still needs validation against a
matching gameplay capture before replacing its runtime mask.

The solver writes `tile-semantics-template.json` with the wall-face pixel masks
and orientations inferred from authored normals. A reviewer can edit that
file and pass it as `--semantics` (`--geometry-semantics` in the inspection
producer). Each entry is canonical to a tile hash; the placed H/V flip
transforms its face mask and direction. A corner can retain two independent
face regions. The file rejects overlapping masks and invalid directions.
These edits change offline face classification; they do not rewrite the
profile's lighting normals.

This prototype does **not** yet validate wall offsets against gameplay, export collision
attributes, or classify every floor texture. Its geometry review PNG uses blue
for relative 0, green for raised flat art, gold for unanchored flat art, orange
for stairs, and separate colors for the four wall facings.

## Reproduce and inspect one proposed room

### Check the proposal during live play

Open the test build at `build/Snes9x-GeometryReview-v1.app`, start the verified
ALTTP ROM, and load `build/remaster-geometry/alttp-profile-wall-review.toml`
through **View > Load Remaster Profile...**. The sibling directory
`alttp-profile-wall-review.toml.geometry-review/` contains `room-055.bin` and
`room-061.bin`. On profile load, Snes9x validates the two `ALTPHM1` maps and
uses their BG2 placement offsets during live play in those rooms. Other rooms
keep the existing runtime behavior. The Scene Controls title reports
"Geometry Review (2 rooms)" when these maps are loaded. If the directory is absent, the app uses
the existing hand-checked room masks. A malformed map makes profile loading
fail with a named error, rather than silently using stale geometry. Renaming
the sidecar directory and reloading the profile disables this test path.

Use the Height debug view in the same room and camera position as the gameplay
capture. Set Height range to 0–50 to inspect the steps; use 50–100 for the
upper landing and its walls. `height-proposals.png` visualizes **placement
offsets**, so its dark stair cells do not mean the stairs have no authored
height. The review profile's four stair-tread hashes have heights 1–39,
and the four rail hashes have heights 2–48. The original checked-in
`alttp-profile.toml` has no height or normal arrays for those eight hashes;
load the review profile for this comparison. Stair metadata stays on the
tile itself, and the live review map deliberately does not add a room offset
to stair treads or rails. This is a test build: the room maps have not passed
matching gameplay validation and do not change the general profile TOML.

### Generate the review map and synthetic capture

The local ROM, reference-engine build, atlas, candidate profile, and generated
captures live under ignored `build/remaster-geometry/`. They are not part of
this repository. Build the renderer from a verified USA ROM and a local
`snesrev/zelda3` checkout if the provenance-enabled renderer is absent. The
builder requires SDL2 and a Python environment with Pillow and PyYAML for
asset extraction. It refuses to overwrite an existing output directory.

```sh
python3 tools/build_alttp_room_renderer.py \
  --source /path/to/zelda3 --rom /path/to/verified-zelda3.sfc \
  --output build/remaster-geometry/room-preview-engine-v15
c++ -std=c++17 -O2 tools/alttp_linked_room_capture.cpp \
  -o build/remaster-geometry/alttp-linked-room-capture-v2
```

With the local candidate profile and atlas already built, capture the
southwest viewport of `0x055` and its proposed offsets:

```sh
python3 tools/alttp_make_room_inspection.py \
  --renderer build/remaster-geometry/room-preview-engine-v15/alttp-room-preview \
  --converter build/remaster-geometry/alttp-linked-room-capture-v2 \
  --profile build/remaster-geometry/alttp-profile-wall-review.toml \
  --geometry-atlas build/remaster-geometry/alttp-atlas.sqlite \
  --room 0x55 --view-x 0 --view-y 288 --advance-frames 0 \
  --output build/remaster-geometry/room-055-inspection-proposed
```

The producer writes the ordinary and `-proposed.s9xrmf` captures, `manifest.json`,
`geometry.json`, `geometry.png`, `height-proposals.png`, the compact
`height-proposals.bin`, and `tile-semantics-template.json`. The ordinary
capture retains the existing room-specific offset logic; the proposed capture
uses the generated BG2 placement map. Open either through **View > Open
Remaster Frame...** in Snes9x and select the Height debug view. Scene Controls
can set the Height range min/max to focus on values 50–100; opening a capture
initializes the range from visible effective heights. The PNGs are maps for
review, not substitutes for inspecting the linked capture. In
`height-proposals.png`, dark blue and green mean solved flat placements at
relative 0 and above 0; pale blue and green are attached object suggestions;
dark gray means no proposal.

The report's `floor_height_rows[y][x]` and
`suggested_bg2_offset_rows[y][x]` are 64×64 BG2 placement maps. `null` means
unresolved. The latter includes supported wall/door proposals;
`attachment_proposals` identifies each source writer, object, neighboring
floor heights, seam votes, unmatched seams, status, and proposed offset.
`provenance.bg2_writer_rows` gives the final writer of every BG2 cell, and
`provenance.objects` describes those writers. A `seam_supported` proposal
matched a neighboring solved floor within four authored height units;
`nearest_door_threshold` is a weaker spatial suggestion. `transition`,
`conflicting_seam_offsets`, `unmatched_floor_contact`, and
`no_solved_neighbor` carry no placement offset. The binary `ALTPHM1` map
stores the same 64×64 offsets as bytes; 255 means unknown and is ignored by
the converter. The renderer's temporary `ALTPPD1` raw export and `ALTPRV1`
last-writer sidecar are checked for matching final BG maps before solving and
removed after the producer finishes.

For wall orientation review, edit `tile-semantics-template.json`, keeping its
`alttp-tile-surface-semantics-v1` format. Under `faces`, each tile hash has one
or more `{ "facing": "north|south|east|west", "mask": "0x..." }` entries.
The 64-bit mask selects pixels in canonical 8×8 row-major order, with bit
`y*8+x` for pixel `(x,y)`; corner regions must not overlap. Re-run the
producer with `--geometry-semantics /path/to/edited-semantics.json`. BG H/V
flips mirror both regions and facings. The file only corrects semantic
classification for this offline solver.

Use `--all-viewports` for six overlapping viewports and the default
`--advance-frames 0,8,16,24` for sampled animation. The geometry report
belongs to the first variant. A later variant whose complete BG tilemaps
change is marked `geometry_state_changed` in `manifest.json` and receives no
proposed capture; rerun it as its own state rather than reusing the earlier
offset map. The renderer does not enumerate switch, chest, or water states,
and the linked synthetic captures do not carry sprite/HUD tile metadata.

### Room 0x061 upper wall correction

The first live review found black, front-facing decorations across the top
wall. The base wall object had received a 50-unit offset, but two column
objects and the crest/banner objects painted over it. Their tile hashes had
zero generated height and front-facing defaults, and the solver did not
classify their room-object handlers as wall assemblies.

The solver now recognizes subtype-0 objects `0x05` and `0x3a`, and subtype-1
object `0x1e`, as wall overlays. Each assembly inherits one solved neighboring
floor level; mixed or missing levels remain unresolved. In room `0x061`, all
four upper-wall overlay writers resolve to 50, including the crest whose
floor is two 8×8 cells away. The new review profile gives 15 previously
neutral decoration hashes a local vertical height profile and wall-facing
normals. The prior candidate remains available unchanged.

In the linked proposed capture's visible upper-wall strip, 2,528 overlay
pixels changed from height 0 with +Z default normals to effective heights
51–104 with cap and +Y/relief normals. Across the 5,114 linked BG2 pixels in
the horizontal wall strip at screen x=96..255, y=32..63, none has effective
height below 50. This checks the exported packet and selected viewport; it
does not establish that every room context or gameplay state is correct.

## What the game actually knows

The game has a *current actor collision plane*. Link's
`link_is_on_lower_level` (`$7E:00EE`) and sprite floor fields affect attacks,
contacts, and which collision attribute plane is queried. Staircase handlers
can change that state. It answers which gameplay layer Link occupies at his
feet; it does not label the visual elevation of every floor or obstacle he
can reach or collide with. A wall can stop Link while its face spans multiple
visual heights. Two visually separated surfaces can also share a gameplay
plane when ordinary 2D barriers keep their actors apart.
The room loader constructs BG1/BG2 tilemaps and collision attributes from a
floor pattern, a default layout, ordered room objects, doors, and current
room state. These are 2D gameplay and rendering structures. There is no
per-pixel z buffer or ready-made upper-platform mask in the ROM or WRAM.

Primary implementation references in the local ROM-matched reference engine:
`src/dungeon.c` `Dungeon_LoadAttributeTable`,
`Dungeon_LoadBasicAttribute_full`, `Dungeon_DetectStaircase`, and
`Module07_08/10_*IntraRoomStairs`; `src/player.c`
`CheckIfRoomNeedsDoubleLayerCheck`, `PushBlock_GetTargetTileFlag`, and the
movement collision checks. The current Snes9x adapter in `gfx.cpp` reads
room ID, BG2 scroll, collision mode, and Link facing, while
`remaster/frame.h` applies hand-drawn room masks.

### Counterexample: room 0x055

The reference engine was started through its own entrance 50 and stopped
after room load. At 8×8 tile centers, the southwest landing `(120,400)`
and the east floor `(220,400)` both have BG2 collision attribute `00`.
The northern walkway `(220,352)` is also `00`. The matching BG1
attributes are `00` too. The room header's collision mode and BG2 properties
are both zero. Link's initial floor flag is zero. Thus neither the room
header, actor flag at entry, nor one collision byte provides the visible
height boundary. The BG2 art words differ, which is useful evidence but does
not by itself prove their visual height.

An audit found that the earlier `tools/alttp_floor_rooms.py` stair IDs mixed
the reference engine's object subtypes: room `0x055`'s two subtype-2 `0x20`
objects are lit torches, not between-room stairs. Its subtype-1 `0x1d` object
*is* an in-room wet stair, at object coordinate `(14,52)`. The scanner now
uses the actual stair handler IDs. It keeps subtype-0 `0x33`, `0x34`, `0x70`,
and `0x71` as a descriptive carpet/floor-trim count; they are not platform
evidence. The inventory remains a review queue, not a height map.

## Replacement: room-state placement provenance

The unit of classification is a **placed 8×8 BG cell in a particular room
state**, not a tile hash. Store `(ROM hash, room ID, state key, BG layer,
map x, map y)` and the height offset to add to that cell's authored pixel
heights. A tile hash may appear on either level without being duplicated.

1. **Reproduce the game's room construction.** Use the verified ROM and the
   reference engine to load each room through a valid graphics context. Export
   both final 64×64 BG maps, collision attributes, doors, staircase records,
   and the state variables that changed them. Record the last room object or
   state update that wrote each cell. Instrument object dispatch and ordered
   writes; do not infer ownership from final artwork alone. Compare generated
   maps with Snes9x WRAM/PPU for matching room states before using them.
2. **Reconstruct visual surfaces.** Classify floor patterns, stair endpoints,
   pits, ledges, rails, door thresholds, wall tops, and wall faces from the
   object handlers that constructed them and the final neighboring artwork.
   Connect floor surfaces where geometry and traversability support it; mark
   the visual rise across a stair or ledge. The actor plane, collision map,
   and actual stair transitions constrain this model where they carry useful
   information. They do not define visual height on their own. In particular,
   a reachable or collidable region cannot automatically inherit Link's
   height. This yields `lower`, `upper`, `transition`, or `unknown` for each
   placement, with the evidence used to reach that classification.
3. **Attach nearby structures.** A door, fence, wall top, or wall face takes
   its height constraints from the surface/object it belongs to. Record its
   object ID and rule in the output so a reviewer can see *why* it was raised.
   Where a structure spans two levels, represent both parts or a pixel-level
   boundary rather than forcing one offset on the whole tile.
4. **Use the map at runtime.** Index the result by room ID, state key, BG
   layer, and room-local map position derived from actual PPU coordinates.
   Add `upper_floor_height` (currently 50) to the placement before applying
   authored per-pixel height. Grounded Link and sprites take their visual base
   from the reconstructed surface at their feet; their live collision plane
   is a cross-check and helps distinguish overlapping surfaces. Handle stair,
   jump, and airborne states explicitly. Unknown cells retain their authored
   base and are reported; they must not silently receive a guessed offset.

The reference engine and Snes9x are useful for different reasons: the former
exposes named room-object handlers and game state; the latter verifies the
actual ROM execution, rendered tile identity, and camera mapping. The final
runtime can use a compact precomputed placement table, not run the reference
engine every frame.

### Propagating height along walls

A continuous wall is a strong structural clue even when its adjacent floor is
missing or the actor collision plane says nothing useful. Use an upper landing
or stair endpoint as an **anchor**, then follow the wall through its connected
object pieces. For a wall face with a meaningful projected XY normal, its
local tangent is perpendicular to that normal: a wall facing north/south
continues left/right, while a wall facing east/west continues up/down in the
map. The object's layout and visible edge connections decide which neighboring
piece is actually part of the same wall. A corner joins two face directions;
it does not necessarily end the wall.

The rule propagates a **height relationship**, not a single flat value:
matching pixels across a seam should have compatible absolute heights after
the placement offsets are applied. A wall cap may be at the upper floor's
height while its visible face descends toward the lower floor. Do not simply
add 50 to every pixel of such a face. Preserve each tile's authored per-pixel
height profile and solve only the extra placement height needed to join the
profiles. If one 8×8 placement contains both sides of a transition, a single
offset cannot represent it; split it by pixel region or mark it unresolved.

Normals are supporting evidence, not the wall classifier. Some authored or
generated normals are front-facing defaults, some encode surface roughness,
and a wall tile reused in another orientation can have a lighting-specific
normal rule. Prefer the ROM object handler, room placement, neighboring tile
edges, and authored height profile to identify a continuous wall. Use the
normal to verify its facing and choose the tangent only when the normal is
meaningful. Stop propagation at a door, stair, ledge, occlusion boundary, or
join that has multiple plausible continuations. If two anchors imply
incompatible heights, report a conflict and show both in the reviewer.

For room `0x055`, the first proof is that one upper-floor anchor follows the
bordering wall around its corners, preserving its face gradient, while the
lower east floor remains at its own base. This must work from object/tile
relationships rather than a room-specific rectangle.

### Room-load geometry solver

The proposed input is a room's *placed* tiles after its floor pattern, default
layout, objects, doors, and state changes have finished drawing. Each placement
needs semantic surface regions (`floor`, `wall_face`, `wall_top`, `stair`,
`rail`, `door`, or `other`), a local height profile or height span, and, for
each wall-face region, its facing and which side borders the higher surface.
A corner tile can contain two differently facing wall regions plus a cap, so
one orientation or role for the whole 8×8 tile is insufficient. A wall
assembly can span several 8×8 tiles; its total rise is an assembly property,
not necessarily the maximum value of one tile's height array. Collision
attributes tell us
which floor connections are passable or blocked but do not supply a visual
height by themselves.

At room load, the solver should:

1. Find connected walkable floor surfaces. Treat a known low floor as height
   zero, and use stair endpoints, ledges, and wall top/foot relationships to
   add relative height constraints between surfaces. A staircase with a known
   rise can anchor the upper landing even when both surfaces have the same
   collision attribute.
2. Attach each wall top to its high-side floor, and its foot to the low-side
   floor if one is visible. Propagate these relationships along compatible
   wall segments and around recognized corners. Match authored height profiles
   across seams; never flatten a wall face to the height of its top.
3. Attach doors, fences, and other fixtures to the floor or wall assembly that
   owns them. A door spanning a wall opening may require its own height profile.
   Place grounded actors from the solved surface at their feet, using their
   combat plane as an additional constraint where surfaces overlap.
4. Solve the resulting equalities and known rises. If a connected loop
   requires conflicting heights, or an isolated wall has no anchor, output an
   explicit unknown/conflict record with the implicated placements. Cache the
   solved placement offsets by room and state; update only affected regions
   when a switch, water level, or door changes the constructed map.

Use the wall tile's **canonical authored surface regions and facings** as the
default. A straight wall may have one face region; a corner may have two
face masks with separate orientations and seam connections, and possibly a
top mask. The BG tilemap records horizontal and vertical flips for each
placement: mirror every region mask and negate its X-facing component for a
horizontal flip or Y-facing component for a vertical flip. This gives the
displayed instance's regions and facings wherever the placement transform
explains them. The BG tile word does not encode a 90-degree rotation;
rotated artwork uses another tile/template or must have an explicit transform
in the object builder.

At a corner, propagate height independently along each face's tangent, then
require the two faces and any cap to agree at their shared edge. Their normals
can differ while their joining height remains continuous. Store the resulting
placement height per visible pixel or face region when one tile-wide offset
would erase the corner's height transition.

Keep object/placement overrides only for exceptions: the same tile can appear
on opposing wall faces with the same flip bits, as the existing profile's
`direct_lighting_opposite_facing` option already acknowledges. In that case
the wall object's neighboring geometry or a local annotation selects the
facing. The normal map remains lighting data; it can suggest the canonical
facing but cannot always serve as its unambiguous semantic label.

The checked-in profile currently labels one hash in `dungeon_floor`, four stair
treads, eleven wall-face hashes, and seven wall-top hashes through material
groups. The single `dungeon_floor` hash is authored as out-of-bounds geometry
at height 50, not as the playable floor, so even that label cannot be used as
a general floor detector. The generated candidate adds pixel metadata to many
more hashes but does not comprehensively classify their geometry. Before
asking for manual orientation of every wall tile, extract candidate face
regions and canonical facings from authored normals where trustworthy,
apply each placement's flips, then use object write provenance and
neighboring geometry to resolve exceptions. Present only unresolved tile
groups, templates, or placements in the room reviewer for manual region
masks, facing, and high-side labeling. A tile annotation should cover every
transformed occurrence; an object or room annotation handles genuine reuse
exceptions.

Keep these semantic annotations in the offline atlas/tooling database; compile
the resolved per-placement heights into a compact room/state table for Snes9x.
Do not expand the live lighting profile with a copy of every wall relationship
or solve the graph every frame.

## Coverage and validation gates

- Generate an explicit state list for all 320 dungeon room IDs. Switches,
  opened doors, water levels, movable blocks, and other state changes produce
  additional room-state keys. Report unmodeled states. The existing room
  gallery covers one default state and 214 rooms borrow a graphics context;
  it is not complete validation of every room state.
- Compare final BG1/BG2 words and displayed tile identities against
  matching Snes9x frames. A state fails if its tilemap differs unexpectedly.
- Compare generated placement offsets against the known `0x055`, `0x060`,
  and `0x061` captures, including upper floors, lower floors, stairs, doors,
  rails, and walls. Require agreement across camera positions, not just one
  screenshot per room.
- Report counts of classified, transition, ambiguous, and unobserved cells by
  room and state. The review UI should open a synthetic scene capture with
  an elevation overlay and provenance on click. Human effort then targets
  the **unknown/conflicting cases**, not all rooms in normal play.
- Only replace a hand-checked mask when the generated map passes its fixtures.
  After that, run the same checks over all rooms and retain a regression set
  of gameplay captures.

## Separate overworld model

The Light and Dark Worlds are area/quadrant maps with stateful overlays, not
the dungeon room-object format. They need their own placement provenance and
surface graph. Ordinary overworld ledges can use discrete height relations;
long ladders and Death Mountain camera behavior need a continuous path or
camera-relative elevation rule. A dungeon's two actor collision planes cannot
serve as the overworld height model.

## Practical next slice

Export both collision attribute arrays and each room object's assembly role
alongside the existing last-writer provenance. Compare the proposed levels
and seam-supported wall/door offsets in `0x055` and `0x061` with matching
Snes9x captures, including Link's floor state on both sides of their stairs.
In particular, the `0x055` landing assignment is still unverified; do not
replace its runtime mask from the current proposal. Extend the existing seam
matching across neighboring wall faces and corners and connect the result to
both high and low floor anchors. A face that visibly spans both levels must
retain its gradient; ambiguous walls, doors, and rails stay unknown. Use the
editable canonical face masks for exceptions to normal-based orientation and
add object or placement overrides only where reuse defeats the canonical
rule. Test `0x060` next, then compile a compact room/state map for runtime
use. This is the gate before scaling the generator to 320 rooms.
