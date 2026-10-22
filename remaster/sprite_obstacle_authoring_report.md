# Movable OBJ progressive authoring repair (October 21)

## Current profile / actual presentation follow-up (14:35 capture010)

This supersedes the old profile inventory below. Disk profile
`Profiles/ALttP/alttp-profile.toml`, SHA256
`5ef31411c5473070ff158af2abdb9239f4af116c816d34714e5a0f0837227c41`,
has schema30 `ThroneRoomDoorBlocker`, spriteAssembly=true, **16 members**, and
all16identities have authored height and normal_xyz. No authored file was edited.
The object has24physical occurrences/192scanline records. The saved sparse motif
matches16/128, leaving8/64unclaimed; its row2x3contact is interior to the actual
object. Thus the exact24-child producer check cannot attest this group. This is
not missing local authoring and not a reason to guess floor0.

Production proof (not resolver-only): opt-in native app probe calls the real
`DrawRemasterFrame` → `S9xPutImageMetal`, resource-slot field/upload caches, compiled
app Metal kernels, synchronous GPU completion and RGBA8 output readback. Capture010
with unchanged current profile verifies **1518/1518 authored-normal pixels** and
**1518/1518 unresolved-height pixels** (RGB56,0,71 ±1). Normal→Height→Normal repeats
exercise cache reuse. A separate deterministic live freeze005 run traverses actual
EndFrame→presentation and verifies **1486/1486 normals** and **1486/1486 unresolved
heights**, with Link hiding32pixels. The latest reported missing-normal symptom
is **not reproduced by the current fresh app binary**. Do not label it fixed by a
new shader change: no such change was necessary. User's old process was not stopped.

New live evidence is contemporaneous, complete and producer-backed:
`build/room051-live-normals/live-evidence_room051.s9xrmf` plus profile, dungeon,
templates, terrain and sprite sidecars. Unchanged16-member group conflicts. Explicit
TEST-ONLY in-memory completion to24members/sixsouthcontacts resolves actor0/plane0/
base50, claims192/192records and yields1486/1486effective-height and normal pixels.
The native compiled-app probe also verifies all1486resolved Height and Normal pixels
against the exact authored fields. These test proposals never write a profile.

Repair is intentionally reviewable: Metadata now offers **Complete Footprint…**
for a uniquely matched sprite motif. Confirmation adds observed adjacent repeats,
preserves existing coordinates/flips/Z and all asset arrays, resets contacts to
the complete south boundary, and registers Undo. No runtime identity flood-fill
or silent profile migration. Runtime rejects a contact with an omitted observed
member immediately below it; an interior edge cannot vote as physical floor.
Metadata reports actual support status/missing evidence, not a generic "resume"
instruction or whole-group missing-height claim.

Trace findings: both live and replay use `DrawRemasterFrame`, the same field cache,
mesh (authored normals protected), RG32Float heights/RGBA32Float normals uploads,
and `remasterDirectLighting` views5/7. Tile Types/FloorWalls intentionally disable
lighting; mesh excludes unresolved OBJ absolute geometry. Inspector local layers
are not proof of placement. Capture009/010have NO sidecars: paused click/menu path
in `mac-os.mm` serializes only the packet, unlike running EndFrame capture. This
known evidence gap remains; those files cannot retrospectively resolve floor bases.
Use a running capture for contemporaneous complete evidence until paused capture
retains a complete value-owned input snapshot, not newer RAM or another session.

Pinned complete-room validation v2 on the NEW live evidence still exits1:
success=false, reachable denominators[76,0], walkable76/76, fallback10/10,
walls564solved/24unknown, directional continuity2318missing/7008edges,
BG obstacles32solved/48missing of80required, one spritegroup explicitly excluded.
Artifacts `build/room051-native-gate/validation-v2.json/.svg`, gate/manifest. This
is NOT full-room/game-wide height approval or an assertion of authored shape quality.

Regression/build results:
- Sprite/obstacle-editor synthetic and actual003/010 tests, ASan+UBSan passed;
  completion tests preserve signed Z across all H/V transforms.
- `--metal` now always runs `remaster-debug-pixels-test.h`: production field cache
  and production kernel RGBA8 readback,4608channels, all mirrors and
  Resolved/Unresolved/Conflict transitions. Existing suite1393/1393passed.
- Quick/full portable gates now include sprite_obstacle and obstacle_group_editor;
  runner selection assertions/fingerprints include the added shader regression.
- ASan O0 small-stack failure was a237728-byte EndFrame frame plus270656-byte
  solver frame. A fallback `RemasterDungeonHeightMap{}` temporary accounted for
  large unused stack. Heap construction reduces EndFrame to40000bytes and
  preserves behavior; both512/544KiB ASan
  tests pass without raising the budget.
