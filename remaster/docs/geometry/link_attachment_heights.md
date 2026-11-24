# Link attachment floor offsets

## Overworld per-pixel support

`overworld_actor_support.h` samples the independently observed BG2 pixel at the
existing grounded Link anchor `(linkX+8, linkY+20)` on each completed frame. It
requires verified ALTTP overworld identity, matching world-coordinate placement,
enabled saved offsets, and explicit Floor metadata. Walls/obstacles, missing
observations and unresolved geometry do not supply support; no nearest/highest
floor search or previous-frame base is used.

The runtime uses the authored asset role even if the asset is marked
context-dependent. Named tile contexts can replace local relief but do not
override the role used to decide whether the pixel supports Link.

Coordinate publication includes all four independently observed BG planes, not
just main/sub/scene composites. Otherwise Link can completely cover a floor tile,
leaving that observed floor without a coordinate/base and resetting actor support
for particular movement/animation frames. All-observation coordinate conflicts
still reject; editor picking remains visible-only. Capture
`frame_overworld_x02ed_y06c8.s9xrmf` reproduces this at BG2 coordinate(94,219):
the original floor occurrence592 has no coordinate/status; publication from its
independent observations restores support40. Capture replay regression recovers
effective raster scroll from existing verified coordinate observations rather
than assuming WRAM scroll copies exactly match the PPU raster. The previous
capture still samples39.7204. Both pass ASan/UBSan; context publication regression
and Release build pass. Fresh live movement confirmation remains pending.

The composed support is local floor height + scalar + world-oriented pixel height
adjustment. Normal offsets are shading data, not height inputs. The attested Link
OAM block (56–67 or100–111, identified by dedicated body DMA) shares that base
across body, equipment and shadow. Fractional support uses a constant float terrain
height translation on those OBJ members; authored sprite arrays and normal
orientation remain unchanged. Held-lamp floor height consumes the same sample.
This changes remaster geometry only, not stock screen positions or gameplay.

Regression: `overworld_link_support_test.cpp`, including optional supplied capture
and profile arguments. `frame_overworld_x0126_y067f.s9xrmf` with its sidecar-attested
Link/camera coordinates yields support39.7204. Per-pixel movement, artwork flip
independence, signed support, shared fractional assembly height and wall/indoor/
unverified rejection pass ASan/UBSan. Existing dungeon Link attachment test passes;
Release app builds. Fresh gameplay/jump traversal, GPU lighting and paused-editor
regrounding remain unverified; replay does not infer actor position from filename.

The dungeon OBJ application path previously sampled each OAM entry's visible
bottom independently, allowing Link's head, shield, sword and ground shadow to
receive different bases at floor boundaries.

`S9xRemasterApplyDungeonHeightMap` now recognizes the active Link OAM block using
the verified ALTTP ROM and a body-DMA address in `[0x8000,0x8400)`. The game's
`LinkOam_Main` reserves twelve entries beginning at byte offset `0xe0` or `0x190`
(slots56–67 or100–111). Its body, sword, shield, shadow and visibility-clear
tables all use that block. Conflicting observations of both blocks reject
ownership; shared artwork and proximity alone never identify an attachment.

The whole block receives one support offset at `Link+(8,20)` on Link's gameplay
plane, matching the existing held-lamp support anchor. This replaces per-part
floor sampling, not authored relief or animation geometry. Unknown support does
not fall back to an unrelated cell beneath a part. Subscreen-only and clipped
members inherit too. BG1 cannot add the offset a second time. Other actors and
the producer-derived carried/thrown-object path are unchanged.

No profile, capture schema, room classification or floor solving changed. This
is an actor-base inheritance repair, not approval of complete-room coverage.
Overworld floor support uses a separate path, not this dungeon map.
Jump displacement/body geometry are unchanged. Intra-room stair support is now
interpolated as described below; authored tread/rail geometry is unchanged.

Portable regression: `remaster/tests/link_attachment_height_test.cpp` (C++17). Covers
both blocks/planes, signed/zero/raised/unknown bases, mixed priorities, clipped
and sub-only parts, preserved local arrays, unrelated artwork, absent/ambiguous
ownership, legacy maps and independent BG1 application. Fresh gameplay visual
confirmation remains required, particularly at stair and doorway transitions.

Validation: attachment regression passed with ASan/UBSan; existing terrain
pickup/carried/thrown/ambiguity regression passed; arm64 Debug app build and
`git diff --check` passed. Specialist delegation was attempted twice but the
harness rejected its unavailable model variant; ownership was checked directly
against the local reference engine's `src/player_oam.c` instead.

## Continuous stair support

`S9xRemasterLinkFloorHeight` interpolates the shared actor base along a recognized
4x4 intra-room staircase, using Link's existing `(8,20)` foot anchor. North and
south landing heights come from both solved landing columns on the appropriate
gameplay planes (high plane0/low plane1 for plane-changing stairs; current plane
for same-plane stairs). The 32-pixel run blends the two actual signed datums,
rounded to the existing integral placement-height units. It is independent of
direction, camera scroll, and the timing of the game's collision-plane switch.
The held lamp consumes the same helper. Unknown/inconsistent landing columns,
ambiguous overlapping ramps, unverified ROMs, unrelated transitions and legacy
maps keep their existing support behavior. No remembered traversal state is used,
so defrosts and reversal cannot retain stale progress.

The local reference engine's `src/dungeon.c` functions
`Module07_08_00_InitStairs` / `Module07_08_01_ClimbStairs` and their `10` counterparts
confirm that the collision plane changes at opposite endpoints depending on
traversal direction. Sampling that plane alone cannot produce a continuous rise.

`remaster/tests/link_stair_height_test.cpp` checks every pixel in both directions,
both stair orientations, same/changing planes, both Link OAM blocks, signed
landings, clipped/sub-only equipment, unchanged local arrays, BG1 double-apply
rejection and unsupported-input fallbacks. This changes actor support only, not
room classification, solved maps, authored layers or complete-room coverage.
It does not add interpolation for inter-room/spiral stair transitions, whose
destination-room support requires different evidence.

Validation: new stair regression and existing Link attachment/NPC support tests
passed with ASan/UBSan; dialog EndFrame and held-lamp regressions passed; arm64
Debug app built at `build/debug/Snes9x.app`. Fresh interactive stair traversal and
complete-room/corpus validation were not performed. No room-map coverage approval
is claimed; this test scope verifies actor interpolation from supplied solves.
