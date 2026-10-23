# Area directional lighting

With the matching ALTTP profile loaded, open **Window > Area Settings**. The
bold heading identifies the current indoor room (`Room 0x061`, for example),
Light World, or Dark World. Settings preview immediately during emulation and
in retained captures. Valid text edits apply while typing; incomplete/invalid
values do not replace the last valid light. Save writes the loaded profile and
is disabled when all area settings match their saved state. Unsaved edits to
other areas survive room changes. Like the other profile editors, Save writes
the complete profile, including other pending profile edits.

## Defaults and coordinates

Each indoor room has its own optional override. The two overworlds each share
one setting across all outdoor screens. Indoor direction defaults to
`[0, -0.48921378, -0.87216391]`, the normalized `[0, -0.488, -0.87]`.
Outdoor direction defaults to `[0, 0, -1]`. These area-specific defaults also
apply when an area table omits its direction. Explicitly authored directions
take precedence. Default
brightness to **4 on the 0–100 UI scale**, and linear RGB to `[1, 0.97, 0.92]`. Direction describes ray
travel: +X right, +Y down, +Z toward the camera. Nonzero vectors are normalized
at render time. UI brightness is `20 ×` linear incident illumination: UI 1 is
old 0.05, UI 20 is old 1, UI 100 is old 5. The new default is linear 0.2.
Saved TOML/capture brightness retains its existing linear units, preserving
explicitly saved lights. Legacy authored values above linear 5 remain readable
but must be reduced to the UI range before editing. Color components are linear
values in `[0, 1]`.

The direction widget reuses the Pixel Inspector's unit-hemisphere control:
center is Z, rim is XY, and intermediate radii set XYZ tilt. Drag/release applies
the direction. **Back (-Z)** is checked for the default; uncheck for +Z. Numeric
XYZ controls remain available and update the widget independently of the pixel
inspector selection. Brightness has a continuous slider and numeric entry.

Unauthored indoor areas are enabled unless their original US ROM room header
has its lights-out bit set. This uses the ROM pointer table at `$04:F502`, with
the `$82:EDC5` fallback, independently of temporary WRAM darkness/torch state.
Outdoor identity comes from the overworld screen's Dark World bit (`$8A & $40`).
Area decoding is gated by the existing verified ALTTP US ROM hash.

## Persistence

### Per-area scene controls (profile v33)

**Window > Area Settings** also contains **Original Scene RGB Contribution**
(0–1) and **Representative Emitters per Tile** (1–8, with a checkbox to disable
approximation). Each indoor room stores its own settings. These two controls
share one `overworld` setting across all outdoor screens, including both Light
and Dark World; directional lights retain their separate world settings.

Areas without a saved RGB value use **0.035**, regardless of the old global RGB
setting. Missing representative-emitter values inherit the existing Profile
Controls default. Editing RGB alone does not pin that inherited emitter count.
Profile Controls labels the legacy global RGB value as applying to unknown areas
only (other games or captures without area identity).

```toml
[area_scene_settings.room_061]
original_scene_contribution = 0.035
representative_emitters = 2

[area_scene_settings.overworld]
original_scene_contribution = 0.05
representative_emitters = 8
```

Zero emitters means approximation is disabled, not that lights are removed.
Values update immediately and switch with area identity without directional-light
fading. Save persists all pending profile edits. Captures already store the
effective RGB/emitter values, so no capture-format change is necessary. Live
updates publish a small settings map without invalidating dungeon geometry.
The panel restores its new fixed size even with an older saved window placement.

Validation: `area_scene_settings_test.cpp` covers defaults, room isolation,
shared overworld, strict schema/ranges, serialization, retained/live frames,
frame-boundary publication and save merging. Existing `area_lighting_test.cpp`,
`profile_merge_test.cpp`, Profile Controls layout tests and arm64 Debug app build
pass. Interactive window/room-change/save testing remains unperformed.

Profile schema 26 adds optional named tables:

```toml
[area_lights.room_061]
enabled = true
direction = [0, 0, -1]
color = [1, 0.97, 0.92]
brightness = 0.2 # UI 4
```

