# Room051 own049 runtime omissions (2026-10-22)

**Superseded wall diagnosis:** follow-up `_052` proves the east/west authored
bands can attach through exact tangent seams to independently supported authored
column assemblies. The absent foot array is real in these pins, but is NOT proof
that the wall bands lack authored data or that user reauthoring is required.
See `dungeon_room051052_report.md` for the reusable repair and remaining blockers.

**Room051 / own049: reviewable but incomplete.** Two shared runtime omissions
are repaired; east/west shell placement remains blocked by missing canonical
local metadata. No profile, authored array, floor datum, room rectangle or
room/hash elevation whitelist was changed by this work.

## Exact evidence

Capture and contemporaneous complete sidecars:
`~/Library/Application Support/Snes9x/Remaster/frame_room051_049.s9xrmf`.
USA ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Capture SHA256 `4d14dbb9c3869dba64b4f70a4ae41e449998c2e5068614ceb7cd1b7589dbda65`;
profile SHA256 `e66e261e26a43fa60056d51aefe0de8fa570082265eed1acca77efb754e45b7a`,
serialized FNV `954ad2dc464e25ee`;
complete ALTPDI1 SHA256 `603879d123bff8a8a0272653697be0a5735159c3c479af272bc3c6610a122c56`.
Settled module7/submodule0, Link world(761,2712), plane0, BG scroll(641,2604),
actor scroll(641,2603), both graphics bases16384, configured rise50, independent
authored stair rise24. Complete WRAM/PPU comparison agrees; production probe
matches167480 BG pixel/hash/flip/canonical-pixel observations.

Own048 is the matching available lower viewport, not visible in own049. Its
production probe matches134362 BG observations. The screenshot in
`build/room051049-repair/own048.png` shows its scope.

## Symptom → failing stage → reusable repair

1. **Far-north movable blocker loses all heights at screen edge.** The complete
   authored footprint has24 physical8x8 children, but the pixel-observation matcher
   only sees18 when the upper row lies at screenY=-8. It rejects the whole motif
   before producer/support resolution; no missing local relief is responsible.
   `sprite_obstacle.h` now reconstructs only wholly clipped children from exact
   complete VRAM plus latched16x16 OAM entries. A uniquely matching USA mantle
   actor and all six complete OAM dispatch entries are mandatory. Signed Y comes
   from that actor/dispatch, not an arbitrary OAM byte sign threshold. Every
   on-screen child still needs owned ink; wrong graphics addressing, duplicate
   actors/slots, sparse metadata and ambiguous ownership reject. Complete room
   contact support is queried even when its screen coordinate lies outside the
   viewport. Hidden children are identity evidence, not fabricated receivers.
   Canonical hashing moved unchanged into `tile_hash.h` for this standalone reader.
   Own049 resolves actor0/plane0/base24: **1022 visible OBJ pixels restored**;
   local authored arrays remain intact. Paused refresh retains exact hidden proof
   without holding dangling VRAM pointers. The production capture probe now reads
   its `.sprite-input.bin`, previously omitted from its replay context.

2. **Authored lower-floor data absent although artwork exists.** The independent
   lamp-free scene omits BG1; stock subscreen still retains real authored floor
   ownership. Prior physical receiver repair admitted Wall/WallCorner only.
   `S9xRemasterRestoreSceneSubscreenWalls` now admits explicit effective Floor too,
   requiring authored height+normal, exact palette artwork, USA pin, empty main
   and scene, and non-Unresolved/non-Conflict occurrence. It restores ownership
   and canonical color together before occurrence placement; no lamp-mask,
   unknown-role or arbitrary color-math BG1 rescue. Own048 restores **13704 floor
   receiver pixels**, missing-height pixels14876→1172. Actual CPU lighting fields
   recover, not merely inspector tint. Existing BG1 seam/base conflicts remain.
   Unknown/Other/stair/door usages remain excluded; artwork presence is not proof
   they belong to the floor. Own048 remaining observed authored non-Floor/non-Wall
   subscreen ink includes105+80+89+64 Unknown pixels,256 Other and256 stair pixels;
   these require role/assembly evidence, not blanket BG1 copying.

