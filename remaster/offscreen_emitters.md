# Completed-room emitter capture

## October20 room012 north-pillar diagnosis (unrepaired)

Own-input production replay of `frame_room012_008.s9xrmf`, profile serialized FNV
`8cd9be1bf06a3123`, room0x012, module7/submodule0/construction0, scroll[1157,604],
finds156 genuinely emitting placements:12 rejected unknown artwork bases,
545 hidden source pixels and1599 visible emitting pixels on accepted placements.
The12 rejected BG2 cells form two2x3 groups: x25..26 and37..38, y16..18,
consistent with the reported north pillars. All12 `offsets` are255; four top-row
cells have support-only base0 but no solved artwork base, and the other eight
lack support too. Existing conservative rejection at `offscreen_emitters.h`
therefore removes their sources as they leave the visible path. Emission exists;
this is not missing emission painting or a light-distance cutoff.

No room-height rule or fallback was changed. Support-under-art is not proof of
artwork placement height. Required room-height-expert delegation failed to launch
twice (unavailable configured `medium` variant). A reusable pillar-placement/base
capability, failure-class tests, complete-room validation v2 and corpus audit remain
needed; this source audit is NOT height-coverage approval. Local reproduction:
`build/remaster-tests/room012-emitter-probe` with `ALTTP_EMITTER_PLACEMENTS=1`;
artifact/log prefix `build/remaster-tests/room012-008-source-audit`.

## Held-lamp presentation

Link's held lamp defaults to a forward offset of 10 source pixels (formerly 12).
Its radius 2, height 12, intensity 200 and warm color are unchanged. Held lamps
set `RemasterFrameLight::hideSource`: Composite/Difference and Emission omit the
source sphere, while direct/indirect transport still receives the complete light.
Authored and session debug lights retain their visible spheres by default.
The optional `LVP1` capture appendix stores sparse hidden-light indices; packets
without it retain visible-source defaults. Older readers reject the new appendix
rather than silently losing presentation state. Existing captures retain their
original offsets/presentation and need recapture for the new held-lamp defaults.

Held-lamp anchor/state/HUD and capture round-trip/malformed-index tests pass.
The Metal harness passes 1387/1387 checks, including unchanged exhaustive and
sampled Direct when hiding a scene sphere. arm64 Debug and Release builds pass;
live gameplay appearance has not been checked.

## October12 camera-edge source continuity (current)

Room fixtures no longer use the sphere approximation described in the historical
sections below. Missing offscreen and HUD-covered pixels use the same exact
signed pixel sources: canonical emission/normal/depth plus existing placement-local
height/base. Visible pixels remain on the authoritative path; no world-artwork
occlusion recovery or ordinary scene pass is enabled. Deterministic Direct and
existing hidden representative clustering remain in use. Held/debug spheres are
unchanged. Historical sphere reduction is available ONLY as the standalone
`ALTTP_TEST_LEGACY_OFFSCREEN_SPHERES` regression oracle, never a runtime setting.

`HES2` has the same33-byte record layout asHES1 but allows room-local signed
screenXY[-511,511.5] and the full8192-placement x64-pixel source denominator.
In-viewport-only packets retainHES1; existing HES1 validation stays strict.
Older readers rejectHES2 instead of misreading it. Signed clustering uses floor
division/positive remainders, preserving negative-coordinate tile buckets.

Pinned room050015 profilec331a975d3bd9963, complete WRAM/VRAM/templates/planes,
87,827 matched BG pixels:8/8 emitting placements solved, unknown0, base12 on
all8 with existing local backing ramps;97 captured emission records unchanged.
Before/after complete height-map inventories are byte-identical: no solver or
height metadata change. At capture scroll[256,2744],190 missing pixels/total
alpha6370 previously became6 spheres; now retain190 exact signed sources.

