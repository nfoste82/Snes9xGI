# Room 0x011 live CPU/GPU investigation — October 28, 2026

## Outcome

Lowering representative emitters reduces Direct GPU work, but leaves substantial
producer-thread remaster assembly. The initial investigation made no production
change; the subsequent authorized CPU optimization is recorded below. No pacing
change or room-height coverage approval is implied.

The running Release process was sampled without restarting or moving Link.
Build label: October 28 13:47:35; framework mtime 13:47:53; process launched14:00.
The dirty working source is not independently identical to the loaded binary.
With user permission, the area representative slider was changed1 ->3 ->1;
the final value1 was verified. No profile save was performed. Directional light
was already enabled and was left unchanged; indirect bounces were0.

## Live evidence

Each A/B phase collected an8-second `sample` and20 read-only Accessibility panel
observations. Observations are latest-value snapshots, not a producing-frame
trace, and sampling itself perturbs execution. Do not interpret instantaneous
FPS readings as interval-average throughput.

| Observation | Representatives3 | Representatives1 (restored) |
| --- | ---: | ---: |
| Direct samples |288–300|104|
| Direct GPU median |12.26 ms|6.805 ms|
| GPU frame median |16.735 ms|10.065 ms|
| Scene fields median / maximum |3.52 /9.75 ms|3.405 /9.78 ms|
| Presentation queue median |0.02 ms|0.02 ms|
| Drawable median / maximum |0.02 /4.12 ms|0.02 /0.04 ms|
| EndFrame assembly / engine-thread samples |4125/6473|4129/6503|
| DrawRemasterFrame / engine-thread samples |1183/6473|1205/6503|
| S9xSyncSpeed / engine-thread samples |459/6473|483/6503|

EndFrame and DrawRemasterFrame are both nested in EndScreenRefresh; their
percentages must not be added to EndScreenRefresh. These are sampled stack
observations, not measured per-frame milliseconds or recoverable-time promises.

The initial passive reps1 sample also showed30 FPS on most panel observations,
scene fields2.995 ms median/8.63 ms maximum, negligible drawable waits, and
3703/5824 engine-thread samples in EndFrame. The later A/B panel FPS alternated
20/30/60; a clean presented-frame interval trace is still needed for throughput.

## Accounting and hotspots

`gfx.cpp` starts `emulationFrameMs` at StartScreenRefresh and samples it at the
end of EndScreenRefresh. It includes packet/room assembly, scene fields and GPU
setup. `cpuexec.cpp` calls SyncSpeed afterward, so pacing sleep is separate.
Skipped frames update refresh time too: observed refresh values range from0.19
to33.72 ms. Consequently a tiny skipped-frame latest value must not be compared
with a previous rendered frame's GPU/field values. GPU and CPU stages overlap;
their milliseconds are not an additive wall-frame budget.

Specialist review of the initial sample found2289/3703 EndFrame observations at
the visible-face call (`remaster.h:3736` in the loaded build). Within it:

- 915 observations annotating scene/main/sub/observed-BG rasters, prominently
  per-pixel ordered-map clone lookup;
- 495 in all-channel OOB visibility selection;
- 328 constructing and123 destroying the full occurrence metadata resolver;
- 173 selecting height donors for exposure.

Scene-field spikes are real in the latest-value observations, but their exact
causes are not established. Field selection currently conservatively reselects
the entire raster after composition or instance changes. Flame animation can
therefore invalidate broad work even though most room geometry is unchanged.

## Next implementation boundary

Optimize invocation-local visible-face work first: share exact visible-occurrence
policy without computing all seven channel donors for OOB visibility, use compact
effective role/height inputs instead of full metadata copies, and replace repeated
ordered-map clone lookup with an exact-key fast lookup while preserving clone
order. Do not retain instance pointers across vector growth. Validate unchanged
complete rasters, claims, HUD/depth precedence, both BGs, flips, unresolved states,
context changes and missing-versus-zero authored sources against the existing
path. Cross-frame visibility caching needs explicit camera/raster/context/owner
invalidation; existing room-template caches do not establish that equivalence.

