# Room051 `_071`: ordered corner cap-region runtime repair

**Verdict: reviewable but incomplete.** The shared structural solve now resolves
all 288 classified BG1 walls in the exact state. This is an actual placement-base
repair, not an overlay change. Complete-room validation v2 still exits 1; this is
not an approved room or permission to hand it back as a successful capture.

## Pins and scope

Exact input is `/Users/nic.foster/Library/Application Support/Snes9x/Remaster/frame_room051_071.s9xrmf`
with its own profile, dungeon metadata/input/templates, terrain and sprite
companions. ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
profile serialized FNV1a64 `c4651736b3e5651a`; room `0x051`, predecessor `0x061`,
module7/submodule0, collision mode0, Link `(844,2993)` plane0,
scroll `(724,2833)`, both graphics bases `0x4000`, rise50. Production replay
matches 147,451 pixel usages against the full final maps/VRAM/flips/camera.
Canonical profile SHA256 remains
`9ff34398d990e899eec578c02443013f4b0762bb7a1be35c02906de5a1a48134`.
No profile, authored height/normal array, UI layer or shader was changed.

The change is confined to shared runtime contact solving plus tests and runner
registration. Baseline inspector checks23/23, tooling Python14/14, and mesh39/39
pass; mesh sanitizer checks also pass. These are independent regression checks,
not evidence that remaining complete-room geometry failures have been resolved.

## Root cause and reusable rule

The ordered HIGH-corner strips contain clipped face regions and ceiling plateaus.
For example `(13,53)` is exactly `min(5,7-x)` across every row. Treating its
constant cap edge as an exterior-floor seam imposed a five-unit equality; its
downhill wall continuation imposed the correct eight-unit band relationship.
This poisoned the connected structural graph. The same class includes clipped
`min(cap,max(axis,tangent))` folds near the perimeter turn.

`frame.h` now excludes **only the explicit Floor contact at that proved cap edge**:

- exact existing ROM HIGH-corner template ownership on both placements;
- complementary phases within the same corner-facing/height family;
- complete exact clipped one-axis or clipped max-of-two-unit-ramps model;
- all eight samples of the proposed cap edge equal the cap;
- at least two surviving non-cap samples meet the ordered downhill wall member
  with one consistent signed seam difference.

All ordinary wall/wall, downhill floor, tangent, non-cap and contradictory
constraints remain in the graph. Ownership is supplied by the existing bounded
ordered final-ROM-word classifier, not by artwork/hash or missing visibility.
No room ID, tile hash, camera, BG index or hidden-bit predicate enters the new
rule. All H/V transforms are evaluated in displayed coordinates. No new datum,
default height, local array replacement or flood fill is introduced.
Obstacle contacts are not suppressed by this rule: their support datum is not
proved to be exterior floor. Existing unknown-covered-support rules are unchanged.

This is the justified **cap-contact interpretation** portion of the approved
connector recommendation. It deliberately does not synthesize a full `0..7`
closure over clipped authored pixels, nor replace ordinary visible usage. A
separate physical hidden-closure model needs stronger emitted region ownership
before overriding local samples. Existing authored clipped geometry remains.

## Exact runtime improvement

| Complete classified wall inventory | Before solved / unknown / conflicts | After |
|---|---:|---:|
| BG1 | 0 / 288 / 419 | 288 / 0 / 0 |
| BG2 | 564 / 0 / 16 | 564 / 0 / 16 |

Conflicts are structural event/node accounting, not unique omitted tiles.
`(13,53)` retains local `0..5`, base16, effective `16..21`.
`(14,53)` retains local `0..7`, base8, effective `8..15`.
`(23,58)` retains local `0..7`, base16, effective `16..23`.
The reported BG1 `9214...` usages resolve 38 at base0 and 20 at base8;
`a3b3...` 2/2 at base8; `9836...` 10/10 at base8. This is not a global +8.
Both stair-center hashes remain 2/2 without solved placement bases; the `9eb9...`
door parts remain 2/2 unresolved. Those are separate support/dispatch defects.

Separate gameplay-plane arrays survive unchanged in representation; the new
rule never equates planes or creates traversal edges. Exact wall examples above
are structurally attached on plane1, not inferred from Link's current plane0.

## Tests and corpus

New `dungeon_corner_cap_region_test.cpp`, registered in the full curated runner:
all four flips, clipped affine/folded caps, preserved arrays, missing ownership,
overwritten downhill ownership, incompatible corner family, malformed relief,
and full-height ordinary usage retaining its exterior contact. The fixture
includes independently anchored opposite floor contacts so negatives really
produce contradictory constraints rather than an unanchored apparent pass.

Narrow O2 tests pass: new test, wall cap, template wall, cross-BG wall support,
wall/floor contact (160 cases), obstacle/wall seam (144 cases), and owned wall
projection. The new test also passes ASan/UBSan; `git diff --check` passes.
Existing non-curated `dungeon_unobserved_wall_test` fails
`underpass supplied unproved wall datum` identically with an isolated pre-change
`frame.h`; this pre-existing failure is not claimed as a pass or repaired here.