Production Metal fixed-world/phase/receiver/occluder source sweep97 cameraY
positions144..48, using actual015 authored source data: old sphere/visible
switch energy1.8137207..21.2890625 (ratio11.7378), while alpha power stayed6370
at every step. First north row transition67->66:19.84375->17.375 (-12.44%).
Exact pixel-depth repair:1.8137207 everywhere; unchanged representative3
clustering:1.80786133 everywhere (96 records vs760 individual strata, -0.323%
approximation). Clustered sweep harness mean0.538ms on M3Max for128x17 test
scene, NOT live256x224 performance.1378/1378 Metal tests pass. The test receiver
sees an OVERbright old offscreen sphere, opposite the user's darker live region:
isotropic sphere replacement is receiver/normal dependent. This proves source
discontinuity, not exact reproduction of the user's full-frame dark patch.

Portable optimized+ASan/UBSan tests cover partial visibility area/power, planar
and depth coordinates/flips, signed source clustering, HES2 roundtrip/bounds,
unknown-base rejection. Historical sphere/HUD/base oracle tests pass unchanged
apart from selecting the legacy test macro and updated out-of-range negative
capture check. Shared smoke test still fails its preexisting line739 legacy
conversion fixture (v30 palette payload not removed); it is not repaired here.
Final integration: arm64 Release `xcodebuild` and `git diff --check` passed.
Updated product: `build/release/Snes9x.app`. Live edge appearance and full-frame
performance remain to be checked after quitting and reopening the app.

Limits: this isolates unoccluded source transport by reprojecting visible source
members through exact depth records in fixed-world coordinates; it is not a
new full gameplay capture. Live visible mesh-derived normals/endpoints and
occlusion geometry may still differ from absent offscreen shells. Visible and
missing sources are clustered separately, so partial-visible representative
selection can retain a smaller approximation difference; complete-fixture
clustering across both paths is not claimed. Offscreen wall occluders remain
unmodeled. Fresh gameplay edge sweeps and live frame-time validation are required.
No room-height approval is claimed; existing485 unknown room050 walls remain.

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
coverage, current extinction/animation, serialization/malformed input), and 1,377
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

## Superseded budgeted Direct experiment (October 10, 2026)

This experiment made normal Direct use the Scene Controls connection count (14 in the user's
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

## Deterministic Direct restoration (October 11, 2026)

The user reported newly noisy direct torch shadows at 14 connections, requiring
50–60 to suppress noise. That regression came from the sampled default above.
Normal Direct is again deterministic: the connection count controls indirect
lighting only. `S9X_REMASTER_DIRECT_SAMPLES=N` remains an explicit stochastic
experiment; it is never enabled from the normal Scene Controls count.

HUD-hidden sources now use the existing emitter footprint/representative settings
on small CPU-only 8x8 support grids. Rectangle centroids require compatible flat
height/normal/depth; actual-member representatives retain authored support and
can span varying height/normal. Planar/depth and omnidirectional modes remain
separate. RGB power and source area are conserved; duplicate XY emitters fall
back to individual records so two room planes cannot overwrite each other.
Hidden sentinels skip visible-patch receiver exclusion but participate in the
same deterministic depth refinement and area normalization as visible patches.
No hidden receiver or occluder geometry is added.

Pinned room060 052: 384 individual hidden depth records become 72 clustered
records, total Direct records 444 -> 132. Upper-floor energy 41.6489 -> 41.9123
(+0.63%), whole-frame energy 315.226 -> 315.797 (+0.18%); this is the same
representative approximation class as visible emitters, not exact per-pixel
shadow identity. Uncovered 053 retains its prior deterministic source list.
Latest room061 069 has 48 planar records and 10 spheres / 1024 maximum sphere
samples, matching the reported performance screenshot; it exercises the restored
deterministic sphere path. Offline timings in this session compete with a running
live emulator and are not clean standalone/live-performance evidence.

Portable checks cover source power/area, deterministic clustering, actual-member
support, rectangle footprints and duplicate-plane fallback. 1,377 Metal checks
pass, including seed/connection independence for deterministic hidden Direct,
cluster sentinels, source transport and all existing visibility/sphere regressions.
Live shadow appearance and post-restoration frame time require user confirmation.
ASan/UBSan source tests and both real HUD capture regressions pass; the arm64
Release app has been rebuilt successfully. Stock scene/owner/lighting fields
remain unchanged by source recovery.
