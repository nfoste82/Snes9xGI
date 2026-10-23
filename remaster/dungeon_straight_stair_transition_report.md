# Straight inter-room stairs: actual lifecycle repair (2026-10-22)

Room061 <-> Room051 default reference state: **reviewable but incomplete**.

## Incoming effective-height follow-up (2026-10-22)

User confirmed outgoing retention but reported incoming art still snapping at
scroll completion. Reproduced a separate **support-plane selection** failure,
not missing maps, collision-mode gating, HUD filtering or graphics upload.
The destination map existed throughout scroll, but its solve enumerated the
outgoing `$ee` actor plane. The actual straight-stair handler does not change
`$ee` until phase11. `Module07_11_0B_PrepDestination` assigns
`kTeleportPitLevel2[cur_staircase_plane]`, table `{0,0,1}`, with the destination
selector already latched at `$048a` by stair detection. In this real route,
061->051 scroll retains actor plane0 but destination uses1; return scroll
retains1 but destination uses0. Wrong-plane solves omit support and change
structural bases. Before repair, frame556 incoming061 BG2 cell32 applied24
instead of the settled destination base0 (matching producer/word/flips).

The shared runtime now records `$048a` and selects the proven destination plane
for **geometry solving/cache identity only**, phases3..11 of handlers18/19.
The actor and rendering context keep the actual live `$ee`; source geometry
stays independently frozen. Unknown/invalid selectors remain unknown (255),
never guessed from submodule/direction. No authored arrays/profiles were edited.
Native dungeon sidecars now retain `straight_stair_plane`; old evidence lacks
this field and cannot reproduce this early-plane proof.

Regression now plays the actual801 snapshots twice: first collects each
destination's first fully settled solve, then compares scroll-time production
EndFrame output against that independent-time oracle. It checks applied base
AND the selected placement-local height sample (ramps/mixed floors), both BGs,
with final word/hash/flip qualification. It also compares complete solved
per-plane bases against the oracle, without collapsing the planes.

Result: all120 scroll frames pass; BG1/BG2 incoming effective sample denominators
3,383,296 / 3,123,456, including1,157,888 / 1,416,416 nonzero bases.
Plane0/1 comparison denominators: BG1 `[416520,120]`, BG2 `[273360,260280]`.
Outgoing3,259,696 nonzero-floor checks still pass. These are repeated sample
checks, not unique cells or approved coverage. Disabling destination-plane
evidence via `ALTTP_TEST_NO_STAIR_DESTINATION_PLANE=1` fails at frame552,
incoming061 BG1 plane0 cell0, actual unknown255 versus settled24. The earlier
effective-pixel diagnostic also failed at frame556 as described above.

Pinned runtime SHA256:
`5dcee9da27fc8e5109450e7f8aae267a15b3daecef2c40da5db0e92b2b366b4f`.
072 profile SHA256:
`3704edce2c4aaa8ac1591f4a8b1cd65f4130edac9f237afb9fa29fbe9c7a8e54`.
ROM/context/entrance/rise unchanged from below. Available actual transition
corpus checked is one800-frame N400,S400 roundtrip, two destination states,
both BGs and both gameplay planes; **not all320 rooms or all stair states**.

Narrow cache/ordinary-scroll and independent collision-builder tests pass;
quick suite passes14/14. Supplied072 exact outgoing capture ablation also passes
with the new native evidence field retained (`incoming-capture072.log`).
arm64 macOS Debug and Release full builds passed (`debug-incoming-build.log`,
`incoming-release-build.log`). Embedded Release framework matches the build,
SHA256 `6ce7216e8f4387ed16105557c7f7c240fbcbe51dc8425cfa1f10dd3e4a80a3a9`.
Parent independently reran the incoming route regression with the same passing
denominators. Mandatory v2 rerun used the existing complete pinned
051040/061071 linked snapshots/attributes/graphs/profiles/runtime/maps, not a
partial packet: artifacts `incoming-validation-{051,061}.*` under the route
directory. 051 exits1 (360/360 supports,4/4 fallbacks,564 solved/54 unknown walls,
existing conflicts/map mismatches);061 exits2, missing independent diagonal
ROM proof at BG2(2,38). These are not fresh072 or real-route room-wide approval.
No new conflicts were hidden or geometry gate weakened. Fully approved room
coverage remains blocked. No new GUI traversal/compositing comparison was
performed, so visible success is **unverified**. Next: fresh gameplay capture
in both scroll directions using `build/release/Snes9x.app`; separately
close the complete-room provenance/structural gates.

## Root cause

