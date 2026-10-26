# Room051 capture104: follower crossing support

Room `0x051`, own capture104, Module7/submodule0: **reviewable but incomplete**.
The reported crossing is reproduced and its shared actor-support defect is fixed:
all128 Zelda OAM48/49 instance records change from base0 to24. Authored heights,
normals, transforms, context selection and pixel ownership are unchanged. Complete
room validation-v2 still exits1; this is not complete-room or gameplay approval.

## Exact evidence (not capture103 assumptions)

Input: `~/Library/Application Support/Snes9x/Remaster/frame_room051_104.s9xrmf`
and its own companions. Packet SHA256
`145b232dbb6080c3f1c8ce89f10e598fedb7417f250f5d772c55e124ed198ca2`;
own profile SHA256 `025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`,
serialized FNV `a0a42bc961c35a5e`. Own profile is pinned/copied separately in
`build/room051104/gate/replay-input-profile.toml`; canonical profile was not edited.
All companion pins: `build/room051104/pins.json`. ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Previous room061, Link761,2665 plane0; BG scroll641,2561; both graphics bases16384;
configured rise50. Complete WRAM/PPU maps agree.166250 BG observations across
main/sub/scene match exact maps, hashes, flips and canonical tile-pixel mapping.

This frame uses **OAM48 head,49 body**, not68/69:

| member | full OAM XY / size | name / flags | visible main AND scene | records / old→new base |
|--|--|--|--|--|
|head48|120,116 /16×16|20 /28 hex|121..134,119..131;135pixels|64 /0→24|
|body49|120,126 /16×16|22 /68 hex|121..135,130..141;129pixels|64 /0→24|

Head is unflipped, body H-flipped: valid animation DMA flag04 applies to the
lower half only. Both have palette4, dedicated VRAM children at
`0x8000+tileNumber*32`, matching final OAM. Their relative Y10 is valid producer
pose. Exact `Tagalong_Draw` / `Follower_AnimateMovement_preserved` semantics and
`kTagalongDmaAndFlags` prove independent half flips are legitimate. The existing
adapter already accepts this; signature/pose activation is NOT the104 failure.
Delayed follower plane is0 from attested flags20. Full body contact bottom eight
rows are screen134..141, x120..135 → room columns31..33, rows16..17.

First head instances1184/1185 are tiles32/33, hashes
`24202c48fc74a074`/`1c62268999534b0a`; lower head48/49 are
`740936d0f13ce373`/`cd8676b29869a0ed`. Head local heights10..25.
First body instances1266/1267 are tiles35/34,
`6c9e91315b731884`/`4e8feda0fca45ced`; body50/51 are
`a2306a861bdf9a34`/`ee2305bf24c1bfac`. Body local heights0..15.
All have active intrinsic heights/normals, no placement override and no context
override. Full signed64-sample arrays,192 normal bytes, all736 OBJ instances,
OAM identities, per-buffer pixel counts and bounds are recorded in
`final.actors.jsonl`/`final.log`. All736 records retain byte-equal geometry sources
and transforms; only the128 follower bases change. Main and scene refer to the
same264 follower pixels, sub to none. Fully occluded records receive the same
base24 as visible records; no stale scene-only follower was found.

## Root cause and shared repair

BG2 `(31,17)` really IS under this capture's lower body. Its
`833d6d68ba42b851` is authored Floor, signed local0, normal source intact, plane0
base24 and plane1 base0. Rows16/17 collision bytes are **0x22** on plane0, zero
on1. Reference `tile_detect.c:339` dispatches0x22 to `tiledetect_stair_tile`, not
ordinary collision-floor traversal. Runtime correctly has no ordinary movement
component there (`floorId=-1`), while the geometric floor seam solve already
establishes24. The actor consumer incorrectly required an ordinary movement ID
and ignored every cell in the contact, leaving the entire assembly at zero.
The actor did NOT copy the floor's local relief, and there was no per-half split.

`frame.h` now publishes a separate `semanticFloorSupport` fact only for exact
collision0x22 placements classified Floor with authored all-zero local height,
compatible plane ownership, and no obstacle/removable-support claim. It carries
the existing solved datum without inventing traversal/component ownership,
expanding movement or solving a new floor. Unknown/conflicting datums still veto
contact. Pits, other special handlers, solid blockers, nonzero/negative local
relief, unknown art, wall/obstacle art and other-plane claims cannot activate it.
The shared actor footprint consumer accepts this explicit support OR ordinary
movement support; no nearby/max floor selection, room/hash/camera whitelist or
profile change. Retained sprite-support cells carry the new fact too.

## Automated before/after and regressions

`room051104_follower_capture_probe.cpp` exercises production EndFrame against
own104, asserts all128 records0→24,264 main/scene pixels, immutable authored
heights/normals/transforms and absent overrides. Repeated unchanged-profile
publication/resume passes three times with exact serialized packet and complete
BG map invariance (inventory now includes the semantic-support bit).

`npc_transition_floor_test.cpp`: actual solver, both BGs/four H/V transforms,
special floor at24 without movement ID or denominator inflation, inactive plane,
signed relief/unknown/solid blocker/obstacle/plane-mask rejection.
Expanded `npc_floor_support_test.cpp`: signed/zero/upper bases, four regions and
effect layouts, transparent/clipped/scanline ownership, valid independent lower
H-flipped animation, independent delayed planes, partial corrupt child/DMA/
palette/flip rejection, unknown/conflicting support veto, no structural fallback,
and immutable local arrays. Both pass ASan/UBSan.