Keys are `room_000` through `room_13f` (lowercase hexadecimal), `light_world`,
and `dark_world`. Missing properties use light defaults; missing tables use the
ROM-aware enable default. Existing schemas remain readable. Saves only require
schema 26 when area overrides are present (or the profile is already v26).
Duplicate tables/keys, unknown properties, zero/nonfinite directions and invalid
ranges are rejected. The optional frame `ARA1` appendix pins area identity,
ROM darkness and the effective light, so new captures retain their lighting.
Legacy captures have no area and receive no inferred directional light.

Saving automatically raises the schema to 26 when area lights are added, including
profiles with emission-depth metadata. A regression covers that combination;
emission depth must never override a newer feature's required schema version.

## Rendering

### Overworld HUD shadow continuity (October 21, 2026)

BG3 HUD ink previously removed the underlying tree pixels from the occlusion
field. Oblique rays then leaked through letter-shaped holes (rather than the
letters themselves casting shadows). Verified USA ALTTP settled overworld and
overworld dialogue frames now rasterize an independent main-screen ownership
plane with BG3 disabled. Main layer enables, clipping, priority, transparency,
scroll and tile transforms remain authoritative. This is not the previously
reverted union-main/sub ordinary-dungeon pass: stock RGB/color math is retained
exactly, and ordinary indoor rendering is unchanged.

The existing optional SCN1 plane carries this ownership and stock presentation
RGB. HUD pixels remain zero-participation receivers/sources, but an evidenced
world pixel behind them supplies its normal mesh/occlusion geometry. Mesh debug
uses that same geometry. No authored heights or normals are generated or changed.
Legacy captures without underlying ownership still lack that information and
need recapture; no neighbor interpolation or guessed geometry fills their holes.

Validation: `hud_shadow_test.cpp` checks identical hidden/uncovered world meshes,
HUD participation changes under cached identities, legacy fallback, BG3 rejection
and provisional ground. Lamp scene tests verify stock-color preservation and
existing lamp lifecycle/wire behavior. Field-cache mutation/full-preparation
equivalence passes on `frame_overworld_x0759_y04bc.s9xrmf`. Production Metal suite
passes 1393/1393, plus directional assertions that a non-participating covered
world blocker still casts a shadow and missing geometry reproduces the leak.
Release app builds. Fresh live screenshot, scrolling/dialogue review and extra
raster-pass performance remain unverified.

One GPU pass adds Lambertian reflected directional radiance after emissive
direct transport/reduction, before indirect transport. It is independent of
emitter counts and batch sizes, works with no emissive tiles, honors receiver
participation/HUD exclusion, authored normals, opposite-facing settings, albedo
and reflectance boost, and contributes to the existing indirect bounces.
Disabled lights skip the pass. The default vertical direction avoids shadow
traversal; oblique directions trace one deterministic parallel ray per lit
receiver to the viewport boundary using the existing mesh and acceleration.
Shadows use available viewport geometry; offscreen occluders are not introduced
by this feature. There is no distance attenuation or time-of-day simulation.
Directional GPU time is included in Direct metrics and also displayed separately.

Area-only edits publish the small light map at frame boundaries without copying
the tile atlas or invalidating room geometry. The UI polls an atomic area value,
not live WRAM or mutable emulation frames.

On a room change, runtime brightness interpolates linearly to the destination
over 20 emulated frames (about one third of a second). During ALTTP's scrolling
door transitions, the outgoing brightness is held through room construction;
the fade starts on the first BG camera movement in the scroll phase, rather
than the earlier WRAM room-ID change. Skipped presentation frames count toward
the duration; pausing does not advance it. Disabled rooms fade to/from zero.
Direction/color use the destination settings (outgoing settings during a fade
to disabled). Same-area authoring stays immediate once the transition finishes.
The UI and saved profile retain the authored target; captures pin the currently
interpolated runtime light. Initial profile activation snaps to the current room.
Portable regressions cover loading-phase hold, both scroll axes, 20-frame timing,
skipped/repeated frames, disabled lights, interrupted fades and same-room edits.
The arm64 Release app builds successfully; native transition appearance still
needs gameplay review.

## Verification (October 13, 2026)