3. **Upper east/west wall shell unadjusted.** Final placements are classified
   Wall, not a cache or projection omission. The shared innermost foot asset
   `v1:4bpp:966069b2157ea21f` has authored normals/Wall role **but no local height**
   in this pinned profile. It appears24 times at x19/44, y16..19,22..25,28..31.
   Its floor base is24, but cannot provide a height seam for three outward bands;
   these72 placements remain unknown (x16..18/45..47). Existing outward local
   ramps are0..7, but a missing foot cannot justify assuming another8-unit band,
   copying a nearby wall, or flattening the shell to24. Some cap hashes also
   lack normals. No unauthorized authored metadata was invented. Smallest
   correction is canonical reusable foot relief/seam metadata review, followed
   by solving/auditing all its transformed placements. The entire lower BG1
   structural graph additionally remains0solved/288unknown walls,419conflict nodes
   (including the earlier three-unit seam contradictions).

## Regression and available corpus

`sprite_obstacle_clipping_test.cpp`:20 signed-base/camera cases (top/left/right/
bottom/two-edge clipping; bases-8/0/24/50), retained paused proof and real field
activation;9 wrong/missing producer/VRAM/owned-child/support/metadata negatives.
Original sprite-obstacle tests pass. `scene_subscreen_wall_test.cpp` now checks
all H/V combinations for Floor/Wall/Corner, effective relief/base, idempotence,
wire roundtrip and prior eligibility/priority/mask/context/conflict negatives.
Both narrow new suites pass ASan/UBSan. The quick test gate includes both and
passes16/16 (final immutable-source rerun); runner contract tests pass6/6.
The initial quick run correctly rejected a test edited during execution;
it was rerun after edits stopped, rather than accepting a mixed-source result.

Physical scene audit: **123 indoor packets /12 room IDs**,0 rejected reads;
4changed packets (051044/045/046/048),**34285** restored field pixels total;
all other packets unchanged. These are the files currently available, not the
earlier340packet/34room corpus, whose missing sidecars/files cannot be assumed.
Sprite audit: **134 packets**,46 support-restorable/88 unsupported (missing
complete sidecars/metadata, non-dungeon or unsettled state); baseline-ablation
versus repaired support application changes only own049,0→1022resolved pixels,
no previous resolved-pixel losses. Captured planes remain separate. This is not
an exhaustive320room/state audit; other clipping producer families are unsupported.

## Mandatory complete-room gate and artifacts

`build/room051049-repair/` retains pinned replay manifest, complete ALTPPD1 plus
matching ALTPAT1, runtime JSON, ALTPHM3 map, observed-seed full static movement
graph, validation-v2 JSON/SVG, production before/after packets/inventories,
per-BG4096placement JSON, field inventories, corpora and test/build logs.
Graph explores the complete collision room from the actual observed seed; no
invented directed stair playback was supplied. World seed remains pinned in
state/runtime while the graph's movement nodes are correctly room-local.

Validation v2 **exits1**: planes[886,0],862/886valid supports,24missing local
sources; fallback26/26, walls588solved/72unknown,4runtime conflict nodes; compiled
map2missing/6mismatches, overlap0. No room approval. Geometry-map inventories
before/after these receiver/producer repairs are unchanged; authored-profile pins
are untouched. Fixture055 gate exits1 (439/439support,1conflict,16unknown walls);
060 validator rejects corner seam disagreement;061 rejects directional diagonal
preview lacking independent ROM proof. Those are explicit unsupported gates,
not passing fixtures.

arm64 Debug build was attempted with documented xcodebuild command; **SIGTRAP
before compilation** (DVT fs-event/cache failure), same environmental failure
seen in previous task logs. No new verified app bundle or interactive Snes9x
camera/plane capture. Legacy `remaster_test` reaches known-stale assertion
`profile.assetGroups.size()==11`; do not claim it passes. `git diff --check`
passes. Fresh interactive validation remains required after a successful native
build, but do not label the room successful or request approval while v2 fails.

## Independent final verification

Parent-session reruns against the final, unchanged worktree pass the complete
portable gate **30/30** and quick ASan/UBSan gate **16/16**. Results are retained
under `build/room051049-parent-verification/20261023T042306Z-jorakp6m/` and
`20261023T042534Z-jmes_zdu/`. The user's current profile is byte-identical to the
investigation baseline. Default and isolated-DerivedData native build attempts
both terminate with SIGTRAP before compilation; an independent Metal test also
fails shader-compiler setup with `XPC_ERROR_CONNECTION_INVALID`. These are
explicit verification blockers, not passing native/GPU checks.