Audit reruns all 105 previously enumerated complete bundles, using each own
profile pin: 35 accepted (26 Room051,9 Room061),70 rejected at profile pin
(45 Room051,10 Room061,6 Room012,5 Room002,2 Room041,1 Room022,1 Room011).
Across accepted inputs BG1 wall unknowns fall from 7,542/12,150 to 54/12,150;
BG2 remains 246/25,590. This is 7,488 newly solved classified placements, not
105 independent rooms or coverage of all 320 rooms/states. Historical serializer
compatibility remains an explicit rejected denominator. Room060 is not an
accepted own-pin bundle in this corpus; no new Room060 approval is asserted.
The final **Floor-only** predicate was rerun across all 105 bundles after safety
review: the same35 accepted/70 rejected, BG1 unknown54 with0 aggregate structural
conflicts, BG2 unknown246 with288 aggregate structural conflicts. No accepted
bundle loses solved-wall coverage. `corpus-summary.json` and per-bundle TSVs/logs
contain this final-source run, not the earlier broader non-wall predicate.

## Mandatory full-room gate and remaining failures

`build/room051-connector/gate/` contains lossless complete ALTPPD1, matching
ALTPAT1, own profile, current runtime solution, compiled ALTPHM3 map, validator
JSON/SVG/log, and exact command in `gate-command.json`. Graph/state use the
existing complete capture-pinned doorway graph; no mismatched reference stair
trace was imported. Validator **exits1**:

- observed plane0 reachable support886/886, plane1 denominator0;
- immediate fallback26/26; observed doorway overlap0;
- residual BG2 structural conflict count16;
- 16 rejected doorframe ownership records (`missing_intact_rom_door_template`);
- compiled missing/mismatched support claims `(30,39)` and `(33,39)`, expected24;
- whole-room authored-floor runtime-base inventory improves BG1 from
  missing `[812/922,3782/3782]` to `[0/922,0/3782]`; BG2 remains fully inventoried
  `[0/942,0/882]`. This inventory is runtime base plus intrinsic metadata,
  **not independent traversal/ownership approval** of unobserved plane1.

No fresh interactive Snes9x gameplay/lighting comparison was performed. Parent
session owns independent curated full+Metal and arm64 Debug builds after source
stability. Passing tests/builds cannot override the failing room-wide gate.

Smallest next actions: capture-provenance door dispatch/overwrite ownership and
stair-assembly landing support; independently review effective wall geometry in
matching gameplay, especially the still-authored clipped caps and plane overlap.

Artifacts: `build/room051-runtime-followup/connector-after*` exact production
replay/placements; `build/room051-connector/corpus-summary.json` and per-bundle
TSVs/logs; `build/room051-connector/gate/` complete v2 evidence. Generated artifacts
are review outputs, never authoritative source or a canonical-profile replacement.

## Independent parent verification

The parent session rebuilt the production probe and reproduced147451 matched
pixel usages, BG1 288solved/0unknown/0conflicts and unchanged BG2
564solved/0unknown/16conflicts (`build/room051-connector/parent-verified*`).
Reported BG1 hash base counts independently match: `9214...`20at8/38at0,
`a3b3...`2at8, `9836...`10at8. Both stair-center hashes and `9eb9...` remain
unresolved. The recorded complete-room validator command was rerun and exits1
(`gate/parent-validation.log`). Both the exact production replay and new focused
regression pass ASan/UBSan. Canonical profile SHA256 remains unchanged.

Final arm64 Release and Debug builds pass (`build/corner-cap-{release,debug}-build.log`).
Fresh interactive validation has not been performed.
Safety review narrowed the exclusion to explicit Floor contacts (not all
non-wall surfaces) and added an obstacle-contact negative regression. Parent
reran the focused sanitizer test, exact optimized replay and both app builds
against that final predicate with the same results. The first aggregate gate
was invalidated by this source refinement; final aggregate runs separately under
`build/corner-cap-final-tests/`.

For interactive review, restart `build/release/Snes9x.app` or the Debug app,
load the matching profile and a fresh live Room051 state, then inspect BG1 with
Cmd–Shift–K and Height/Normals with Cmd–Shift–D. Capture071's original packet
retains old offsets; opening it alone does not recompute the repair. Compare
`(13,53,BG1)` base16/local0..5, `(14,53,BG1)` base8/local0..7 and
`(23,58,BG1)` base16/local0..7. These are verified bases, not a claim that a
new hidden shell was generated or a seamless upper-floor closure was reviewed.

Final parent O2 reruns also pass: wall cap, template wall, cross-BG wall support,
160wall-floor contact cases,144wall-obstacle seam cases and owned wall projection.
Final stable curated full+Metal suite passes **37/37**:
`build/corner-cap-final-tests/20261023T233136Z-5u9r1w8o/results.json`.
This does not override the failing complete-room validation v2.