All exact capture probes below pass ASan/UBSan with current production:

*098 complete lower26..29 writer all H/V and overwrites;512 doorway scene pixels.
*099398 doorway scene pixels; own source identity and repeated resume retained.
*092467 stair and256 inner-jamb scene pixels, transformed partial-column negatives.
*061010896 doorway pixels /169178 matching identity observations.
*07116BG1 stair members,288 solved walls, BG2 overwrite rejection/arrays retained.
*103 all OAM68/69 remain24 with immutable arrays; no104 repair back-projected into103.

Fixture-free directional doorway, surviving column, stair-assembly, Link attachment
and scene-subsceen tests pass sanitizers. Independent Python complete-room
validator tests pass26/26. `git diff --check` passes. No aggregate suites, app
builds, macOS/profile edits or commits by this worker; parent owns integration.

## Available corpus, explicit unsupported scope

Union of available own Application Support complete inputs and retained previous
corpus:129 unique input/profile/state combinations,258 BG solves,8192 placed
BG cells each. Rooms002(4),011(1),012(6),022(1),041(2),051(90),061(25).
No missing companions or basic-input rejection. Exact names/combination pins in
`corpus-inputs.json`. All258 prior solver metric rows are exactly equal before/
after (`corpus-before.csv`, `corpus.csv`): bases, required/missing support,
structural conflicts, unknown walls, doorway/stair ownership/attachments unchanged.
This is an effective actor-consumer repair, not newly solved BG geometry.

Failure-class audit `transition-corpus.csv`:1536 collision0x22 layer/plane
placements across96 inputs (room0511440,room01296);1088 activate proven semantic
support, all solved (BG1720,BG2368, all room051, plane0).448 marker placements
are rejected by semantic/ownership requirements, NOT defaulted.90 inputs contain
accepted supports. No actor pose/scene claim for these full-room placements:
exact104 is the actor-change proof; exact103 is the no-change proof. Remaining
game rooms, states, special-handler families and NPC producer poses are untested/
unsupported; this is seven rooms, not320-room or all-state approval.

## Mandatory full-room v2 (still fails)

`gate/gate.json` records the actual validator command and exit1. Complete pinned
ALTPPD1,ALTPAT1, exact state, own generated profile, complete doorway graph,
runtime JSON and compiled ALTPHM3 are supplied. Reference directed stair edges
are transferred only on exact collision digest match (true). Graph denominator
76/0 supports,76/76 valid; runtime candidate denominator962/3720 with0/0 missing;
zero unknown walls but four structural conflicts remain. Doorway20/20 passes;
independent stair audit has zero issues. Four north doorway compiled-map
mismatches remain at `(30,6),(33,6)` (visible0 vs24 and plane0 unknown vs24).
The graph's empty plane1 is NOT a claim to gameplay coverage of the lower plane.
BG2 independent24/0 overlap is retained. No v2 waiver or approval is claimed.

Reproduction from repo root (output directories already created):

```sh
c++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  remaster/tests/room051104_follower_capture_probe.cpp -o build/room051104/actor-san
ALTTP_PROFILE_RESUME_REGRESSION=1 build/room051104/actor-san \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_104.s9xrmf" \
  build/room051104/final
python3 -B remaster/tools/ALttP/alttp_replay_live_height_evidence.py \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_104.s9xrmf" \
  --solver build/room051104/solver --output build/room051104/gate \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
python3 -B remaster/tools/ALttP/alttp_capture_complete_gate.py \
  --capture "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_104.s9xrmf" \
  --output build/room051104/gate \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
  --fixture build/room051_098_103/gate-103/live-graph.json
```

Review packet: `build/room051104/final.s9xrmf` (original untouched). Evidence is
automated capture/source comparison, not fresh interactive gameplay acceptance.
Smallest next actions: parent integrates narrow runner registration and app
builds; inspect a fresh104-like crossing in Height/Normals after relaunch.
Independently fix compiled structural-region projection/four existing conflicts,
rerun full-roomv2 to zero before room approval.

## Independent parent integration checks

Parent sanitizer replay reproduces all128follower records0→24 and264main/scene
references, immutable intrinsic arrays and repeated profile/resume packet/map
invariance: `build/room051104-parent/capture-sanitized.log`.
Independent complete-room validator tests pass26/26; inspector25checks and
tooling14tests pass. arm64 Release and Debug app builds succeed. The canonical
profile is byte-identical to the saved pre-repair snapshot, SHA256025e261c…62743d.
The fixture-free transition-floor regression is registered in the curated full
suite; aggregate result is recorded separately.

Parent independently reran the recorded full-roomv2command: exit1 remains;
`build/room051104-parent/validation-v2.log` retains the output. No nonzero gate,
unknown scope or lack of fresh interactive gameplay validation is waived.
Parent also passes the fixture-free transition-floor regression under ASan/UBSan,
and independently compares all258before/after BG corpus rows byte-for-value:
all pre-existing solver metrics remain identical. This repair changes effective
actor support consumption, not room floor solving or traversal denominators.

Final curated full CPU/Metal gate passes **45/45**:
`build/room051104-parent/full-tests/20261026T023941Z-bo22q3t3/results.json`.
Release/Debug builds pass; complete-room v2 remains exit1 and fresh interactive
crossing validation remains pending. No profile changes or room approval claimed.
