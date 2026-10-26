# Room001 / settled plane1 — reviewable but incomplete

2026-10-26 follow-up. **Detection/tools/tests/report only. No runtime or profile
repair; all user-authored arrays preserved. Room-wide v2 exits 1.** This supersedes
the earlier report's missing-source diagnosis for BG1 `(31,12),(32,12)` when
discussing the **current** profile. Door assessments remain confirmed, unchanged.

Artifacts: `build/room001-source-seam-20261026/`. Important outputs:
`capture-pins.json`, `capture-summary.json`, `matching-capture-inputs.json`,
`current-terminal.jsonl` (8192 placements), `exposed006.json`,
`high-wall-anchor-path.json`, `corpus-provenance.json`, `fixture-summary.json`,
`validation-v2.json/.svg`, `height-field-006/007/008.png`,
`capture006/007/008-original.png`, and `replay-comparison.json`.

## Exact evidence and author's edits

ROM USA SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Current profile copied before analysis: SHA256
`032d17aed98f024adace48854323f574d9541b7fd96a9e8c4d6cc22fd31f54fb`,
serialized FNV `6b0c8c2709253e8d`, rise50. Its bytes **exactly match** the
own-profile sidecars of fresh captures006/007/008, not003/004/005. End-of-work
profile hash still equals the copy (`profile-preservation.json`).

Fresh Room001006: module7/submodule0, previous room002, Link802,120 (local290,120),
gameplay plane1, scroll682,12 (local170,12), same graphics base16384 on both BGs,
complete maps, collision mode0, no stairs. Entrance history is not attested;
reference construction uses borrowed entrance123/theme19/main7/sub0. Both final
maps match the existing provenance-enabled reference **8192/8192 words**. The
live complete WRAM/PPU comparison has zero mismatches.006/007/008 have identical
8192 words and8192 attributes. VRAM bytes differ442/455 in007/008, so they were
**not** treated as identical graphics snapshots: each selected raster hash,
canonical pixel, flip and camera is independently checked by the detector.

006=all layers;007=BG1 isolation;008=BG2 isolation, independently identifiable by
packet source distributions. Same Link/camera/profile. Live sidecars were pinned
individually. Production own-input006 replay validates139362 BG samples and
reproduces all139362 exact captured source/base/local records with zero changes.
This is offline replay of fresh user gameplay evidence, **not** a new capture
from a repaired implementation.

## 1. Purple center: TWO concrete scene-admission failures

**The user's two authored heights are present AND active.** Both are explicit
64-sample zero arrays in the current profile, resolve as Floor, and006's real
subscreen uses them with `hasHeight=true`, status Inherit, base0.007 uses the
same geometry successfully. No facing/flip/context source overrides erase them.
The previous capture's missing arrays are not current evidence.

The actual Height handler is `macosx/shaders.metal`, `uniforms.view==5`:
purple `(0.22,0,0.28)` means **lighting height-field validity G=0**, not "the
selected asset lacks a height key". `mac-render.mm` selects scene geometry when
lighting and the complete independent scene arrays are available. Its inspector
helpers instead use real main/sub visible ownership (`frame.h`'s
`S9xRemasterFrameVisibleTilePixelAt`), explaining why a pick can show authored
BG1 while the Height field at that screen position is absent.

For006, the exact CPU field producer reproduces **1664 purple pixels /57344**:

|Cause|Pixels|Placed geometry|
|---|---:|---|
|`S9xRemasterRestoreSceneSubscreenWalls` requires **height AND normals**|128|BG1 `(31,12),(32,12)`; heights exist, normals absent|
|Same function's physical-role admission omits **Obstacle**|1536|24 BG1 statue cells; height AND normals present, roleObstacle|

No opaque or transparent BG2 tile was selected for those1664 pixels. Both raw
main and scene are **backdrop/id0**, while sub contains actual opaque BG1. All
1664 sub sources have active effective heights. The scene restoration predicate
rejects them before placement/application. Example screen `(82,84)` -> room
pixel `(252,96)` -> BG1 `(31,12)`, canonical4: ink9, height0, normalsfalse,
palette artworktrue, main/scene backdrop. `(86,84)` similarly resolves BG1
`(32,12)` canonical0, ink10, height0, normalsfalse.

Why scene misses real color: `gfx.cpp`'s independent scene pass is main-screen
composition with BG3 removed in geometry-only mode, or the BG2/OBJ union in
cone-free mode. It does not automatically reproduce all physical BG1 color-math
contributors. This room's stock main is backdrop in the center and stock sub
provides the visible BG1 art (confirmed in captured stock RGB). Restoration is
the intended bridge; its normals and role prerequisites are the exact failing
stage. `lighting_field_cache.h` deliberately has **no sub fallback when
useScene=true**; it sees an invalid scene receiver. No cache staleness is needed
to reproduce the symptom; a forced full field recomputation produces it.

