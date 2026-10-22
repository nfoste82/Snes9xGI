# Paused obstacle support and selected-member Z (October 21)

## Cause and scope

The native click-to-pause producer serialized `S9xRemasterCompletedFrame()` and
dispatched `openRemasterFrame:` on the resulting file. `spriteObstacleSupport` is
intentionally not serialized. Unlike normal EndFrame captures, the paused writer
did not save complete input sidecars. Opening then called editor metadata sync,
which reapplied the OBJ matcher without support and replaced valid live height
offsets with Unresolved. Purple Height was missing placement proof, not missing
local authored layers or a shader masking error.

The Z action deliberately wrote only `remasterObstacleMemberIndex`, the last
focused member, despite the independent scene/footprint multi-selection. It now
resolves the same exact selection as Support Contact and edits all selected Z.

No floor solver inference/rules or profile arrays were changed. Complete-room v2
approval is not claimed. User profile is read-only, SHA256
`c5206e0c07170406fcb513ac4bc89e47d1c510607ac631901b729ca113a4825b`, with 24 members,
six contacts and all member Z=0 in `ThroneRoomDoorBlocker`.

## Changes

- `macosx/mac-os.mm` / `macosx/mac-os.h`: dispatch a shared, value-owned copy of
  the exact producer packet. The private open overload accepts that packet and
  skips disk reconstruction; metadata sync consumes its retained support.
- `remaster/remaster.h`: pin complete room/attribute/graphics/template and actor
  inputs at the same EndFrame as the completed live packet. Save these on paused
  capture, not newer RAM. Profile copies are cached once per profile/settings
  revision and shared; no per-frame serialization or solver-map copy is added.
  Per-frame inputs are 102,988 bytes plus bounded value-only context. Solver-local
  pointers are cleared. BeginFrame revokes completed evidence.
- `remaster/capture_path.h`: orphan sidecars reserve capture names too, avoiding
  accidental overwrite when a main packet has been deleted.
- `remaster/obstacle_group_editor.h`: transactional selected-member Z/name edit,
  signed range validation and unchanged contacts/unselected members. Native UI
  shows Mixed for unequal Z and supports blank mixed rename without flattening.
  Missing selected repeats require explicit completion confirmation before commit.
- Production regression `remaster/paused_obstacle_test.cpp` runs EndFrame, proves
  the old support-stripping failure, tests immediate same-packet handoff and disk
  sidecar reopening, compares all height/surface/normal fields, applies Z/undo
  repeatedly and changes live inputs to prove retained-state isolation.

## Checks / remaining acceptance

The synthetic EndFrame test passes 128 authored OBJ height/normal pixels with
ASan/UBSan. All 16 source routing/layout checks and six runner contract tests pass.
Debug and Release arm64 builds pass (`build/paused-obstacle-{debug,release}.log`).
The actual Room051 24-occurrence editor fixture and signed Z transaction tests
pass. The full gate is now 21 portable cases plus Metal (22 total), including
`paused_obstacle` in quick. Final full ASan/UBSan + Metal gate passed **22/22** in
156.77 seconds. Result:
`build/paused-obstacle-validation/20261021T223858Z-bvzgh6sp/results.json`.
The earlier run was correctly invalidated by a concurrent final pointer-clearing
header edit; it is not counted as passing evidence. Native readback follows.
Capture-path orphan-sidecar preservation passes ASan/UBSan. Profile merge,
lighting-field dirty-row cache, and three Profile Controls layout checks pass.

Actual failing `frame_room051_019.s9xrmf` reproduces the lifecycle issue with the
unchanged current 24-member profile: 16 identities have both local layers,
192 records are initially not unresolved, but no sidecars exist. Restore returns
false; profile/matcher refresh becomes Unresolved across all 1,518 blocker pixels,
while 1,518 authored normals remain. No historical evidence was invented to
force this old capture resolved.

Old captures lacking complete sidecars cannot retrospectively gain producer
proof; reopening them remains explicitly unresolved. Interactive user click,
bulk edit, Undo and Save acceptance is still required. Existing runtime assembly
ambiguity/support rejection remains unchanged; no arbitrary unmasking is used.
