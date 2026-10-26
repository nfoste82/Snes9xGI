# Room051 capture092 effective geometry

Room `0x051`, own `frame_room051_092.s9xrmf`, Module7/submodule0:
**reviewable but incomplete**. Production repairs restore the omitted physical
scene receivers and attach the two surviving south jamb columns. Complete-room
validation v2 remains **exit1**; no successful-room/capture approval is claimed.

## Pins and preserved work

Inputs are the capture's own companions under
`~/Library/Application Support/Snes9x/Remaster/frame_room051_092.s9xrmf*`.
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
packet SHA256 `7ac180f42bb43925c267760b408cb83db0e965fb7b699fa18b76767dd0e5ebd2`;
own profile SHA256 `e43b6f7c1deca9ad4bb74477b996e78598b6e15cb1228647b7f198aaf28882d5`,
serialized FNV `ff73f254e1858b79`; complete input SHA256
`2e71d4ab2d2c9c623bb9c15152dbc05d9f5cfe6d9e86d07615e18ac4e460a08b`.
Link `(840,2991)`, active plane0, scroll `(720,2833)`, graphics bases both16384,
configured rise50, previous room061. Complete WRAM maps equal PPU maps;
template companion equals fingerprinted ROM. Canonical current profile is
different (`025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`)
and was NOT substituted. No profile, macOS, runner, or authored array edits.
All initial dirty work retained; no commits or app/aggregate builds launched.

## Diagnosis → reusable production repairs

1. **Physical scene omission, not an authored-array bypass.** Middle stair cells
   have canonical `stair_treads`/`stair_rails` group semantics, but no explicit
   tile role and no matching rule assigning an instance assetGroup. Classification
   correctly recognizes the complete BG1 4x4 stair and attaches all16 members at
   base0. Cone-free scene recovery previously accepted only explicit stair roles;
   consequently its middle rows existed on sub but not in the physical scene.
   It now accepts canonical authored stair group membership even when the
   observed instance group is empty, retaining explicit conflicting role rejection.
   Authored Doorway is also eligible for the existing observed subscreen recovery.
   Local height AND normals remain required. Stock main pixels/colors, existing
   scene receivers, OBJ/HUD and nonphysical lamp art remain untouched.

2. **South column support rejected by complete-frame ownership.** Canonical
   `south` on V-flipped support correctly transforms to **north** already.
   Four overwritten outer-band ROM words at row58 reject the whole-frame proof.
   A new independent, conservative two-band ROM column proof accepts only the
   marked roomward foot and its exact surviving inner neighbor (all axes/H/V,
   priority-only writes accepted). It accumulates ownership claims before
   publication, rejects conflicting contacts/alternate sources, and never sets
   doorAssembly, claims the opposite jamb/outer band, or invents split-BG support.
   The foot's explicit transformed contact requires reproduced occupied support,
   solved same-plane base and authored zero contacting edge. The next exact band
   composes `landing+8+authored_local`; opening pixels retain `landing+local`.
   No wall, nearest floor, visibility, room/hash whitelist or blanket offset is
   used. Cache semantic equality and entry-state keys include the new evidence.
   Independent Python validation reproduces the partial two-word proof and checks
   landing/band/local preservation; adapter reports `surviving_inner_column`
   separately from `complete_frame` ownership.

## Actual effective sources (not diagnostic tints)

`build/room051092/{baseline,source-after}.sources.jsonl` inventories every observed
BG pixel on main/sub/scene, with BG, cell, canonical tilePixel, hash, H/V, local
source selection, placement override, effective height and transformed normal.
The probe uses `S9xRemasterResolveFrameGeometry`, including facing/flip/context
and normal-layer resolution, rather than reading default height presence alone.
Exact own092 affected sources have **no active context/flip/facing override**;
every restored pixel retains its authored height/normal arrays and sampling.

| BG1 placement | authored local | base | effective/scene result |
|--|--|--|--|
| (30,56),(33,56) |0..7|0|64 pixels each restored; authored normals V-flipped |
| (30,57),(33,57) |0..7|frame8/opening0|64 pixels each restored; frame8..15/opening0 |
| (30,40),(33,40) |12..17|0|32/64 observed scene pixels, respectively |
| (31,40),(32,40) |13..15|0|64 scene pixels each |
| (30,41),(33,41) |6..11|0|51/64 observed scene pixels, respectively |
| (31,41),(32,41) |10..12|0|64 scene pixels each |

The two stair rows restore467 scene pixels. Existing scene/main receivers explain
the partial left-rail denominators, not guessed filling. Four jamb cells restore
256 scene pixels. Actual placementHeight is false throughout these cells;
no floor clamp or replacement array suppresses the authored data. The old sub
already sampled the correct local arrays, but the lighting scene did not contain
these receivers. Restored scene pixels now consume both their intrinsic relief
and transformed normals. The jamb next-row band offset is a real production
height correction, not a debug classification change.

The southern support is proven on gameplay plane1, not active Link plane0;
floors `(30,55),(33,55)` have edge-local0 and solved plane1/base0. Plane0 at these
contacts is special attribute28 and is **not** fabricated as ordinary support.
BG2 rows40/41 are overwritten `ccdfce079b151621` cells, not stair owners; no BG2
stair geometry is manufactured. Row58 BG1/BG2 local0..7 remains base0/unowned;
the full assembly's missing provenance is still unresolved. Local input art at
BG1 `(31/32,56)` retains signed -8..-2 and is not flattened.