007 has **57344 known, zero purple**: its isolated fresh main raster carries
BG1 directly and disables the independent scene.008 is **not all purple over
the entire viewport**:28672 known /28672 purple. The central transparent BG2
region is legitimately backdrop in isolation; top/bottom BG2 walls do have
height. This is not permission to manufacture BG2 geometry underneath BG1.

Historical own-profile003/004/005 still have104/104/128 missing throne-source
pixels respectively and768/768/1176 excluded obstacle pixels. Their camera
clipping differs. They are separately labeled historical and never used to
deny the current authored arrays.

**Required general runtime capability (not implemented):** prove physical
subscreen receivers through effective semantic/assembly ownership independently
of whether normals have been authored; include proven physical obstacle
receivers without admitting lamp/color-math masks. Keep actual BG identities,
source pixels and independent planes. Fix scene admission/projection, not the
user's already valid heights or transparent BG2.

## 2. Cross-BG wall height gap: reproduced, signed, ownership-backed

This is a **different** defect. Complete maps show BG1 LOW three-band walls
joining surviving BG2 HIGH perimeter strips. Exact ROM dispatches and final
writers prove the complementary sections; screenshot resemblance is not proof.
The new check admits only whole surviving three-band cross sections with exact
ROM words/flips and writer ownership, then requires actual opaque exposed
captured pixels on both sides. Overwritten trim is irrelevant; overwritten
face members reject that cross section. It does not pick nearest floors or
join differing plane supports merely because their coordinates coincide.

North concrete join at room cell x40:

|BG/cell|Final writer|Effective base/local/span|
|---|---|---|
|BG1 `(40,12)`|32, subtype0 handler03 LOW north|base0/local0..7 ->0..7|
|BG1 `(40,11)`|32|base8/local0..7 ->8..15|
|BG1 `(40,10)`|32|base16/local0..7 ->16..23|
|BG2 `(40,9)`|25, subtype0 handler01 HIGH north|base39/local0..7 ->39..46|
|BG2 `(40,8)`|25|base47/local0..7 ->47..54|
|BG2 `(40,7)`|25|base55/local0..7 ->55..62|

BG2 `(40,10)` was overwritten by writer59 (phase3/ordinal2); it is transparent.
The true exposure is therefore BG2 row9 -> BG1 row10. BG1 `(40,9)` is hidden
floor-pattern cap at24, not a fourth visible wall band.

Screen006 `(150,67)` corresponds to north edge room pixel `(320,79)`:
BG2 bottom39 -> next displayed pixel BG1 top23. Actual signed step **-16**;
the authored wall slope requires **-1**. Residual **-15**. Conditional on the
already solved LOW chain, HIGH bottom should be24 (and its tile span24..31),
not39. This is an edge relationship, **not a proposed global -15 repair**.
South is V-transformed: BG1 row23 top edge23 -> BG2 row24 edge39,
actual+16 versus slope+1, residual+15. Local ramps must survive unchanged.

006 detects **384 failing exposed pixel seams across50 tile-edge pairs**:
north128 pixels/17pairs, south256pixels/33pairs. Every sampled contract fails;
none is flattened or guessed. Whole maps contain588 proved cardinal cells and
76 candidate cross-BG edges;26 lack observed exposure in006 and remain
**unobserved**, not passes. Side cardinal pairs use the same H/V-aware rule;
corners, doors, diagonal/partial sections are outside this narrow contract.

### Why the wrong base exists, rather than merely where it occurs

Production classifies both sections, keeps local0..7 ramps and correctly
transports their current geometry into the scene. The failure is **constraint /
contextual anchor ownership**, not absent metadata or raw-main debug heights.
There is no direct equality for this differently layered HIGH/LOW exposed join.
`frame.h`'s transported BG1 wall relations require paired same-word ownership
and a specific complementary contact; this state exports no such relations.
The straight BG2 HIGH face instead acquires its base through its own structural
graph, including accepted edges to authored exterior-pattern geometry.

Read-only `ALTTP_TEST_WALL_COMPONENT_AUDIT` reconstructs the actual adjacency
path: BG2 `(40,9)` base39 follows the north strip to`(59,9)`39,
`(60,9)`47,`(61,9)`55, then side`(61,14)`55; its shared structural node reaches
plane1 `(61,15)`55 -> `(62,15)` base0/local60 -> exterior support component0.
The `(61,15)` transformed face has high edge60 adjoining the exterior flat's
authored60. This internally consistent **55-unit side-face / exterior60
anchor** produces bottom39; it is not the24-high BG1 landing relationship.
See `high-wall-anchor-path.json` for exact nodes, signed edges and plane tags.

