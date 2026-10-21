# Room002 upper walls and Esc resume (2026-10-20)

## Fixed: equal-key invalidation drops all placement adjustments

`S9xEngine.resume` publishes the editor profile, even without edits. BeginFrame
invalidates `runtimeDungeonHeightValid`. EndFrame previously rebuilt only when
the input key differed; identical publication retained the same key and skipped
both rebuild and application indefinitely, until another geometry input changed.
Now an invalid solve is rebuilt even on an equal key. Profile/room/state safety
invalidation is preserved; no stale-map revalidation or frontend-only workaround.

Strong historical evidence: adjacent `frame_room002_009.s9xrmf` has complete,
publishable module7/submodule0 inputs but `applied_map_room=65535`. Its own-profile
production replay restores offsets for13637 distinct visible BG cell/pixel
samples. E.g. high cap(21,36) changes raw0..5 to52..57. User-supplied010 has
`applied_map_room=2` and exactly matches recomputation: it captured the recovered
state rather than the dropped-map state.004 also matches recomputation.

Own009/010 profile serialized FNV7d050a028783828f;00496fbd008f2ae3a2f. Differences
include user normals/emission/door annotations; no profile substitution was used.
All inputs pin ROM66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb.
Production packet/maps/camera parity78541samples009/010,78575samples004.

## Upper-wall investigation: narrowed, NOT repaired

The96unknown BG1 walls are NOT visible in main pixels of004/010. They form
unanchored center-passage wall components (36nodes per center side plus the
bottom continuation), not the visible upper-shell band whose offset disappears
on resume. These are separate defects, not evidence to patch the viewport.

Visible cross-section x20, y42..47:

| BG/cell | base | local | effective | collision planes |
|---|---:|---|---|---|
| BG2 y42 |52|0..7|52..59|01,00|
| BG2 y43 |44|0..7|44..51|01,00|
| BG2 y44 |36|0..7|36..43|04,00|
| BG1 y45 |16|0..7|16..23|1c,28|
| BG1 y46 |8|0..7|8..15|1c,01|
| BG1 y47 |0|0..7|0..7|1c,04|

The first cross-BG boundary is NOT a continuous one-unit ramp: displayed high
foot36 versus low head23 leaves13units. No accepted cross-BG seam currently
relates these differing final words and differing collision owners. Their local
profiles are intact, not clipped during application. The graph audit shows the
upper shell is anchored through isolated flat local60 ceiling artwork on plane1,
not an established floor on its solid plane0:12wall/flat edges at the central
and bottom door bands constrain its52datum. The lower shell is independently
support-anchored0/8/16. This explains why '750solved,0unknown' was not proof of
correct upper-wall geometry. It does NOT justify raising/lowering it by a guessed
constant or overwriting the user's ceiling60; the intended physical join and
missing region ownership still need independent assembly evidence.

Next shared capability: reproduce the complete complementary high/low shell
assembly across BGs (including door overwrites) and carry signed same-surface
contacts independently of actor-plane support/ceiling relief. Where shape changes
axis, contacts need explicit part/region ownership, not whole-tile offset equality.
No blanket height increase, room/hash exceptions or authored-array changes made.

## Regression and coverage

`dungeon_profile_resume_test.cpp`: Debug and O2/NDEBUG pass; repeated identical
profile publication, equal-key invalidation, warm-cache reuse and actual height
edit invalidation. Existing semantic-cache regression now requires a valid rebuilt
map on identical publication (old test checked only unchanged epoch and missed
the invalid flag). Geometry-cache, transition-cache, dialog-height, entry-lifecycle
regressions pass.

Capture probe `ALTTP_PROFILE_RESUME_REGRESSION=1`: three publications each require
byte-identical complete output packets and both-BG map inventories.004/009/010
pass. All85locally available complete evidence sets attempted:73pass,10refused
stale packet/maps/camera,2unsupported/unsettled. This is a lifecycle ablation,
not observed Esc traces or complete dynamic-state/gameplay coverage.

Complete010 validation-v2: lossless ALTPPD1, matching ALTPAT1, exact profile/state,
observed-seed graph, compiledALTPHM3 and verified ROM. Support730/730,
planes[0,730],fallback8/8,missing0,component conflicts0; map missing8/mismatches8,
exit1. Runtime supports[0,3272],missing[0,0],BG1walls566/96,BG2walls750/0,
structural conflicts0. Validation remains failed; upper-wall approval NOT claimed.
No height-rule change beyond restoring application of the existing exact solve.

Release macOS build succeeded: `build/release/Snes9x.app`. Fresh interactive Esc
and upper-wall visual review not performed; quit/relaunch required for new code.
`git diff --check` passes. Specialist delegation attempted again but failed before
execution (`Variant unavailable ... medium`); investigation performed locally.

Artifacts `$TMPDIR/opencode`: `room002009-before*`,`room002010-before*`,
`room002010-graph.jsonl`,`room002{004,009,010}-resume-verified*`,
`resume-corpus/summary.json`,`room002010-gate/`,
`room002-resume-release-build.log`. Original captures/profiles untouched.
