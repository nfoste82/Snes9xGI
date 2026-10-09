# Room061 / capture042 — reviewable but incomplete

Obstacle local-shape repair verified, **not room approval**. Parent completed Debug
and Release app builds into `build/debug/` and `build/release/`, independently
passed the obstacle-invariance regression, inspected before/after replay images,
and passed `git diff --check`. Fresh gameplay comparison remains pending.
No profile, backup, authored height/normal array, semantic
JSON, schema30 or artwork field was edited by this repair. No staging/git reset.

## Evidence and exact pins

Artifact directory (`OUT` below):
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/obstacle042`.
All detailed input SHA256 pins are in `summary.json` and corpus per-packet records.
Exact042 and matching041 packet/sidecars are retained as `immutable-*` copies;
the summary tool refuses replacement with different source bytes.

- Exact user packet `frame_room061_042.s9xrmf`, schema30, SHA256
  `bcc0ce487ba086959cf659fd7cb377ecd628f2d79d268c0bf0d45b7e6a11ee94`.
  **No042 sidecars exist.** It contains no SCN1 independent scene pixels.
- Explicit matching replay candidate: `frame_room061_041.s9xrmf` and its own
  sidecars, never current profile. Profile SHA256
  `7fe5abdbec32bec58c288ced1a61884af2cf40fe11f8bf92a8aa3b2af9947465`, serialized
  FNV1a `8540631947d0fa70`. Input SHA256
  `adea8c3547ce6a5620e331ceea19d098cc22453e7d82707837739f55d4a46876`.
  Templates SHA256 `be6a8a9c5b436772f96700973307ec81fc31843e06118e0e567b658e59bc5ddd`.
- ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
- Candidate state: room97/0x061, previous0, settled submodule0, collision0,
  Link(651,3353), gameplay plane0, scroll(531,3246), BG graphics16384 each,
  PPU maps[4096,0]/sizes[3,3], floor-rise50. Epoch1862/cache12222182844157273721.
-042 matches candidate complete maps/flips/canonical coordinates for112533
  main/sub BG pixels. Every overlapping packet/profile authored height/normal
  array and explicit role compares equal. This proves a matched replay, **not**
  that unobserved042 state equals041; full042 state pin remains missing.

## Root cause, not diagonal reauthoring

Three reported assets were Unknown role, no obstacle group. Authored relief was
present and unchanged. Solver emitted immediate collidable-border fallback;
`assignBorderHeight` replaced it with minimum outward0..7 ramps. Actual packet
contained those overrides. Semantic-diagonal activation did **not** classify
these tiles or change their normals: correction mask0 on every reported instance.

ROM source `src/dungeon.c`, subtype0 handlers39/3d call
`RoomDraw_Object_Nx4(2,src,dst)`, offsets0x84a/0x85a. This is freestanding column,
not the separate wall-column dispatch. Six intact BG2 columns at x12/16/20/42/46/50,
y28..31 match all eight column-major template0x84a words, including H/V identity.
Three reported hashes are rows29..31; top row28 also participates. No asset hash,
room rectangle, normal/art similarity, nearest-floor or fill rule is used.

Shared classification now recognizes complete freestanding columns and statue
dispatches (2x3 offsets0xe26/0xed2/0xf5e), preserving intrinsic relief. Existing
explicit roles veto inference. Accumulated ownership rejects partial overwrites,
unsupported roles, uniform templates, conflicting overlapping assembly origins;
priority-only writes and full transformed placements survive. Support contacts
remain plane-specific in the existing geometric solver. Obstacle heights never
enter wall seam/ramp interpretation. Unknown support remains255, not default proof.

Apply retains a heap/direct-index producer-obstacle mask and blocks border,
mixed-floor and diagonal-preview replacement for both proved usage and explicit
obstacle roles/groups. Explicit obstacle role also beats stale stair-group offset
exclusion. Continued-diagonal normal correction rejects authored obstacles.
No new per-pixel linear scan or large emulator-stack scratch was introduced.

## All64 canonical source samples

`captured042.json` / `after042-pixels.json` enumerate authored -> selected local
override -> signed base+local, normals, flips, instance IDs and main/sub/scene
reference counts for every reported occurrence.18 instances,1152 main pixels;
none on sub, none SCN1.858 canonical local samples differed before, zero after.

| hash | authored span | before source | after source | base |
|---|---|---|---|---|
|19088620e4b960cc|15..19|min outward0..7|authored unchanged|0|
|2fd3eada43bdfcd4|0..15|outward0..7|authored unchanged|0|
|eff6f17a9f4420d9|0..6|min outward0..7|authored unchanged|0|

Example canonical pixel0 on190886: authored17 -> override0 -> effective0 before;
after authored17 -> no override -> base0+17. Canonical pixel7 on2fd3:
authored15 -> override7 -> effective7 before; after15. Obstacles are not required
to be raised when proved floor base is0. Both authored and applied normals survive.
GI upload (`mac-render.mm`631..649) and inspector (`inspector_height.h`) consume
the same selected local source plus signed instance base. No separate diagnostic
shape fixes. Independent scene behavior is exercised synthetically because042
has no scene packet evidence.

Images: `matched042-captured-composed-height.png` (before),
`matched042-recomputed-composed-height.png` (after), 0..100 grayscale range.
PPMs and matched042 review packet retained. Visual inspection confirms restored
column relief, but does not validate fresh emulator execution or room coverage.

## Corpus / complete-room gates

Own-profile replay corpus:102 indoor packets;28 successfully matched complete
packets;67 missing required complete evidence;7 refused identity/camera mismatches
(041 base/001/004/005,060011,061006/007). Includes complete BG1/BG2 offscreen cells,
not viewport-only instances.2709 classified authored/proved obstacle placements,
346752 all64 main/sub canonical samples, zero changed local sources and zero
unresolved obstacle bases in this admitted corpus. Visible base distribution:
444 at0,2166 at24,99 at50. Negative signed bases are covered synthetically(-24),
not observed in this obstacle corpus. Overlapping supports remain independently
tested by dungeon_obstacle_support_test;1613 corpus obstacle placements retain
both plane support bases independently. Corpus overlap count is in summary.json.
Unclassified unknown-role obstacles outside supported complete ROM templates
are **not** claimed in the obstacle denominator. Hundreds of other rooms/states
and unsupported handlers remain outside this evidence corpus.

Mandatory validation-v2, own041 profile pin, verified ROM templates, complete
ALTPPD1/ALTPAT1/doorway graphs and ALTPHM3 maps:

|state|support|plane denominators|missing|conflicts|solved/unknown walls|v2 exit|
|---|---|---|---|---|---|---|
|055 fixture|439/439|439,0|0|0|1391/1|1|
|060 fixture|711/711|437,274|0|1|391/66|1|
|061 fixture|1568/1570|1570,0|2|0|894/12|1|
|041 live candidate/room061|1568/1570|1570,0|2|0|894/12|1|

Live graph is re-explored from room-local observed seed(139,281), not a height
anchor. Reference playback stair edges are reused **only after full attribute
SHA256 and stair-record equality**, recorded explicitly. No fresh Snes9x movement
playback. Independent Python complete obstacle ownership reproduces transforms,
overwrite/role veto and ambiguity rejection in v2; does not trust runtime roles.
All gates remain nonzero; do not hand back as approved room capture.

## Reproduce / tests

```sh
c++ -std=c++17 -O2 tools/ALttP/alttp_live_height_repair_probe.cpp -o "$OUT/probe-after"
c++ -std=c++17 -O2 tools/ALttP/alttp_runtime_room_heights.cpp -o "$OUT/solver-after"
c++ -std=c++17 -O2 remaster/dungeon_obstacle_invariance_test.cpp -o "$OUT/invariance-test"
"$OUT/invariance-test"
"$OUT/probe-after" "$CAP/frame_room061_041.s9xrmf" "$OUT/matched042" \
  --packet "$CAP/frame_room061_042.s9xrmf"