The previous change repaired only Module07/submodule02 ordinary door scrolling.
The throne-room stair connection actually executes decimal submodules18/19
(`$12/$13`, `Module07_11_StraightInterroomStairs`). Actual reference execution
N400,S400 from entrance4 produced **zero submodule02 frames**,342 straight-stair
frames,120 camera-scroll frames. Thus collision preparation, destination
publication, source retention and screen room ownership all bypassed the previous
fix. This is not a wrong-app diagnosis; the supplied072 packet is from Release.

The new handler-specific rules retain outgoing both-BG geometry from detection,
including the45-frame pre-load fade; A0 already names destination then but the
origin and art still belong to source. Room construction returns at phase3, not
ordinary-door phase2. Read-only collision completion and destination publication
start there. Phase10 scrolls; phase11 switches Link plane and prepares destination.
Ownership uses the real destination origin on the transition axis through10,
and destination only from11. Each usage still requires matching final word,
hash and flip. Stairs do **not** gain ordinary level-door entry anchors.
Unsupported spiral/fall/warp handlers remain excluded. No profile edits.

## Actual traversal evidence

The diagnostic adapter now optionally exports per-frame full WRAM, actual PPU
VRAM and scroll/base registers **before** inspection's synthetic camera jump and
tilemap synchronization. `ALTTP_ROUTE_ONLY=1` avoids trying to reuse original-room
provenance after real traversal. The production EndFrame regression consumes
801 actual snapshots (frame0+800 movement frames), pinned072 profile and ROM
templates. It projects actual PPU BG words; it does NOT emulate final winning
pixel compositing, actors, palette visibility or Snes9x CPU execution.

Optimized/NDEBUG test passed342 handler frames,248 loaded-destination frames,
120 scroll frames,8,888,320 source-coordinate checks and3,259,696 producer-qualified
nonzero outgoing floor-base checks across both BGs. Reverting the new handler
recognition in an isolated header copy fails immediately: `actual stair lost
outgoing geometry before load`. Existing ordinary departure and independent
attribute-builder regressions also pass. The exact settled072 gameplay replay
passes169,130 matched BG pixel usages with profile FNV e6fd563fa0a85504.
arm64 Debug and Release full app builds succeed; git diff --check passes.
Release app: `build/release/Snes9x.app`. Embedded framework matches the build
product, SHA256 `af031438ea878e68c618a7884af8d2f46bf936121b35eda5bb1087e0dcd1b8ad`.

## Limits / gates

This checks one800-frame roundtrip, two rooms, one entrance/default reference
state/theme, ROM SHA25666871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb.
It does not attest every staircase destination/state across320 rooms.072 is a
settled packet, not a recording of this player's scrolling lifecycle. No fresh
interactive Snes9x traversal has been observed after this repair. The smallest
next action is a rebuilt Release traversal of the throne-room stairs, including
both directions; do not claim visible success from the automated projection.

Mandatory complete-room v2 reruns with existing full linked inputs, profiles,
runtime solutions and compiled maps remain blocked, exit2:061 directional
diagonal preview(2,38) lacks independent ROM proof;051 corner lacks both surviving
ROM branches. These pre-existing artifacts are not a fresh072 room-wide approval.
072 sidecar reports1570/1570 plane0 reachable supports, no plane1 denominator,
962/0 solved/unknown walls and2 structural conflicts; these are diagnostic counts,
not a full-room success. The two gameplay planes and both BG maps remain separate.
No solver geometry rule, authored local height or normal data was changed.

## Reproduction

Artifacts: `build/transition-lifecycle-route/` (ignored).

```sh
mkdir -p build/transition-lifecycle-route
python3 tools/ALttP/alttp_build_route_adapter.py \
  --engine build/remaster-geometry/room-preview-engine \
  --output build/transition-lifecycle-route
# Run with the engine directory as cwd (absolute binary/output paths):
ALTTP_ROUTE_ALLOW_ROOM_TRANSITIONS=1 ALTTP_ROUTE_ONLY=1 \
ALTTP_ROUTE_RUNTIME=<absolute-output>/runtime.bin \
  <absolute-output>/route-renderer --linked 0x61 <absolute-output>/actual.raw \
  4 0 0 0 N400,S400
c++ -std=c++17 -O2 -DNDEBUG -pthread \
  remaster/dungeon_straight_stair_route_test.cpp \
  -o build/transition-lifecycle-route/test
build/transition-lifecycle-route/test \
  build/transition-lifecycle-route/runtime.bin \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room061_072.s9xrmf.profile.toml" \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room061_072.s9xrmf.dungeon-templates.bin"
```
