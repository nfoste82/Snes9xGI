# Mostly clipped movable blocker: producer culling (2026-10-23)

**Room051 / pinned `_059`: reviewable but incomplete.** The exact supplied
packet is already correct for the movable blocker; the deeper-clipping defect
is reproduced and repaired in the available exact `_056` packet and controlled
camera crops of `_059`. This is an OBJ producer repair, not room-height approval.
No authored profile, array, BG solver, support base, ROM rectangle or whitelist
was changed. Work follows the reorganized `runtime/`, `tests/`, `tools/` layout.

## Evidence and diagnosis

Pins and all generated evidence are under `build/room051059-blocker/`.
`input-pins.json` records SHA256 for the exact `_059` packet and six sidecars;
`own056/input-pins.json` records the independently failing packet. USA ROM SHA256
is `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
The contemporaneous `_059` profile SHA256 is
`044e25516d1a2ae6384da27a2a331929e18f88ef645eef87005ca83ff0e90f67`;
serialized FNV is `eef8e3acedf3300d`. Complete input SHA256 is
`53d910819fb9118fd0689a4b59f76bea25a45f0d28ddf307607c911e5b540b7f`.
Settled module7/submodule0, plane0, Link(746,2696), BG scroll(626,2588),
actor scroll(626,2587), graphics bases16384, configured rise50; independent
authored stair rise24. Production replay verifies166816 BG/hash/flip samples.

The `_059` blocker is at screen(118,8), **not mostly offscreen**. All24 children
are observed. Both the original packet and production EndFrame already have
1296/1296 main pixels and1518/1518 scene pixels with authored height+normal and
base24. The fix leaves its complete serialized production packet and both-BG
map inventory byte-identical. Thus an exact `_059` recovery count above zero
would be false. All seven original files remain byte-identical to investigation
pins. Existing `build/room051059-repair/` is separate earlier wall-work evidence;
new blocker evidence is not written there.

The actual failure is before support: USA `Sprite_CorrectOamEntries` at86:FEBC
sets an entry's Y to$f0 when `uint16(screenY + 16) >= 256`. Mantle draw at9A:FCB3
calls it for all six16x16 entries. Prior clipping reconstruction required raw
wrapped Y for every entry, so a mantle origin above-16 failed the complete
producer signature even though its bottom remained visible. `_049` tested-8,
where no dispatch entry is producer-culled; that repair could not expose this.
Local metadata was present; no floor/semantic/elevation inference was missing.

`sprite_obstacle.h` now reproduces that exact unsigned correction in all four
producer comparisons. Logical child coordinates still come from the uniquely
matching latched actor/camera, never from interpreting$f0 as a sign bit. Complete
VRAM, all six dispatch signatures, exact owned visible child records,24 authored
members, unique actor/slots and real room support remain mandatory. Hidden
children remain identity-only evidence; no receivers are created. Overlapping
gameplay planes are not merged; the proven actor selects its own plane, with
unchanged contact checks and base24 in the reproduced cases.

## Production proof and regressions

- Exact `_056`: **618/618 visible main pixels** recover actual lighting-field
  authored heights at base24; the separate scene buffer recovers618 too. Prior
  implementation has zero absolute heights, but retains authored normals.
- Controlled `_059` camera crops (explicitly **not captured gameplay cameras**)
  shift only existing ownership/ink plus matched camera/OAM evidence. Production
  EndFrame reproduces actor Y-17/-24/-31: **601/378/44** visible pixels recover.
  Boundary controls Y-8/-16 preserve931/637 already resolved pixels. Exact `_059`
  preserves1296. `pixel-counts.txt`, `production.log`, `capture-test.log` retain
  counts. Camera ablation is opt-in `ALTTP_MANTLE_CAMERA_CROP=25|32|39` on the
  existing production capture probe; `ALTTP_TEST_PRIOR_MANTLE_CULLING` is a bounded
  prior-rule differential, not a runtime fallback.
- `sprite_obstacle_culling_capture_test.cpp` compares real before/after packets:
  no asset/instance addition, no receiver ownership change, immutable authored
  asset/placement arrays, verified base24 and real CPU lighting-field activation.
  Seven production packet pairs pass. `sprite_obstacle_capture_audit.cpp` is a
  read-only local-layer/support audit and never manufactures missing metadata.
- Expanded `sprite_obstacle_clipping_test.cpp`:160 signed-base/camera/canonical
  H/V representation cases;14 producer/ownership/metadata/support negatives.
  Includes-16/-17/-24/-31, two-edge clipping, duplicate actor/culled slot, incorrect
  raw versus corrected Y, falsely culled visible part and missing visible ink.
  Both normal and ASan/UBSan runs pass. Original sprite-obstacle regression passes.
- Current Application Support corpus:146 packets,58 support-restorable and88
  unsupported (missing evidence/profile, non-dungeon/unsettled/unsupported state).
  Only `_056` changes,0→618. Ten complete build bundles are supported and unchanged.
  Exact resolved-pixel-mask digests are unchanged for every other packet; no
  previous resolved-pixel losses. This is not320room/all-state coverage. Other
  producer families still need independently verified adapters.
- Curated quick gate16/16 passes; `git diff --check` passes. Parent performs the
  independent final full gate and native arm64 Debug build to avoid duplicate
  build/test execution. No interactive/GPU gameplay comparison is claimed here.

## Mandatory whole-room checks: not passed

Fresh pinned `_059` complete solver replay exactly matches the earlier complete
runtime JSON. Validation v2 was rerun using its complete ALTPPD1/ALTPAT1, observed
seed graph, pinned profile, runtime JSON and existing matching compiled map:
**exit1**,886/886 walkable supports, denominators[886,0],26/26 fallback borders,
588solved/72unknown BG2walls,4runtime structure conflicts, compiled2missing/
2mismatches (stair endpoints), overlap0. `validation-v2.json/svg/log` retain this.
The graph is complete static movement from the observed seed but contains **no
invented directed stair playback**; dynamic OBJ collision is not independently
validated by this static graph. Full both-BG/plane geometry remains incomplete.

Exact `_056` separately replays complete room inputs and its observed seed through
the same static movement predicate; its v2 gate also **exits1**:886/886 support,
624solved/36unknown BG2walls, existing conflicts remain, overlap0. No compiled map
was generated for `_056`; the map used for `_059` is not substituted for it.

Existing055/060/061 fixture runner was attempted. Historical generated obstacle
profile rejects duplicate group membership. Retrying with this pinned capture
profile gives055439/439support but nonzero gate,0600/711 with existing conflict,
and061 rejects diagonal preview lacking independent ROM proof. Logs are retained;
these are not passing room fixtures and no profile was edited to bypass them.

Smallest next action for this symptom: fresh matched gameplay capture with the
blocker above screenY-16 using the rebuilt app, comparing Height/Normals while
scrolling across the culling boundary. Do not call the room successful or request
complete-room approval while the independent room-wide gates remain nonzero.

## Independent final verification

Parent-session full portable plus production Metal gate passes **33/33** and
quick ASan/UBSan gate passes **16/16** against the final unchanged sources.
Results are under `build/room051059-blocker-parent-verification/` in
`20261023T164419Z-xqcdno5g/` and `20261023T164711Z-brjt838g/` respectively.
Both arm64 Debug and Release native builds succeed. Products:
`build/debug/Snes9x.app` and `build/release/Snes9x.app`. Logs:
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room051-059-blocker-debug.log`
and sibling `room051-059-blocker-release.log`. The current canonical profile and
all seven original own059 inputs remain byte-identical to the initial baseline.
The reorganized paths were preserved. These checks do not replace the outstanding
interactive scrolling check or turn failing room-v2 gates into approval.
