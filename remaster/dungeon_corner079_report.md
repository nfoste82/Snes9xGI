# Room060 own079 corner attachment repair (2026-10-14)

**Reviewable but incomplete.** Runtime attachment repaired; complete-room gate
still fails. No profile edits, no guessed ceiling/floor heights, no approval.
macOS build/git review belong to the main agent; fresh gameplay review pending.

## Exact input

`~/Library/Application Support/Snes9x/Remaster/frame_room060_079.s9xrmf`
and its own complete ALTPDI1, ALTPDT1, terrain and profile sidecars. ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
profile serialized FNV `3f781bcda5e8990c`, byte SHA256
`b6784c1c97cd7a1ef01d426a790608056b087906e8e884fba669ab1e09ca1a30`.
Active `Profiles/ALttP/alttp-profile.toml` was byte-identical at comparison.
No active-profile substitution. Packet v35,256x224; settled module7/submodule0,
previous room061, collision mode0, Link(376,3327), plane1, scroll(256,3220)
(room-local256,148), both graphics bases16384, PPU maps[4096,0], sizes[3,3],
rise50, stair(46,42,high-south,plane-changing). EndFrame replay independently
checked92650 matching BG pixels across main/sub/scene. Complete WRAM/PPU maps
agree; captured templates equal verified ROM table. Entrance identity is not
retained in this live state; exact observed Link position is the movement seed.

## Placements (cell coordinates, not pixels)

`a9b6cfd6b17c9385`: **BG1 only**, ten placements. Canonical signed local model
`max(7-x,7-y)`,0..7; legacy asset has no explicit tile_role but ROM usage owns
the wall. Words/placements:

* no flips2119: (42,8),(42,23)
* H18503: (53,8),(53,23),(57,27)
* V34887: (42,14),(42,44)
* HV51271: (53,14),(53,44),(57,39)

The six already classified occurrences retain base8/effective8..15 on plane1;
plane0 attribute28(hex1c) is not accepted solid-face support. Four at
(42/53,14/23) were unknown/base255; now classified from exact ordered low-corner
strips0xbe6/0xc06/0xc26/0xc46 plus independent perpendicular authored seam,
base8/effective8..15. Two independently authored seeds required; incompatible
roles, overwrite, missing witness and multiple ownership reject. These four
had later writes replacing adjacent original footprint cells, so old endpoint-
wall requirement rejected them. None of this hash is visible in packet079's
winning main pixels: full-room inventory proves the repair, not this viewport.

`ccf87d6be13c02a4`: **BG2 only**, nine high-corner placements; explicit authored
wall_corner/inside with canonical south/east semantics. Local profile is
`min(6,max(7-x,7-y))`,0..6, includes flat cap samples. Words2196(no flip),
18580(H),34964(V),51348(HV). ROM high-corner templates0xb66/b86/ba6/bc6.

* base16: (2,4)none,(29,4)H,(2,61)V,(29,61)HV
* base40: (34,4)none,(61,4)H,(61,43)HV,(34,61)V,(57,61)HV

All nine were unknown/base255 (runtime effective0..6). Both independently solved
same-template tangent faces now agree on16 or40, giving effective16..22 or40..46.
Visible southeast(61,43)64 pixels changed0..6 ->40..46 in production EndFrame.
No profile array or normal changed; no placement-local profile manufactured.
Plane0 solid1 owns the structure; plane1 nonblocking0 is an underpass/occluder
usage and does not become ceiling support. High corners attach terminally: they
do not add movement edges, floor anchors, or graph propagation through the cap.

## Reusable changes

`remaster.h`: ordered corner strip matching supports both axes and overwritten
endpoint folds, with complete perpendicular seam evidence. Stops immediately on
word/metadata rejection; accumulates ambiguous endpoint claims and rejects them.
`frame.h`: clipped high-corner terminal attachment samples only non-cap pixels
on both same-template solid/solved face contacts. Signed samples/base preserved;
unknown, contradictory, missing, cross-plane or incompatible contacts rejected.
No room IDs or asset hashes in runtime repair. Existing classification snapshots,
movement scratch, plane/entry invalidation and inspector changes untouched.

## Gates and remaining blockers

Own079 before/after complete ALTPPD1/ALTPAT1 wrapper, exact captured profile/state,
complete doorway graph (reference directed edges transferred ONLY after exact
attribute digest), compiled ALTPHM3, mandatory validator v2:

* support711/711, planes[437,274],missing0 unchanged;
* BG2 solved/unknown walls1021/9 ->1030/0;
* runtime structural conflicts3 unchanged; directional continuity conflicts67
  unchanged (edge denominator10944 ->11008); **validator exit1 both**.