The existing one-pixel boundary-step convention extends each authored ramp to
the next pixel (`boundaryStep`); LOW faces solve0/8/16 and high cap24. Neither
configured floor rise50 nor "one tile is8" explains the extra15. Current
exterior tile `c40518b112d0814e` is explicitly authored60, side curved profile
0..5, and HIGH ramps0..7. Those arrays are evidence of existing conventions,
not authorization to alter them. A correct shared repair must resolve the
competing contextual anchoring/role of exterior geometry and the proved
cross-BG connection, preserve authored relief, and report contradictory anchors
rather than silently shifting all BG2 walls. No independent absolute wall datum
or complete southern floor relationship is established here.

## Tooling and regression scope

- `alttp_capture_debug_sources.cpp`: read-only exact CPU field producer plus
  per-buffer owner/identity/opacity/effective height/normals/role. Directly
  distinguishes missing assets, admission failure, and invalid final fields.
- `alttp_exposed_height_detection.py`: ROM-pinned complete-map/writer section
  proof + actual captured exposure join; outputs signed pixel residuals and
  explicit unobserved candidate edges. No profile/runtime/map writes.
- `test_alttp_exposed_height_detection.py`: active heights without normals,
  excluded obstacle role, invalid/opaque/transparent negative cases, signed
  N/S/H seams, same/ambiguous writer rejection, transformed exact words,
  priority-only writes and partial overwrite rejection.
- `scene_source_detection_test.cpp`: exercises real scene-restoration and field
  code, proving current failures and valid/opaque-main rejection controls. It
  deliberately pins detection of the failing behavior, not a runtime repair.

Corpus actually checked: all39 available provenance snapshots under`build/`
(including this work's wrapper), four room IDs001/055/060/061,319488 placed BG
cells; snapshots include duplicate cameras/routes and are **not39 independent
states**. Three055 snapshots:92 candidate edges each; thirteen060:0;
twelve061:26; eleven001:76. Every provenance input validates; unsupported
ownership/exposure remains explicit. Separate fresh current-profile terminal
checks cover complete055/060/061 fixtures24576cells: proved cardinal cells
1530/249/813, candidate edges92/0/26. No matching raster exposure supplied in
that fixture-only pass:118 edges remain unobserved, **zero geometry passes
claimed**. Only Room001003..008 have been fully joined to their actual gameplay
rasters in this follow-up. Other316 room IDs and alternate game states are not
covered. General corpus inference is not asserted from these four fixtures.

## Gate, limits and next action

Complete matching ALTPPD1/ALTPAT1, observed006 local seed290,120/plane1,
complete movement graph and current profile replay were passed to
`alttp_validate_room_heights.py` with verified ROM. Coordinates are converted
from global Link802 to room-local290; an initial mismatched-seed attempt was
rejected, not accepted as coverage. Final **v2 exits1**:

- supports368/368, per-plane[0,368], no missing supports;
- immediate fallbacks6/6; BG2 solved walls444, unknown480;
- zero runtime structural conflicts; no compiled placement map supplied;
- existing directional audit8 effective seam conflicts,3112 missing-base edges,
  542 unresolved-ownership edges. Its same-layer scope does not replace the new
 384 cross-BG pixel failures;
- zero activated-plane overlap in this observed graph. Both geometry arrays
  remain independent; no support planes merged. Unobserved plane0 and the
  separate southern chamber remain unapproved.

Presence inventory: all8192 terminal placements now have an effective source;
that does **not** imply8192 correct geometries or runtime-field visibility.
Raw BG2 transparent748 cells and opaque unclassified/exterior1336 cells remain
explicitly separate from solved required geometry as in the complete prior
ledger. Existing doorway failures are not repaired by this investigation.

31 detector/join/oracle tests and26 validator tests pass; the standalone real
scene-source regression passes; `git diff --check` passes. Runtime untouched,
so no app rebuild required. Neutral Height PNGs are CPU reconstructions of
matching captured fields, not Metal screenshots or repaired gameplay review.

Smallest next authorized change: shared scene receiver admission with explicit
physical ownership, followed by a fresh all/BG1/BG2 capture; independently add
proved cross-BG HIGH/LOW seam constraints and correct exterior-anchor contextual
ownership without changing authored arrays. Rerun whole-room v2 and compare
matching gameplay before any approval. **No handback as successful, no runtime
or authored edits performed.**