- `remaster/area_lighting_test.cpp`: defaults, area isolation, strict schema,
  TOML/capture round trips, legacy capture fallback, and frame-boundary updates.
- Production Metal suite: **1377/1377** checks passed on Apple M3 Max, including
  emitter-free lighting, RGB/brightness, reverse direction, HUD exclusion,
  oblique mesh blocking, disabling and production half-float radiance.
- macOS arm64 Debug app build and `git diff --check` passed.
- Broad `remaster_test.cpp` stops at its existing assertion expecting 12 asset
  groups; the current checked-in profile has 11. Earlier checks passed.
- Native interactive room-transition/window/save testing and representative
  gameplay performance measurement remain unperformed.

## Performance follow-up (October 13)

The user's screenshots show 52.30/36.98 ms total GPU and 30.01/23.05 ms Direct
enabled/disabled, with 264 direct samples and no indirect bounces. Controlled
Release benchmarks on this M3 Max did not reproduce that directional delta:

- Slot006, 15 warmup/60 measured frames: 7.90 ms total GPU median,
  7.58 ms emitter transport, **0.011 ms directional**. Live 30/120 run: 8.04 ms
  total median, 57.66 completion FPS, 0.009 ms directional.
- Slot004, same executable/profile/state, 15/60 frames, alternating on/off twice:
  on GPU median 19.42/19.21 ms, off 19.08/19.09 ms. Directional stage 0.010 ms
  both on runs and absent both off runs. Emitter transport 18.88–19.20 ms.
- Outputs: ignored `build/area-pair-{on,off}{1,2}/report.json`, plus
  `build/area-perf-{before,counters-before,live-before,live-counters-before}/`.
  Same-state fixtures verified end RAM and original artwork. These measurements
  use default -Z, not the user's unavailable exact state/direction.
- Tried 32×1/8×4 thread layouts, early facing rejection and flat-sheet analytic
  hits. None demonstrated a reliable speedup; all were removed. No quality/sample
  reduction is enabled. Existing emitter transport remains the bottleneck.

`tools/run_remaster_benchmark.py --directional-light off` now supplies a
benchmark-only pass-disable override while retaining the same input profile;
`directional_disabled_override` is pinned in reports. The default `profile`
clears inherited overrides. Run comparisons sequentially, never concurrently.
The metrics panel now fits its full text and shows separate directional GPU ms.
The exact reported 40 FPS regression still needs the user's room capture/freeze
and light direction. This is an investigation result, not a performance repair.

Updated portable brightness/default/persistence test, 1377/1377 Metal checks and
14 benchmark/tooling unit tests pass. Updated arm64 Debug and Release apps build
successfully. Interactive dragging and hemisphere toggles
still need native user review.

## Room060 user reproduction (October 13)

User supplied `frame_room060_070.s9xrmf` and slot A (`Zelda_ALTTP.009.frz`).
Use the capture's `.profile.toml` sidecar for exact authored settings: room060
direction `[0,-0.5,-0.800000012]`, linear brightness 0.2 (UI 4), zero bounces.
State SHA256 `c9982a29bf2dc51ef8901f0add8e2d50ae6e96f321fbb59c8158dbc1251d6ee3`;
profile SHA256 `20e051f6b1727469096aaa96fd8f72ff769f24ff5a46de99910a7809459ee3ec`.
Release M3 Max, no movement, Composite0; all tests run sequentially after the
interactive app quit. No lighting/profile/quality changes were made.

| Mode | Directional | GPU median / p95 ms | Completion FPS |
| --- | --- | --- | --- |
| Deterministic, 30 warmup / 120 measured | on, run 1 | 18.43 / 19.62 | 27.89 |
| Deterministic | off, run 1 | 18.41 / 19.97 | 27.22 |
| Deterministic | on, run 2 | 18.46 / 19.62 | 27.71 |
| Deterministic | off, run 2 | 18.34 / 19.66 | 27.72 |
| Live, 30 / 300, stage counters | on | 40.43 / 73.87 | 44.08 |
| Live, stage counters | off | 42.26 / 73.20 | 44.04 |
| Live, 60 / 600, counters disabled | on | 41.46 / 75.65 | 43.91 |
| Live, counters disabled | off | 41.03 / 74.75 | 43.53 |

