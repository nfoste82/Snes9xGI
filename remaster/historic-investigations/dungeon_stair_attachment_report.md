# Complete stair assembly datum attachment

Room051 / exact `frame_room051_071.s9xrmf` state: **reviewable but incomplete**.
The stair capability passes its independent checks; the complete-room gate
still exits 1. No profile or macOS UI files were changed.

## Failure class and repair

The exact ROM `0x10c8` 4x4 intra-room footprint already supplied a measured
rail rise to the floor graph, but not a placement datum to its members.
Special collision tread cells and solid rails have no ordinary floor/wall
graph nodes. The runtime additionally suppressed all StairTop/StairBottom
and stair-group placement offsets. Thus a default-zero picture concealed
the missing contextual attachment (and could not represent nonzero stairs).

The shared proof now verifies final words (ignoring priority-only writes),
H/V transforms, compatible semantic roles, complete local arrays, and both
uniform, monotone rail cross sections. Identical descriptions of a bilateral
H-symmetric footprint are one owner, not two assemblies. Distinct competing
stair records invalidate the attachment. Unknown alternate context/facing/flip
sources are explicitly withheld rather than proving from the default array.

After support solving, two cells at each landing must have solved,
same-endpoint plane bases and authored zero local relief. The upper endpoint
must differ by the measured authored rise. Members then consume the lower
datum, unchanged authored arrays, and no border fallback. Stair member bases
are terminal structural placement offsets, not actor-support or movement
edges. Both planes survive independently; incompatible claims are withheld.
The renderer consumes only a producer-identity-matched owned stair datum;
ordinary/unowned stairs keep the existing zero-offset policy. There is no
room/hash whitelist, new metadata grammar/type, global rise addition, gap fill,
or BG-number height inference.

The runtime adapter exports per-plane stair members and actual consumer pixel
arrays; validation v2 independently reconstructs the ROM footprint, transformed
rail endpoint proof, landing bases, immutable local samples and runtime result.
The compiled BG2 map is checked for attached stair plane members as well.
Cache differential inventories include the new producer/attachment fields.

## Exact071 result and pins