python3 -B tools/ALttP/alttp_audit_obstacle_corpus.py \
  --captures "$CAP" --probe "$OUT/probe-after" --output "$OUT/corpus"
python3 -B tools/ALttP/alttp_run_runtime_height_fixtures.py \
  --solver "$OUT/solver-after" --fixtures build/remaster-geometry/surface-audit-room-local \
  --profile "$CAP/frame_room061_041.s9xrmf.profile.toml" --output "$OUT" \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
python3 -B tools/ALttP/alttp_validate_obstacle_live.py "$CAP/frame_room061_041.s9xrmf" \
  --solver "$OUT/solver-after" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
  --fixture-graph build/remaster-geometry/surface-audit-room-local/room-061-doorway-graph.json \
  --output "$OUT/live-v2"
```

Passed portable tests: obstacle_invariance, obstacle_support, template_wall,
complete_assembly, geometry, mixed_floor, semantic_diagonal, doorframe_runtime,
doorframe_placement, inspector_height; approved preview061026(122 previews),
variable-diagonal061032; actual room060028/032/034 replay environment assertions.
Passed512/544KiB Release and Debug small-stack checks. Python obstacle-assembly regression1,
existing validator23, compiled-map3; git diff --check. Initial invocations of
preview/variable tests without/cross-room arguments rejected as designed, then
correct pinned captures passed. Debug/Release app builds and interactive comparison
are parent tasks, not performed here.

Smallest next action: parent builds, then fresh matched live room061 capture with
all sidecars and camera/plane views. Full room approval additionally needs remaining
required geometry solved (not the obstacle equality metric). No042 exact state
evidence can be recovered from its absent sidecars; keep candidate label explicit.