Raw local artifacts: `build/remaster-tests/room011-live-sample.txt`,
`room011-live-reps3-sample.txt`, `room011-live-reps1-restored-sample.txt`,
`room011-live-metrics.jsonl`, `room011-reps3-metrics.jsonl`, and
`room011-reps1-restored-metrics.jsonl`.

## Follow-up optimization baseline

The user subsequently authorized optimization of the visible-face CPU hotspot.
Baseline source is commit `32a59dc69c958e65ce11a8f6f6b26d9a6e2bdba0`.
Own-input `frame_room011_020.s9xrmf` SHA-256 is
`e3968a7342f3162bd2e9b6ae3929a07499c7316ee1000478866cf71999a1ddd0`.
The existing complete-input replay probe matched111,829 raster samples and
measured14.318 ms median /14.739 ms p95 for50 warmed EndFrame invocations.

Separately, `lighting_critical_path_probe.mm` exercised actual EndFrame and the
production renderer (only drawable acquisition replaced with an offscreen
target). Apple clang17 `-O2`,31 repeated own-input frames, first five discarded:
visible stage7.271 ms median, EndFrame finalization3.801 ms, projection/apply
1.654 ms, total exclusive CPU service18.946 ms. CPU service includes packet
assembly and rendering setup, not full SNES emulation. GPU median11.255 ms is
not a live prediction; a separate live emulator remained running. Outputs and
traces are under `build/remaster-tests/room011-visible-face-baseline-renderer*`.
Additional baseline output packets/Direct/composite buffers were retained for
room011_019, room012_038 and a controlled camera-toggle sequence. These are
regression inputs, not complete-room correctness approval.

Actual-renderer probe build from the repository root:

```sh
xcrun clang++ -std=c++17 -O2 -fobjc-arc -D__MACOSX__ -DHAVE_STDINT_H \
  -I. -Imacosx -Iapu -Ifilter -Iapu/bapu -I"macosx/snes9x framework" \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  remaster/tools/lighting_critical_path_probe.mm \
  -framework Cocoa -framework Metal -framework MetalKit \
  -framework QuartzCore -framework Carbon -o /path/to/probe
S9X_REMASTER_DISABLE_STAGE_COUNTERS=1 LIGHTING_REPLAY_ENDFRAME=1 \
  S9X_REMASTER_LIGHTING_TRACE=/new/path/trace.jsonl \
  LIGHTING_OUTPUT=/new/path/output \
  /path/to/probe /path/to/frame_room011_020.s9xrmf 41
```

The packet's contemporaneous profile and complete native sidecars are required;
current-profile substitution is not used. Repeated observations do not reproduce
a live animation or traversal sequence. `LIGHTING_REPLAY_EVENT=camera` toggles
the camera as an explicitly controlled invalidation test, not new gameplay evidence.

Before candidate integration, the broader ALTTP Python discovery command
(`python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_*.py'`)
ran354 tests and was **not green**: two existing assertions in
`test_diagonal_sets`/`test_door_pot_regions`, four import errors (missing Pillow),
and seven skips. Log: `build/remaster-tests/visible-face-alttp-python-gate-complete.log`.
This optional discovery is separate from the curated portable runtime gate;
its failures must not be presented as optimization regressions or a passing gate.
The general tools Python suite passed16 tests and profile-controls layout passed9.

## Retained CPU optimization

`dungeon_visible_face_runtime.h` now uses an invocation-local value-owned subset
of effective height, role and normal presence instead of copying full occurrence
metadata for OOB visibility. `frame.h` shares the exact visible-occurrence
precedence without calculating unused channel donors. Clone lookups use exact
hashed `(instance,cell,base,publish)` keys; clone creation still follows the
original raster first-encounter order. No authored data, light sampling, room
solve or cross-frame cache behavior changes.