- Supported ROM SHA256:
  `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
- Own profile SHA256:
  `9ff34398d990e899eec578c02443013f4b0762bb7a1be35c02906de5a1a48134`.
- Current canonical profile SHA256 (preserved):
  `45815eb9f0d31dc11bc04934c95985f31710a1f910c574e84d9d97e16a24aec8`.
- Exact capture replay uses its own complete companions/profile, not the newer
  canonical profile. 147451 observed BG pixels match input identities/transforms.
- BG1 staircase origin `(30,39)`: 16/16 members attached at base0,
  low plane1 / high plane0, measured rise24, landing bases0/24. Both named center
  arrays `99863ac4c1bb8e68` (12..17) and `76a7931735f4b282` (13..15) are unchanged.
- BG2 final footprint is overwritten and deliberately not attached. BG2 top
  row artwork does not license filling the missing footprint.
- Existing BG1 cap-region result stays 288 solved walls, 0 unknown, 0 conflicts.
- Complete v2: 886/886 reachable supports, plane denominators `[886,0]`,
  zero unknown walls, four existing runtime structural conflicts. Stair audit
  passes 32/32 plane-members; no stair proof/consumer mismatch.
  The four runtime conflicts and 16 door issues below are different metrics;
  this is not a reduction from16 to4 or an unsupported conflict repair claim.
- Gate exit1 persists: 12 missing marked door placements, 16 door issues,
  and BG2 map gaps at `(30,39)` / `(33,39)` (expected24, map unknown).
  Door repair remains out of scope. An exact-collision directed graph was
  transferred only after matching the complete ALTPAT1 digest.
- Separate current-profile research replay runs the complete v2 gate with that
  newer profile pin and has the same counts/gaps; it is not capture-pinned
  visual approval.

Artifacts: `build/remaster-geometry/stair-attachment-review/` includes own071
production replay packet/inventory, lossless complete ALTPPD1/ALTPAT1 wrapper,
pinned graph/state, runtime JSON, signed ALTPHM3 review map, v2 JSON/SVG/gate pins,
corpus CSVs and `current-profile/` research outputs. Generated artifacts are
review evidence only. No fresh interactive Snes9x comparison was performed.

## Corpus and regressions

Available own-input corpus checked: 98 complete captures / 196 BG solves,
rooms002,011,012,022,041,051,061. Across 236 detected stair records counted
per BG solve, 118 complete proofs attach (BG1 67 / BG2 51), yielding 1888
BG member occurrences (captures can repeat the same state). The other118 records lack a complete admissible
final footprint/local-source proof and remain unsupported; they were not
filled. All pre-existing corpus audit fields, including support denominators,
missing supports, wall counts and conflict counts, are unchanged. These are
sampled captured states, not all320 rooms or every gameplay state.

Tests passing:

- `dungeon_stair_assembly_support_test`: north/south, horizontal reflection,
  rise24/32 versus configured50, signed datum -17/+13, array invariance,
  priority-only writes, cross-BG signed relation transport, partial overwrite,
  nonuniform/missing rails, incompatible roles, alternate source rejection,
  competing ownership and plane-mismatched endpoints.
- `dungeon_authored_stair_test`: ordinary unowned suppression remains; proven
  signed nonzero producer bases apply on both BGs without replacing local arrays.
- `dungeon_stair_attachment_capture_test`: exact071 16-member attachment,
  overwritten BG2 rejection, named authored array invariance and preserved cap fix.
- `link_stair_height_test`.
- Python independent stair audit 2 tests (transforms/signed data and negative
  word/upper endpoint/plane/consumer/unclaimed-member cases).
- Existing v2 validator 26 tests and compiled-map 3 tests.
- `git diff --check`.

Safety review additionally fixed profile cache-key coverage for alternate
context/facing/flip activation and rejected alternate local sources at both
landings, not just footprint members. `dungeon_semantic_cache_test` verifies
selector-only invalidation, and C++/independent Python stair tests reject
alternate landing sources. Same-plane attachment claims are gathered before
publication and require unanimous bases; overlapping complete stair owners
are rejected. Producer hash/flip verification precedes the owned-stair consumer.

Legacy 055/060/061 fixture runner was attempted: old obstacle candidate is
invalid under current parser (duplicate group ownership). Using current
canonical metadata yields room055 support439/439 exit1, room060 support0/711
exit1 (different graphics metadata scope), and room061 fails independent
diagonal proof because that runner omits ROM provenance. These are explicitly
not successful historical fixture gates or claims of stair regression coverage.
The 98 exact own-input corpus is the matching runtime regression corpus.

Main session owns aggregate tests and arm64 macOS Debug build after this source
handoff. Remaining smallest next action is reusable door/overwritten ownership
evidence and BG2 top-row support/map repair, then rerun complete v2 until zero
before fresh matching gameplay review. Context/facing/flip-local stair geometry
needs shared placement-local source compilation before attachment is authorized.

## Independent parent verification

Parent recompiled the exact-capture regression and reproduced147451 matched BG
pixel usages,16owned BG1 members at base0, overwritten BG2 rejection and the
preserved288solved BG1 walls. Independent Python stair/complete-room-validator/
compiled-map tests pass31/31. Re-executing the recorded complete-room v2 command
still exits1 (`parent-validation.log`); it is not room approval.
Final arm64 Release/Debug builds pass
(`build/stair-attachment-{release,debug}-build.log`). Canonical authored profile
SHA256 remains45815eb9f0d31dc11bc04934c95985f31710a1f910c574e84d9d97e16a24aec8.
The two fixture-free stair regressions are now registered in the curated full gate.
An initial aggregate run was invalidated by source safety refinements; the final
stable aggregate runs separately under `build/stair-attachment-final-tests/`.
Parent ASan/UBSan runs pass for both the complete exact071 production replay
and synthetic signed/plane/source stair attachment regression. Independent v2
stair audit reports success with32required plane-members and no consumer or
compiled stair-map mismatches, while overall room success remains false.
Parent independently compared196before/after BG corpus rows from98captures:
all pre-existing fields are identical;118complete proofs and1888members match
the reported counts. The separate current-profile research v2 command was also
rerun and remains exit1; its remaining failures are not silently waived.
Final stable curated full+Metal gate passes **39/39**:
`build/stair-attachment-final-tests/20261024T032419Z-ezl3t65y/results.json`.
This is not whole-room approval. Parent authored-stair ASan/UBSan consumer test,
23inspector checks and14tooling Python tests also pass.
