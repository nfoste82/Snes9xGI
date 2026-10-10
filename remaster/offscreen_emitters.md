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
representative with equal sphere emitting area. HUD-covered in-viewport pixels
instead populate `frame.hiddenEmitters`: exact pixel-center XY, existing base plus
local Z, transformed authored normal, RGBA and depth/direction. The source-only
list does not write scene colors, owners, receivers or occluders. Directional
planar emission is supported here too. Uncovered in-viewport emission remains
exclusively on the visible path; world-artwork coverage remains excluded.
No historical room lights survive a transition,
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

HUD-overlap regressions additionally cover partial/full BG3 coverage, authored UI
sprites, independent-scene packets, source area/power, current animation/extinction,
world-artwork exclusion and capture/replay. The optimized emitter and held-lamp
tests, arm64 Debug Xcode build and `git diff --check` passed for this repair.
The user confirmed source-only HUD-overlap lighting looks good in the GUI.

The HUD-independent ordinary-room scene experiment was reverted because
it changed room061 layer composition, hiding pedestals/doors and changing floor
colors. Source-only recovery replaces the original HUD sphere approximation. See
[lamp_scene.md](lamp_scene.md#reverted-ordinary-room-world-pass).

## Source-only HUD transport

`hidden_emitter_samples.h` expands each hidden pixel into the same one planar or
four quarter-power depth strata consumed by prepared Direct. Metal appends these
samples after visible/clustered records, with a sentinel preventing patch-member
exclusion. Budgeted Direct collapses four depth strata to one full-power record
with its depth extent, evaluating the same four form factors and tracing only
selected strata. Serial, batched, clustered and budgeted transport consume these
sources. HUD participation and
stock RGB remain unchanged; no hidden self-emission is drawn over HUD ink.
Offscreen sources retain their existing sphere approximation.

Optional `HES1` trailing capture appendix retains these source-only pixels. Each
33-byte record is seven finite floats (XYZ, depth, normal XYZ), four emission bytes
and a strict omnidirectional byte. Coordinates/count/depth/emission are validated;
truncation/duplicate appendices fail. It coexists with SCN1, and old packets with no
appendix keep decoding. Readers predating HES1 reject new packets containing it.

Hidden pixels are not reconstructed as world geometry: their endpoint uses the
known zero-thickness epsilon, and absent authored normals default to +Z. Visible
derived shell normals/occluders and representative clustering can therefore still
change near-field shadows across coverage; this repair preserves individual
emission positions/depth/radiance, not hidden world geometry or identical cluster
approximations. Current existing placement sources/bases are consumed, not solved.

Checks: optimized source tests (flips/normals, planar/depth calibration, partial
coverage, current extinction/animation, serialization/malformed input), and 1,370
Metal checks pass. A controlled GPU regression equates visible and hidden depth
transport and verifies batching, clustering sentinel and HUD receiver exclusion.
Immutable room060 052/053 replay recovers 96/0 HUD pixels (384/0 depth samples),
and room061 051 recovers none; stock RGB/main/sub ownership and all visible fields
are identical before/after recovery. Actual covered replay has finite, nonzero
Direct output. The user accepted source-only lighting appearance, then reported
17.1 FPS / 62.4 ms Direct with HUD recovery active. See the performance repair below.
The shared smoke test fails its stale schema-v16 conversion fixture at line
739, which does not strip the newer v30 palette payload; dedicated source capture
round trips and the reported v30 captures pass.

The core `gfx.cpp` passes `-std=c++17 -D__MACOSX__ -DHAVE_STDINT_H
-fsyntax-only` (existing `sprintf` deprecation warnings). `git diff --check`
passes. The scene-capture continuation repaired the shared smoke test's v27
metadata erasure (`+266`) and stale legacy fixture assumptions; it now passes.
The integrated arm64 Debug Xcode build and Metal harness (1,308 checks) pass.
Live source-only appearance is user-confirmed; performance repair validation follows.

## Budgeted Direct performance repair (October 10, 2026)

Normal Direct now uses the Scene Controls connection count (14 in the user's
report), instead of silently tracing every hidden and sphere sample. Hidden
depth sources retain one full-power base/extent record each. Offscreen/held-lamp
spheres join the same receiver-specific importance CDF with the existing adaptive
solid-angle quadrature and exact unoccluded normalization. Sphere cone/basis
setup is cached once per light per CDF scan. All source classes share the
visibility-query budget; no source is discarded. Visibility/GI diagnostics and
the debug sphere retain exhaustive transport. `S9X_REMASTER_EXHAUSTIVE_DIRECT=1`
restores exhaustive normal Direct; `S9X_REMASTER_DIRECT_SAMPLES=N` overrides the
Scene Controls budget. Sampling has the existing stochastic shadow-noise tradeoff.

Actual room060 052 replay: exhaustive 444 pixel records + 384 sphere samples took
83.77 ms; budgeted 14 connections / 111 pixel records took 17.02 ms warmed
(first-use runs ~25 ms). Upper-floor linear energy 41.6489 versus 41.6362
(-0.03%); whole-frame energy 315.226 versus 315.427 (+0.06%). Uncovered 053 replay
took 14.55 ms budgeted versus 17.65 ms exhaustive. These are standalone full-command
GPU timings. 1,370 Metal checks cover collapsed/expanded hidden transport, patch
sentinels, HUD exclusion, partially occluded mixed sphere/pixel means, full/adaptive
sphere budgets, inside-sphere receivers and sphere-only frames. Live performance
after this repair needs checking.
The updated arm64 Release app build and `git diff --check` pass.