Deterministic directional stage median is 0.101 ms; emitter transport is
18.09–18.15 ms. Live stage medians are 0.142 ms directional and 24.42/26.27 ms
emitter transport (on/off). All 120 deterministic frames have 423 emitter pixels,
280 prepared direct samples, 215 depth emitters, no sphere lights, batch size 8.
Deterministic runs match final RAM and original artwork; each live on/off pair
matches final RAM. Live runs have different submitted subsets and cannot support
pixel-exact comparisons. Counter-free on completes 451/600 frames. Median
completion interval is 19.47 ms, mean command-buffer span 43.61 ms: overlapping
GPU frame execution means GPUStart/End spans are not exclusive per-frame work
or display-present measurements. The approximately 44 FPS problem reproduces,
but a directional-enabled penalty does not.

Existing benchmark overrides tested at unchanged quality: serial direct gives
57.62 ms deterministic / 22.22 live FPS; 32x2 and 16x4 dispatch layouts give
30.99 and 21.17 ms deterministic medians. All regress versus stock 8x8 batched;
none retained. Next work is shared emissive transport and frame-correlated GPU/
presentation scheduling, not a directional quality reduction. Reports/images/
captures are in ignored `build/room060-oblique-*/`; counter-free results are
`build/room060-oblique-live-nocounters-{on,off}/report.json`.

## Receiver-emission optimization (October 14)

Clustered exhaustive Direct now tests receiver emission once outside the source
loop. Non-emitting receivers bypass the per-source exclusion helper and its
repeated emission texture read. Emitting receivers retain the existing member
exclusion/recentering algorithm. Source counts/order, shadow rays, quadrature,
geometry, authored profile and radiance math are unchanged. Production defaults
to the hoisted path; function constant7 retains the old path for benchmarking.
Runner `--reference-receiver-emission` sets the benchmark-only override and pins
`reference_receiver_emission_override` in report.json (default clears inherited
override). This permits same-executable comparisons without changing game data.

Counter-free stationary Room060 live comparisons, 60 warmup / 3600 measured
emulated frames, same executable/profile/freeze, sequential A/B then B/A:

| Run | Completion FPS | GPU span median / p95 ms |
| --- | --- | --- |
| Reference 1 | 32.08 | 69.80 / 116.22 |
| Hoisted 1 | 35.37 | 46.64 / 74.03 |
| Hoisted 2 | 34.45 | 48.03 / 75.96 |
| Reference 2 | 34.12 | 47.31 / 78.44 |

All four have matching final RAM. Paired throughput differences are +10.2% and
+1.0%; substantial system/GPU variability prevents a stable percentage claim.
Earlier alternating 10-second pairs measured reference25.61/27.45 versus
hoisted28.14/28.05 FPS, and an earlier nearby rebuilt pair44.49 versus47.71.
Do not compare these absolute values with earlier44 FPS as a regression or
promise48 FPS: conditions changed materially even for the reference path.
Final deterministic same-build pair is18.54 versus18.44 ms median, p95
20.46 versus19.92, direct stage18.203 versus18.112 ms. Final PNGs are byte-identical
(SHA256 `aa7059e8455a01407d10f16f1e82a81fa6e732fae1c23fdc6fa00b9a57e9074d`),
with matching end RAM. Gain is modest; stable60 Hz remains unresolved.

Rejected experiments: four-source batches slightly improve some short timings
but lose to eight with hoisting (46.46 versus49.33 FPS); sixteen-source batches
are slower20.08 ms. Two in-flight frames shorten GPU spans24.24 ms but reduce
throughput42.59 FPS, so they do not repair frame rate. 8x4 threads are slower
18.94 ms. Those experiment code changes were removed.

Validation: production Metal suite1377/1377 for optimized, reference-emission,
and adaptive-depth configurations; final Release build, 14 tooling tests and
diff whitespace check pass. No native display-present deadline gate or moving
60-second corpus claim is made. Shared emissive visibility work remains the
dominant cost. Artifacts: `build/room060-long-*`, `build/room060-ab-*`, and
`build/room060-final-{reference,hoist}-image/`.