Initial exact replay matches146850 BG observations; restored receivers contribute
additional observations on subsequent replay, yielding147106. Repeated unchanged
profile publication/resume is byte-identical for packets and complete BG maps.
This change in observation count is not changed ROM identity or solved coverage.

## Corpus and regression evidence

Union of complete Application Support inputs and retained prior corpus bundles:
118 unique own-input combinations,236 BG solves, rooms002/011/012/022/041/051/061.
`build/room051092/corpus-inputs.json` lists every input; zero missing companions
in that declared union, zero basic-input rejections. It is not all320 rooms/all
states. Both BG placement inventories retain8192cells per solve pair; unknown,
empty and non-geometric cells are not declared solved geometry.

Before/after shared solver audit (`union-{before,after}.csv`):

| metric | before | after |
|--|--:|--:|
| doorway cells |2748|2748|
| complete-frame owned cells |1844|1844|
| authored support markers |56|56|
| directional plane attachments |216|232|
| unknown doorway cells |1864|1848|
| structural conflicts |568|568|
| unknown walls |1488|1488|
| attached stair cells |2464|2464|
| required support plane0 / plane1 |250532 / 613936|unchanged|
| solver-missing supports |0 / 0|0 / 0|

Four own Room051 captures090/091/092/093 each recover four placements; no previous
attachment is lost. Before binary is compiled from current source with only
`ALTTP_TEST_PRIOR_DOORWAY_COLUMNS` ablation, not an unrelated older solver.
Physical-scene per-pixel audit is exact092 plus preserved exact071/061010 fixtures;
the236-solve audit does NOT certify every corpus viewport's effective receivers.
Unobserved other states and missing asset metadata remain unsupported denominators.

Passing focused evidence:
- `dungeon_doorway_column_test`: all axes/flips, per-plane support, band datum,
  direction/alternate-support rejection, semantic/cache-key invalidation.
- `dungeon_doorway_column_capture_test`: exact own092; full spatial H/V transforms,
  overwritten-word rejection, priority-only acceptance, alternate-source rejection,
  no forged whole-frame/row58 ownership;467stair/256jamb actual scene samples,
  immutable effective local heights/normals. Also ASan/UBSan with repeated resume.
- `scene_subscreen_wall_test`: Doorway and canonical group activation, conflicting
  explicit role rejection, effective context/placement/flip source regressions.
- door propagation, door-foot, stair-assembly support, semantic-cache,
  frame-recognition288 cases, placement-reuse regressions pass.
- Exact071 cap/stair fixture retains288solved BG1walls,16owned stair members,
  overwritten BG2 rejection and unchanged arrays. Exact061010 retains896effective
  doorway pixels and169178identity observations.
- Python independent doorway/audit tests pass;26 complete-room validator tests
  pass. Updated overwrite fixture distinguishes surviving-column proof from full
  frame proof and rejects wrong foot band/overwritten foot.
- `git diff --check` passes. Parent owns runner registration and app/aggregate gates.

## Mandatory complete-room v2 and next action

`build/room051092/gate/gate.json` records the actual full command, input SHA pins
and **exit1**. Complete ALTPPD1 wrapper, matching ALTPAT1, exact-collision linked
doorway graph (directed transitions retained), pinned state, own generated profile,
runtime solution, compiled ALTPHM3 map, validation JSON/SVG are all supplied.
Reachable denominator `[886,0]`,886/886 valid support placements; independent
stair audit remains passing. South four marked placements independently pass.
Room-wide blockers remain: four structural conflicts,8missing north marked frame
placements/16ambiguous-visible-plane records, and BG2 compiled map missing
`(30,39),(33,39)` expected24. Zero unknown wall cells does not waive those failures.
The graph's reachable plane1 denominator0 is distinct from runtime's larger
candidate/support solve; it is not evidence of exhaustive plane1 gameplay review.

No fresh interactive gameplay comparison or capture approval was performed.
Smallest next shared capability: resolve north doorway visible-plane ownership and
the overwritten BG2 upper stair-row support/map provenance, then rerun v2 to zero
before handing the room back for fresh gameplay inspection. The row58 overwritten
assembly remains an explicit evidence gap; this repair intentionally does not
forge its writer ownership merely because its inner jamb columns now attach.

## Independent parent integration checks

Parent exact092 production replay under ASan/UBSan passes with147106matched
BG observations,467stair and256jamb effective scene pixels, unchanged local
height/normal sources, transformed columns and overwrite/alternate-source
rejections. Independent doorway/stair/complete-room-validator Python tests pass
30/30. Inspector25checks and tooling14tests pass. Release/Debug arm64 builds
succeed (`build/room051092-parent/{release,debug}-build.log`).

Parent reran the recorded complete-room v2 command and reproduced exit1;
`build/room051092-parent/validation-v2.log` retains its output. This is not room
approval. The new fixture-free surviving-column regression is registered in
the curated full suite; aggregate result is recorded separately. No authored
profile data was changed and no fresh interactive gameplay review was performed.

Final stable curated CPU/Metal gate passes **43/43**:
`build/room051092-parent/full-tests/20261025T231152Z-cxg_ckpy/results.json`.
Parent byte-for-byte canonical profile comparison and `git diff --check` pass.
This regression result does not waive the nonzero complete-room validation gate.