* EndFrame BG1 unknown wall count0 both; BG2 authored obstacles20,solved16,
  unmatched4 both. Debug restored coverage is not complete-room approval.

Fixture gates exact079 profile:055 supports439/439,missing0,conflicts0,
walls1670/unknown16,exit1;060 supports711/711,missing0,conflicts1,
walls1030/unknown0,exit1;061 solver artifacts generated but validator exit2:
directional diagonal preview BG2(2,38) lacks independent ROM proof.

**Mixed ceiling limitation:** terminal attachment preserves the user's cap6;
it does NOT independently attach cap pixels to adjacent out-of-bounds artwork
whose local authored height is60. Thus repaired40..46 is evidence-backed local
composition, not proof that the ceiling part joins the external60 shell. There
is no canonical authored region mask/independent cap-contact declaration on this
asset. A single tile offset cannot preserve0..6 and simultaneously satisfy
arbitrary independent ceiling/floor levels. Next shared capability: canonical
wall/cap region ownership and per-region high-shell contact constraints, backed
by ROM assembly parts, not treating +Z normals/flat samples as walkability or
copying nearby60. Do not silently alter the authored shape to make that seam fit.

## Corpus/test/performance

Available complete corpus `diagonal-sets`:346 entrance/default contexts,319/320
room IDs,8192 BG placements each (2834432 total). Shared079 metadata used as
research corpus input, NOT matching-gameplay coverage. Paired exact solver run:
solved BG2 walls88511 ->89308; unknown257796 ->257002; conflicts1291 unchanged;
88 changed contexts;0 adapter unsupported contexts. +797 solved vs -794 unknown
also includes classification denominator changes (not one fixed wall population).
Switch/chest/water/door state variants not enumerated; unclassified/nongeometric
and BG1 art are not counted as solved by these BG2 summary metrics. Full corpus
publication remains unsupported. Complete target-room two-BG inventory provided.

Passed: new clipped-corner regression (4 flips,signed-256/0/37,mixed ceiling,
contradictory seam,ownership and underpass negatives); complete-assembly endpoint
regression (both axes,4 flips,negative local relief,overwrite/missing/multiple
ownership); high_corner_ownership,terminal_corner,geometry,classificiation_plane,
semantic_cache,wall_cap,placement_reuse,disconnected_plane,mixed_floor,signed_pit,
cross_bg_wall_support,transition_cache,inspector_height; Release small-stack
synthetic cases on512/544KiB. Checks in new test remain
active in Release. Existing profile untouched; git diff --check passed.

Own079 alternating8 samples: cached21.6562ms vs forced classification42.0216ms;
cached classifications0/full16,solves24 each; two complete plane inventories and
serialized packets exact. Cold single sample41.44ms vs prior39.56ms; warm8.97 vs
8.90ms (single-sample observations, not statistically justified slowdown/60FPS
claim). Additional bounded strip scans only on classification miss, endpoint
owner scratch8KiB heap; cap scan only at solve boundary. No per-pixel live work.

## Artifacts / reproduction

All outputs under `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode`:
`corner079-{before,after}.s9xrmf/.json/.inventory`, paired `-placements.jsonl`
(8192 rows each),`-effective.jsonl`, `corner079-summary.json`,
`corner079-gate-{prior,after}/` complete snapshot/planes/state/profile/graph,
runtime/map/validation JSON/SVG/gate pins;`corner-fixtures/`;`corner-corpus.json`.
Before runtime adapter uses read-only ablations
`ALTTP_TEST_PRIOR_CORNER_STRIPS` + `ALTTP_TEST_PRIOR_CLIPPED_CORNER_ATTACHMENT`.
Production capture probe supports `ALTTP_PLACEMENT_DIAGNOSTICS=1`; offline
scripts `alttp_corner_placement_review.py`,`alttp_corner_result_summary.py`,
`alttp_corner_corpus_audit.py` summarize these outputs without editing profiles.

Old packets retain baked offsets: opening original079 unchanged will still show
0..6 for the southeast corner. Only recomputed review packets/fresh live frames
use repaired base40. No fresh interactive review, occlusion boundary approval,
or matching camera/plane sequence was performed. Do not hand back as a successful
room: main agent should build/review code, then address residual complete-room
conflicts and independent cap-region evidence before capture approval.

Main-agent integration: reviewed classification/terminal attachment and negative
regressions; Debug and Release shared-scheme macOS builds passed with signing
disabled. Updated app is `build/release/Snes9x.app`. Fresh interactive comparison
and cap-region/ceiling continuity remain unverified.