- Debug/Release build logs `build/sprite-debug-probe-build.log` and
  `build/sprite-debug-probe-release.log`. Native logs/readbacks `build/room051-native010*`,
  `build/room051-native-completed*`, `build/room051-live-{normals,height}/`.
- Fresh human click/confirm/Undo/Save review remains pending. Source layout tests
  and six runner contract tests pass. User profile SHA remains unchanged.

Viewable PNG evidence: `build/room051-native010.normals.png`,
`build/room051-native010.height.png`, `build/room051-native-completed.normals.png`,
`build/room051-native-completed.height.png`. Release independently verifies
capture010's1518normal/unknown-height pixels (`room051-native010-release*`).
The shader's view7reads surface normals directly; no height-known mask suppresses
them. Captured flat-looking authored directions are not silently reshaped.

## Reproduced failures

`sprite_obstacle.h` previously combined `missingLocalInstances` with unknown
floor support. One missing local source made the whole group Unresolved; the
shared frame resolver then removed height, normals and occlusion. Editor metadata
sync refreshed profile arrays but retained that status without rerunning support.

Observed latest capture `frame_room051_008.s9xrmf`: 16 unique mantle identities,
16 captured height layers, zero base normal layers, 192 unresolved records.
`007`/`008` have no complete room/terrain/actor sidecars. Only `003` has complete
room inputs; it lacks contemporaneous mantle actor evidence. Current requested
`Profiles/ALttP/alttp-profile.toml` has no sprite assembly group and zero authored
local heights for these identities. It is NOT the same authoring snapshot as008.
No profile, backup or user-authored arrays were edited.

## Repair

- Floor support resolves independently of local source availability. Diagnostics
  keep the missing-local denominator; Resolved means support/ownership, not full
  local height coverage. OBJ members lacking relief produce no flat shell merely
  because their floor base is nonzero. Authored normals still reach lighting.
- Unresolved/conflicting OBJ support retains local normal directions but never
  absolute heights/occlusion. BG unresolved behavior remains strict. Context
  layers cannot bypass unresolved absolute placement.
- Each current supported packet retains immutable, value-owned movement-floor
  cells/actor observations, not live RAM or solve-local pointers. Paused painting,
  save and metadata sync reapply the shared matcher using that packet's support.
  Invalid live evidence revokes the snapshot. Captures do not serialize pointers.
- Replay reader checks complete ALTPDI1/ALTPDT1/profile pins, verified ROM, room,
  every available BG ownership/flip/canonical-camera observation and matching
  terrain/actor sidecars. It solves both BGs in order and retains support. Missing
  or mismatched sidecars do not invent a plane/base or borrow the active session.

## Checks

Portable `c++ -std=c++17 -O2` tests passed: sprite obstacle synthetic tests plus
capture008 matching; ASan/UBSan sprite tests; lighting field cache; authored BG
obstacles; profile merge; context binding; inspector heights; obstacle editor
using its actual003 fixture (008 has a different visible-pixel count and must not
be substituted for that fixture's hardcoded1518 assertion).

Synthetic progressive tests span all H/V transforms and bases0/50/-8, partial
heights, normals-only members, paint refresh, missing/conflicting floor support,
snapshot revocation and context-layer rejection on unresolved placement.

Research-only copied003 plus existing independent freeze005 producer evidence
was written under temporary `room051-authoring-research/`, not native sidecars.
`ALTTP_SPRITE_TEST_LAYERS=1 build/sprite-obstacle-authoring-test <research-copy>
<profile>` explicitly supplies test-only partial local arrays; never authors assets.
Pinned historical profile: plane0/base50,1518pixels claimed,814height,1518normals,
704without local relief,88missing-local records. Replay serialization/restoration
and absent-sidecar rejection passed. Current disk profile on the same research
inputs solves base24, demonstrating that configured rise50 is not a floor datum.
The separate original producer probe also passed its exact actor/motion/negative
controls. Neither result is approval of historical actor ownership or authored
object shape.

Pinned complete-room validation v2 rerun on003 retains exit1/success=false,
reachable denominators[76,0]. It is not room-height approval; the existing room
wall/base/conflict/obstacle failures and missing directed stair traversal remain.
Artifacts: temporary `room051-sprite/gate.json`, `validation-v2.json/.svg`.

Native Debug and Release arm64 build logs: `build/sprite-authoring-debug.log` and
`build/sprite-authoring-release.log`. Fresh interactive paint/save/relaunch and
contemporaneous sidecar-backed movable-actor gameplay verification remain pending.

Latest008 with explicit test-only layers and NO sidecars independently proves
1518 authored-normal pixels remain visible,814 local-height metadata pixels do
not become absolute geometry, and support stays unresolved. The real008 has zero
captured base normal layers; no claim that test normals were user-authored.
