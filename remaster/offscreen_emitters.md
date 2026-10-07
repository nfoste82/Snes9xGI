# Completed-room emitter capture

`gfx.cpp` supplies verified USA ALTTP state at the completed display frame.
`S9xRemasterEndFrame` appends physical room sources and the held lamp before
animation capture, serialization, and live-frame publication. `frame.lights`
already serializes signed screen XY and physical Z/radius in source-pixel units;
intensity 25 is standard emission and RGB is authored sRGB. No schema change.

## Proven memory contract

The local reference engine's `variables.h` identifies `dung_bg2` at WRAM
`$2000` and `dung_bg1` at `$4000`: each is a complete row-major 64×64 word map.
`DrawObjects_LightableTorch` uses the unlit/lit ROM templates `$ec2/$eca`.
`RoomDraw_AdjustTorchLightingChange` writes the replacement 2×2 words back to
WRAM `$2000` (the `overworld_tileattr` alias), then schedules an overlay DMA.
Thus current completed-frame WRAM, not accumulated viewport observations, is
the source of placements and lit/extinguished state. Current VRAM 4bpp graphics
provide canonical content identity, including animated tile payload changes.
PPU screen blocks are streamed quadrants, not a complete room inventory.

`offscreen_emitters.h` decodes each referenced VRAM tile once per invocation.
There is no persistent cache. Eligibility requires verified ROM, settled indoor
module 7/submodule 0/subsubmodule 0, collision mode 0, valid room and normal
256-wide packet. Missing/conflicting producer identity or unknown/missing
existing placement bases suppress a source. Both tile flips are respected.
Equal paired BG2/BG1 writes sharing a graphics base are one physical fixture.
BG1 otherwise consumes its independent existing height map.

Only authored omnidirectional/depth-volume emission can enter sphere transport.
Missing offscreen pixels are reduced per placement to a radiance-weighted
representative with equal sphere emitting area. In-viewport pixels always stay
with the authoritative visible emitter path, including pixels occluded by
on-screen artwork. A partially entering flame contributes only its missing
pixels through the adapter. No historical room lights survive a transition,
defrost, extinction, graphics animation or profile edit.

With Representative Emitters enabled, reciprocal `combine_emitters` identities
join cardinally adjacent missing-pixel placements within one BG plane before
sphere reduction. One connected fixture becomes one sphere with summed emitting
area and linear RGB power and intensity-weighted position/height. Combining in
the inspector authors all known animation variants, so the current VRAM identity
keeps its links as the fixture animates. The approximation can change near-field
shadows; visible and missing-pixel sources are still separate. Off preserves
one sphere per placement. Different directional modes, disconnected copies and
one-way links do not merge. No frame/profile schema change.

## Held lamp

The completed-frame hook reads `$0458`, Link Y/X `$20/$22`, facing `$2f`, and
submodule `$11`. Screen coordinates subtract BG2 scene scroll (including the
renderer V-offset scanline bias). The existing actor-plane solve at Link's
feet supplies physical floor Z before `S9xRemasterHeldLampLight` appends a light.
The helper's default relative floor is retained if no resolved actor base is
available. Lamp settings currently use the portable helper defaults.

## Frontend integration and scene capture

The lamp agent's macOS implementation consumes all `frame.lights` as physical
Metal sphere sources, so these sources require no additional discovery in
`mac-render.mm`. Its diagnostic source signature includes every light's position,
radius, RGB and intensity. Sources are rebuilt from each completed frame.

The pre-cone scene pass and optional v28 capture appendix are implemented;
see [lamp_scene.md](lamp_scene.md). GI uses independent BG2/BG3/OBJ colors and
ownership, while Original/HUD retain stock RGB. Existing v21 lamp captures
need recapture because their black pixels contain no recoverable raw palette.

## Limits and verification

The source reduction is a physical sphere approximation, not an exact extension
of planar/volume pixel transport. Directional surface sources are excluded.
Occluder geometry still covers the viewport only: room walls between a source
outside the viewport and its entry edge are not represented. Full-room occluder
transport remains necessary for exact through-door/wall visibility. Object/OAM
sources, overworlds, transitions and unverified ROM revisions are not covered.
No room-height generation, validation or approval is performed here.

`offscreen_emitters_test.cpp` verifies first-frame discovery without visible
history, canonical decoding/flips, duplicate suppression, signed left/right XY,
existing base composition, color calibration, visible suppression, transition
and construction rejection, profile/animation/extinction freshness, unknown
bases, directional rejection and capture round trip. It passes optimized and
ASan/UBSan builds. `held_lamp_test.cpp` also passes.

The core `gfx.cpp` passes `-std=c++17 -D__MACOSX__ -DHAVE_STDINT_H
-fsyntax-only` (existing `sprintf` deprecation warnings). `git diff --check`
passes. The scene-capture continuation repaired the shared smoke test's v27
metadata erasure (`+266`) and stale legacy fixture assumptions; it now passes.
The integrated arm64 Debug Xcode build and Metal harness (1,308 checks) pass.
Live GUI appearance and performance validation have not been performed.