Actual-renderer baseline/optimized/optimized/baseline legs,41 observations per
leg, first five discarded:

| Leg | Visible CPU median | Total CPU service median |
| --- | ---: | ---: |
| Baseline A1 |7.650 ms|20.165 ms|
| Optimized B1 |3.412 ms|15.382 ms|
| Optimized B2 |3.395 ms|15.160 ms|
| Baseline A2 |7.524 ms|19.181 ms|

The visible stage is approximately55% faster, saving about4 ms in this snapshot.
Total CPU service falls approximately22%; these instrumented offline replay
numbers are not live FPS predictions or compliance with the5 ms total budget.
Pooling the72 warmed observations per variant gives7.589 ->3.409 ms visible
CPU (4.180 ms saved) and19.604 ->15.282 ms total CPU service (4.322 ms saved).
Visible-stage p95 falls8.190 ->3.587 ms; total CPU-service p95 falls20.684
->16.097 ms across those same warmed observations.
GPU medians drifted12.2–14.2 ms without any transport change; no GPU speedup is
claimed. Packet, Direct and composite bytes match across all four legs.

All18 settled own-input room011 captures with complete sidecars also produced
byte-identical packet/Direct/composite outputs. Room012_038 and room061_079,
plus controlled camera/plane toggles, cold invalidation and profile resets,
matched too. This is bounded captured-state parity, not live traversal/corpus
height approval. Manifests: `visible-face-corpus-final/manifest.json` and
`visible-face-extra-parity.json` under `build/remaster-tests/`.
Separate31-frame replays (26 warmed observations) show smaller visible-stage
savings in room012_038:3.694 ->3.159 ms, and room061_079:2.894 ->2.590 ms,
with exact packet and GPU output parity. Those unpaired timings are less
controlled than the room011 ABBA; do not generalize its4 ms saving to every room.

The new `visible_face_work_test.cpp` compares1024 compact sources against the
full resolver,32768 visible-precedence cases against a frozen selector and192
synthetic complete-output states against the pre-change fingerprint
`18198169831120924552`. It forces instance-vector growth and covers both BGs,
flips, HUD/depth/OBJ precedence, contexts, zero/missing sources and unresolved
states. The allocation bound fails baseline1,022,084 bytes and passes optimized
941,568 bytes. Narrow Debug/optimized/ASan/UBSan checks and actual Room011_020
EndFrame ASan/UBSan replay passed; complete packet/inventory/maps unchanged.

Integration: arm64 Release and Debug Xcode builds passed, including the unchanged
Metal transport source. The independently compiled production Metal harness
passed1396/1396 checks,456 visibility reference comparisons and65,536 patch
probes (zero mismatches). The updated product is `build/release/Snes9x.app`.
The curated full portable gate passed75/75, including constrained-stack Debug
and optimized tests; result manifest:
`build/remaster-tests/20261028T210430Z-n7yy4mio/results.json`.
The running application was intentionally not restarted. Live before/after FPS,
animation/scroll appearance, and whether rendered refresh time drops by the
offline saving still need interactive confirmation after a restart. Remaining
CPU stages (notably frame finalization and field preparation) and GPU transport
mean this optimization alone does not establish60 FPS or the5 ms total target.
The optimization is exact work elimination, not a new approximation: no reduction
in representative counts, depth rays, source power or occluders. The next CPU
profiling candidates remain finalization (baseline median3.8 ms) and field
selection/preparation; reducing them requires separate measured work and
invalidation evidence rather than extending this patch opportunistically.

Independent repair-role review reran the new regression atO0/O1 and with
ASan/UBSan and found no correctness regression for valid frames. Remaining test
limits: the randomized selector differential does not explicitly generate
observed-depth ties; the allocation threshold is host/library-sensitive with
only8,432 bytes of headroom. The gate's source manifest includes headers but
does not fingerprint included `.cpp` fixture helpers, so the retained Git diff
and baseline pins remain necessary provenance. No universal numeric/performance
or complete-room correctness claim is made.
