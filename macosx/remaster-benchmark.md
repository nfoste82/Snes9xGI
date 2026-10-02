# Live application lighting benchmark

Run from an unlocked macOS desktop session. The runner opens the specified ROM,
loads its matching profile, restores the save state on the emulator thread before
each case, and ignores physical controller/keyboard input during the benchmark.
It exits automatically with status 0 on completion, 2 on app-side failure.

```sh
python3 tools/run_remaster_benchmark.py \
  --app "$HOME/Library/Developer/Xcode/DerivedData/snes9x-atcnlahonwpbthgwbglgovfsrdlq/Build/Products/Release/Snes9x.app" \
  --output build/benchmark-slot004-right \
  --move right --seconds 3
```

Defaults use `~/Downloads/Zelda_ALTTP.zip`,
`~/Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.004.frz`, and the
repository's `alttp-profile.toml`. Override with `--rom`, `--state`, `--profile`.
The output directory must be new. `--connections` overrides the profile setting.

Cases: Direct lighting only; Direct + Indirect at 0/1/8 bounces; Composite at
0/1/8 bounces. The profile supplies other lighting settings. The debug sphere is
disabled. Audio output is muted automatically, including after each save-state
restore. GPU diagnostic/readback environment variables are disabled by the runner.

`--move none|left|right` holds the direction for the measured emulated frames;
warmup (default 0.5 emulated seconds) has no input. Each case restores the state
and resets the shader random seed. End-state RAM hashes verify case consistency.
Whether movement actually scrolls depends on the selected save state's location;
the script cannot force scrolling through an obstructing wall.

`--presentation deterministic` (default) renders every emulated frame and waits
for completion: identical animation/movement frames are compared across cases.
Three emulated seconds means 180 measured frames at 60 Hz, and can take minutes
of wall time when rendering is slow. `--presentation live` keeps normal bounded
asynchronous presentation and frame dropping; its submitted frame subset depends
on performance. This mode is useful for real gameplay throughput, but not
pixel-exact comparisons. Both force frame skip 1 and disable sound synchronization.

`report.json` contains input hashes, executable/build identity, hardware, settings,
per-frame GPU and matched field-preparation timings, throughput, dropped counts,
and median/p95/p99 summaries. Emulator refresh/pacing values are explicitly latest
samples and can refer to a previous frame. Frames/s is GPU-completion throughput,
not a display-present callback measurement. GPU duration includes all command
buffer graphics. `gpu_stages_ms` additionally isolates compute encoders using GPU
timestamp counters on devices supporting stage-boundary sampling (macOS 11+).
Set `S9X_REMASTER_DISABLE_STAGE_COUNTERS=1` to disable sampling without disabling
whole-command-buffer metrics. Reports record the override. `submit_to_gpu_ms`
measures from a host uptime marker after drawable/render encoding immediately
before handler registration/commit to GPUStartTime. `driver_ms` is Metal's
kernelStartTime/kernelEndTime interval, not CPU-active time or additional GPU work.
Repeated `source_power` and `indirect_transport` intervals are summed per frame;
each stage's statistics include a sample count. Empty stage dictionaries mean
counters were unavailable. These intervals exclude render/present and encoder
gaps; sampling can itself affect performance. PNGs and `.s9xrmf` captures
are written after each case outside its timed interval. Reports are checkpointed
after each case so completed results survive interruption. Benchmark shutdown
does not save benchmark-driven SRAM.

## Verified baseline (October 1, 2026)

M3 Max, Release build, slot 004, repository profile (96 connections), right held
for 180 measured frames after 30 warmup frames. All seven cases had 180 completed
samples and matching final RAM hashes. The final image confirms Link moved to
the right and the scene scrolled. GPU median/p95 milliseconds:

| Case | Median | p95 |
| --- | ---: | ---: |
| Direct only | 230.03 | 477.31 |
| Direct + Indirect, 0 | 229.46 | 475.98 |
| Direct + Indirect, 1 | 401.73 | 693.50 |
| Direct + Indirect, 8 | 448.10 | 821.98 |
| Composite, 0 | 225.67 | 475.47 |
| Composite, 1 | 270.53 | 534.23 |
| Composite, 8 | 422.80 | 652.28 |

Scene-field medians were 4.84–6.94 ms; mesh medians 2.92–4.09 ms.
These are whole-command-buffer timings from serial deterministic presentation,
not isolated pass timings. The direct+indirect and composite one-bounce variation
needs repeated profiling before interpreting it as a view-specific cost.
The complete run took roughly ten wall-clock minutes. A separate live-mode smoke
run completed all seven cases with normal dropped frames and matching RAM hashes.

### Direct-shader specialization

The production direct pass uses a Metal function constant to compile out radial
indirect quadrature and diagnostic branches. Visibility/diagnostic/reference
passes retain the unspecialized variant and existing triangle intersection path.
Analytic flat-patch and early-triangle-rejection experiments were removed after
live measurements regressed; neither changes the final geometry path.

The same muted, right-moving, 180-frame/96-connection run then measured:

| Case | GPU median | GPU p95 |
| --- | ---: | ---: |
| Direct only | 84.81 | 168.93 |
| Direct + Indirect, 0 | 83.59 | 169.72 |
| Direct + Indirect, 1 | 125.37 | 207.61 |
| Direct + Indirect, 8 | 252.33 | 343.69 |
| Composite, 0 | 84.36 | 170.40 |
| Composite, 1 | 126.88 | 210.16 |
| Composite, 8 | 256.19 | 344.61 |

All cases completed 180 samples with matching RAM hashes. The lighting suite
passed 1215/1215 checks. Synthetic full-frame direct comparisons differed in
5–7 half-float channels per scene (of 229,376); every difference was within one
half-float step, maximum relative error below 0.00086. Specialization can change
compiler rounding, so direct comparisons enforce that explicit tolerance rather
than bit identity. Synthetic open-scene timings did not consistently improve;
this optimization is validated against the authored slot-004 workload, not a
claim of universal speedup. Earlier/later live results also include machine load
and thermal variability and are not simultaneous A/B measurements.

The app also accepts these options directly:
`--remaster-benchmark --benchmark-rom PATH --benchmark-state PATH
--benchmark-profile PATH --benchmark-output PATH --benchmark-seconds 3
--benchmark-warmup-seconds 0.5 --benchmark-move right
--benchmark-presentation deterministic --benchmark-connections 8`.

## Session handoff: resume GPU optimization

Read `AIDocs/00-index.md` first, then this note and `remaster/lighting.md`.
The user wants GPU frame time addressed before CPU scene fields. Benchmark mode
and direct-source preparation/specialization are implemented; CPU field caching,
buffer reuse, and material-resolution optimization have not yet been implemented.
The prior preparation/specialization work is committed as `dc3fafc1`; the
visibility/counter follow-up below is currently uncommitted. Inspect `git status`
and diff before continuing, and preserve the current working tree.

Relevant code:
- `tools/run_remaster_benchmark.py`: launch/setup and report summary.
- `macosx/mac-os.mm`: benchmark state machine, save-state reset, input sequence,
  report output, and automatic audio mute.
- `macosx/remaster-benchmark.h`: producing-frame tags and completion sample ABI.
- `macosx/mac-render.mm`: GPU source preparation, specialized pipeline creation,
  frame-tagged timing collection, draining, screenshots, and mesh/wait metrics.
- `macosx/shaders.metal`: source preparation and direct function constant;
  `remasterVisibility` and `remasterMeshHit` are the next profiling targets.
- `macosx/remaster-lighting-test.mm`: production shader regression suite.
- `macosx/remaster-lighting-benchmark.mm`: synthetic direct comparison (`--direct`).

Next steps:
1. Use the new isolated GPU stage timings for the authored room, and obtain finer
   visibility traversal/intersection counters or a controlled captured-frame
   microbenchmark before further algorithm changes. Verify traversal,
   triangle intersection, and emitter enumeration costs before another algorithm
   change. Direct currently enumerates every emissive sample for every receiver;
   zero indirect bounces does not avoid direct transport.
2. Reduce receiver × emitter visibility work, preserving fractional opacity,
   geometric endpoints, emission-depth energy, and current mesh semantics. Consider
   tighter conservative acceleration or source reuse; do not assume analytic flat
   patch paths are faster (the attempted shortcuts regressed and were removed).
3. Compare identical slot-004 movement sequences and captured outputs; retain
   reference visibility comparisons and the documented half-float tolerance.
4. Address scene fields after the GPU work: distinguish waits from actual work,
   reuse scratch storage, pre-resolve materials, remove redundant gradient normal
   work, then consider geometry caching with correct movement/edit invalidation.

No room-height metadata was generated or approved in this performance work. If
future changes involve ALTTP room-height/floor/wall placement generation or
validation, follow the repository's required room-height-expert delegation.

Validation commands (temporary parent already exists):

```sh
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra \
  macosx/remaster-lighting-test.mm -framework Foundation -framework Metal \
  -o /var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-fast-test
/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-fast-test
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra \
  macosx/remaster-lighting-benchmark.mm -framework Foundation -framework Metal \
  -o /var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-specialized-benchmark
/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-specialized-benchmark --direct
xcodebuild -quiet -project macosx/snes9x.xcodeproj -scheme snes9x \
  -configuration Release -destination "platform=macOS,arch=arm64" \
  CODE_SIGNING_ALLOWED=NO build
git diff --check
```

Raw reports/images/captures from this session are local temporary artifacts:
- Baseline: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-slot004-right-3s/`
- Final specialization: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-direct-specialization-only-3s/`
- Rejected visibility experiments: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-gpu-specialized-right-3s/`

Temporary artifacts may be cleared by macOS; the tables and reproduction
instructions above are the durable record. `AIDocs/` is intentionally local-only
and ignored, while this note and the benchmark tools can be tracked normally.

## Visibility follow-up (October 1, 2026)

Retained changes:
- 4×4 height-envelope blocks instead of 8×8. All allocation/build/query paths,
  including standalone tests, use the same size. Pixel DDA rounding is preserved.
- Stream two successive triangle-edge vertices instead of dynamically indexing
  an array of four. Same triangles/test order, no analytic geometry shortcut.
  The reference macro retains the original array expansion.
- Benchmark-only stage-boundary Metal timestamp counters; each producing frame
  owns its sample buffer through GPU completion.
- `tools/compare_remaster_benchmarks.py` checks input identity, final RAM, original
  pixels, measured counts, PNG byte identity, and prints median/stage comparisons.

Short screening runs (6 warmup + 15 right-moving measurement frames): original
Direct median 153.78 ms; 4×4 alone 145.88; 4×4 + streamed edges 119.69; streamed
edges + 8×8 143.64. Smaller 4×8/8×4 transport threadgroups did not convincingly
improve performance; default stays 8×8. 2×2 blocks regressed to 196.62 ms and were
removed. Explicit four-edge loop unrolling made no convincing difference and was
also removed. Short initial-room timing is not comparable to the full scrolling
sequence below.

Full right-moving run: 30 warmup + 180 measured frames, 96 connections, muted,
deterministic, M3 Max. Same inputs as the preceding specialization benchmark.
All seven PNG files are byte-identical to that benchmark and to the 8×8 streamed
edge control; final RAM and original RGB555 hashes match across runs.

| Case | Earlier specialization median | 8×8 streamed-edge control | Final 4×4 + streamed edges median/p95 |
| --- | ---: | ---: | ---: |
| Direct only | 84.81 | 79.05 | 66.55 / 134.75 |
| Direct + Indirect, 0 | 83.59 | 79.58 | 67.21 / 131.54 |
| Direct + Indirect, 1 | 125.37 | 123.88 | 105.20 / 163.03 |
| Direct + Indirect, 8 | 252.33 | 250.56 | 226.38 / 306.44 |
| Composite, 0 | 84.36 | 79.69 | 67.93 / 126.35 |
| Composite, 1 | 126.88 | 123.70 | 104.85 / 180.85 |
| Composite, 8 | 256.19 | 269.35 | 230.30 / 349.76 |

Timings are milliseconds, sequential runs rather than interleaved A/B; thermal
and machine-load variation remains. PNG identity checks the final case image,
not every measured frame's float radiance. The independent lighting suite passes
1215/1215, including 456 visibility comparisons in float/half formats. Synthetic
direct checks still have 5–7 half-channel differences, all within the documented
one-half-step tolerance. Synthetic timing remains variable and is not evidence
of universal speedup. Release build and `git diff --check` pass.

Direct transport's final median stage duration is 66.49 ms. At one bounce, direct
is 66.04 and sampled indirect 36.88 ms; source-power construction is 0.049 ms,
envelopes 0.027 ms, preparation 0.006 ms, compositing 0.008 ms. At eight bounces
direct is 67.57 and summed indirect 154.90 ms. Thus receiver/source transport
remains the next target, rather than power-tree reductions or compositing. Even
zero bounces remains far above the 16.7 ms 60 FPS budget.

Raw full reports/captures are in the local temporary parent listed above:
- `gpu-edge8-right-3s/`: streamed-edge 8×8 control.
- `gpu-edge4-right-3s/`: final 4×4 + streamed-edge full sequence.
- `gpu-stage-8x8/`, `gpu-stage-4x8/`, `gpu-stage-8x4/`, `gpu-stage-block4/`,
  `gpu-stage-block4-unroll/`, `gpu-stage-block2/`, `gpu-stage-block4-edge/`,
  `gpu-stage-block8-edge/`: short screening runs.

Compare two completed sequences:
```sh
python3 tools/compare_remaster_benchmarks.py BEFORE_OUTPUT AFTER_OUTPUT
```

Optional benchmark-only execution-layout experiment:
`S9X_REMASTER_TRANSPORT_THREADS=8x4 python3 tools/run_remaster_benchmark.py ...`.
The report records the requested override; shader/source enumeration is unchanged.

Final-build repeat (including the independent array-based reference and 512-entry
counter buffer) reproduced the improvement, with all 180 frames/case and all seven
PNGs byte-identical to the preceding specialization baseline:

| Case | Final-build median | p95 |
| --- | ---: | ---: |
| Direct only | 66.39 | 126.06 |
| Direct + Indirect, 0 | 66.32 | 123.97 |
| Direct + Indirect, 1 | 105.38 | 163.22 |
| Direct + Indirect, 8 | 225.54 | 300.26 |
| Composite, 0 | 66.57 | 128.84 |
| Composite, 1 | 105.00 | 162.59 |
| Composite, 8 | 227.91 | 299.25 |

Raw final-build repeat: `gpu-edge4-final-right-3s/` under the same temporary parent.
All stage counts are 180. Direct stage median 66.31 ms; one-bounce indirect 37.52;
eight-bounce summed indirect 157.35. Use this report as the next baseline.

## Parallel transport follow-up (October 1, 2026)

The user's live screenshot showed 671.64 ms GPU, 323.87 ms drawable wait, and
37.54 ms CPU lighting with 9 connections/zero bounces. Its old panel text did not
include the newer mesh/wait/emitter rows. Process inspection found the running
app at the DerivedData **Debug** path; the earlier measured app was **Release**.
The screenshot's exact scene/build/environment was not captured, so it is not
an A/B measurement against the benchmark. Both configurations are rebuilt now;
the running process must be restarted to load them.

Retained changes: eight-element source/connection GPU batches plus float32 partial
reduction, existing geometry/visibility and sampling seeds, 64 MiB per-slot cap
with serial fallback, and removal of gradient-normal calculations always replaced
by mesh normals. Direct still enumerates every authored emitter even at zero
bounces; the connection slider controls indirect work. Stage counters now also
run when the live metrics checkbox is enabled. The panel reports build timestamp
and measured direct/indirect GPU intervals.

Rejected experiments: barycentric early rejection offered little gain; a fan-plane
prefilter regressed; reversing rays failed 10 sphere/reference checks (including
offscreen endpoints). All three were removed. Batch screening: 32 sources/batch
about 50 ms initial-room Direct, 8 about 45 ms, 4 about 43 ms but twice the partial
storage. Eight is retained. Full scrolling workload is cheaper than the initial
room, so do not compare screening medians to the full sequence.

Full Release, 30 warmup + 180 right-moving frames, 96 connections, deterministic:

| Case | Previous serial median | Batched median | Batched p95 |
| --- | ---: | ---: | ---: |
| Direct only | 66.39 | 22.00 | 50.91 |
| Direct + Indirect, 0 | 66.32 | 21.88 | 50.24 |
| Direct + Indirect, 1 | 105.38 | 37.97 | 74.50 |
| Direct + Indirect, 8 | 225.54 | 101.60 | 157.43 |
| Composite, 0 | 66.57 | 22.69 | 52.66 |
| Composite, 1 | 105.00 | 38.06 | 84.89 |
| Composite, 8 | 227.91 | 107.86 | 168.54 |

All cases completed 180 samples, matched final RAM/original pixels. PNGs are exact
for five cases. At eight bounces, Direct + Indirect differs in 14 8-bit channels
and Composite in 3, all by exactly one byte value. Synthetic direct differs in
42–53 half-float channels per 229,376-channel scene, all within one half-float
step (maximum relative difference below 0.000975). Serial and batched regression
suites each pass 1215/1215. Summation grouping changes rounding; later power-tree
proposals can respond to small radiance changes. Do not claim bit-identical GI.

Release **live/asynchronous** mode, 9 connections, three emulated seconds/case:
GPU medians 53.03, 48.20, 60.14, 62.25, 53.38, 62.01, 60.46 ms; p95
145.92, 138.73, 157.08, 127.10, 152.51, 146.93, 118.48. Completed throughput
24–35 frames/s with drops. Direct stage medians 23–30 ms. The remaining elapsed
time needs queue/scheduling profiling: inter-encoder gaps and concurrent workloads
may contribute; stage sums need not equal whole-frame elapsed time.
Debug short deterministic screening at 9 connections measured Direct 45–48 ms,
CPU fields about 35 ms (mesh about 23 ms); Release CPU fields about 3–6 ms.
This does not achieve a 16.7 ms total budget in either mode.

Next targets: control asynchronous encoder/queue scheduling overhead, parallelize
or accelerate the remaining visibility work (without failed ray reversal), then
reduce CPU mesh allocations/material-resolution cost. Profile both deterministic
and live modes. Avoid measuring concurrent standalone GPU tests alongside the
app; other running GPU apps also introduce variability.

Artifacts under the same local temporary parent:
- `gpu-batched-final-right-3s/`: new deterministic baseline, both transports batched.
- `gpu-batch8-right-3s/`: Direct-only batching; later cases had machine-load noise.
- `gpu-batched-live-9/`: asynchronous live report, 9 connections.
- `gpu-batched-debug-smoke/`: rebuilt Debug screening, 9 connections.

Verification and comparisons:
```sh
S9X_REMASTER_TEST_BATCHED=1 /path/to/remaster-lighting-test
S9X_REMASTER_TEST_BATCHED=1 /path/to/remaster-lighting-benchmark --direct
python3 tools/compare_remaster_benchmarks.py BEFORE_OUTPUT AFTER_OUTPUT
swift tools/compare_remaster_images.swift BEFORE_OUTPUT AFTER_OUTPUT
```
`S9X_REMASTER_SERIAL_DIRECT=1` / `S9X_REMASTER_SERIAL_INDIRECT=1` force serial
transport for controlled A/B. Reports record these overrides. Changes remain
uncommitted; preserve the working tree. Restart the tested Release app at
`~/Library/Developer/Xcode/DerivedData/snes9x-atcnlahonwpbthgwbglgovfsrdlq/Build/Products/Release/Snes9x.app`
for gameplay performance comparisons.

## Resolved geometry cache follow-up (October 1, 2026)

User confirmed Release timings and set a target of approximately **5 ms total
lighting overhead**, with emulator and older-PC headroom. Implemented CPU geometry
reuse and per-instance material-selector resolution. `surface_mesh_cache.h` compares
resolved samples; dirty outputs expand three pixels and 16×16 output tiles rebuild
with an additional three-pixel input halo through the original mesh builder.
More than half the tiles dirty, dimension changes, or quantum changes use a full
build. Cache access is serialized by `renderMutex`. Emission depth/authored shading
normals refresh every frame, and GPU uploads remain complete for each resource slot.

This requires no static metadata: moving walls change effective heights/domains/
coverage at their old/new positions and invalidate naturally. Current viewport
boundaries match full reconstruction. Retaining whole meshes offscreen is deferred:
the frame lacks stable world-space object mapping and authoritative offscreen
changes. Retained history must not invent blockers or weld against offscreen pixels.

Sequential full three-second deterministic runs, 96 connections:

| Case | Cached GPU median/p95 | Cached fields/mesh | Full-build fields/mesh |
| --- | ---: | ---: | ---: |
| Direct | 21.76 / 49.09 | 2.09 / 0.78 | 3.14 / 2.13 |
| Direct + Indirect 0 | 22.08 / 49.28 | 2.06 / 0.77 | 3.25 / 2.20 |
| Direct + Indirect 1 | 36.26 / 71.68 | 2.26 / 0.83 | 4.68 / 3.18 |
| Direct + Indirect 8 | 96.35 / 152.78 | 2.49 / 0.92 | 4.45 / 3.00 |
| Composite 0 | 22.00 / 49.15 | 1.76 / 0.67 | 3.15 / 2.12 |
| Composite 1 | 36.11 / 70.19 | 2.24 / 0.83 | 3.17 / 2.13 |
| Composite 8 | 97.97 / 157.54 | 1.73 / 0.67 | 5.37 / 3.63 |

All 180 samples/case, matching end RAM. All seven final PNGs are decoded-pixel
identical both to the previous batched baseline and forced full-mesh control.
GPU timing differences reflect run/load variation; **no GPU acceleration retained**.
CPU cache versus forced-full uses the same material pre-resolution implementation.

Rejected visibility experiments (both passed 1215 regressions): a second 16×16
envelope carried in RGBA alongside 4×4 bounds cost ~0.15 ms to construct and
raised Direct to ~25 ms; checking the last interior cell against the empty-block
proof measured ~23.4 ms versus ~21.8 after removal. Both removed. Future hierarchy
work should avoid repeated coarse reductions and per-ray 15-step boundary math.

Final live run at 9 connections: GPU medians in case order **31.40, 33.59, 40.24,
54.13, 31.79, 41.16, 55.15 ms**; CPU fields 2.90–3.17 ms, mesh 0.98–1.08 ms;
throughput 27–39 completed frames/s. Sample counts 144/146/136/104/148/139/106.
Live timings are variable and are not a controlled cache-versus-full live A/B.
Still well above 5 ms. No manual moving-wall-room playthrough performed.

Artifacts in the existing temp parent:
- `gpu-cache-final-right-3s/`: retained CPU changes, original GPU traversal.
- `gpu-full-mesh-control-right-3s/`: `S9X_REMASTER_FULL_MESH=1` control.
- `gpu-cache-final-live-9/`: final asynchronous 9-connection run.
- `gpu-cache-hierarchy-right-3s/`, `gpu-cache-fine-right-3s/`: rejected GPU screens.

Verification: mesh suite 35/35 (240-frame exact full/cache differential sequence),
serial and batched Metal suites 1215/1215 each, Debug/Release builds, diff check.
Reports now record `full_mesh_override`. New cache header and prior comparison
tools are untracked; preserve them along with all other uncommitted work.
Next: more efficient visibility representation/traversal, CPU scratch reuse,
then authoritative world-space cache only if placement/state inputs support it.

## CPU scratch and field-pass follow-up (October 1, 2026)

User confirmed the Release cache improvement and unchanged visuals. Continued
with capacity reuse for owner/highlight, byte/float field, mesh-sample and mesh
staging vectors. All contents reset every frame under `renderMutex`; synchronous
uploads finish before queued asynchronous presentation. Fused metadata/material,
height, authored-normal and mesh-sample resolution from three pixel passes into
one. Invalid pixel ownership retains the same zero/default fields as before.
No mesh-builder/visibility semantics or GPU transport changes retained.

Two GPU candidates removed: rejecting triangle t outside the DDA interval before
barycentrics, and guarding sphere setup when disabled. Both passed the batched
1215-check suite but offered no convincing measured gain. Unrelated composite
and source-power stages also slowed versus earlier runs, suggesting external
load/scheduling variation; timings are not isolated A/B evidence.

Final full 30+180-frame, 96-connection deterministic benchmark
`gpu-scratch-fused-right-3s/` (same existing temporary parent):
- GPU medians in case order: 28.40, 28.00, 47.40, 122.99, 27.92, 46.00, 121.98 ms.
- CPU fields/mesh: 1.98/0.73, 2.26/0.80, 2.64/0.93, 2.55/0.95,
  2.47/0.92, 2.55/0.95, 2.78/0.97 ms.
- All seven final PNGs decoded-pixel identical to `gpu-cache-final-right-3s/`;
  matching RAM and 180 samples/case. No extra rounding differences.
- No claimed GPU speedup, nor controlled CPU speedup estimate from these noisy
  sequential runs. CPU changes remove allocations/passes and preserve outputs.
- Candidate reports: `gpu-scratch-interval-right-3s/`,
  `gpu-scratch-sphere-right-3s/`.

Serial and batched Metal suites each pass 1215/1215 with final runtime shaders.
Debug/Release rebuilt and diff check passes. Further GPU work needs a larger
representation/traversal change and controlled profiling, rather than small
triangle arithmetic branches. The approximately 5 ms total goal remains unmet.

## Profiling findings (October 1, 2026)

Added `tools/analyze_remaster_profile.py` (stdlib-only report and xctrace XML
summaries), stage-counter disable override, submission-to-GPU and driver intervals.
Lighting output and execution defaults are unchanged. All seven deterministic
PNG outputs match between counter-on/off runs, with matching end-state RAM.

Four full right-moving 30+180-frame runs at **9 connections**, counters in ON,
OFF, OFF, ON order. Whole-GPU medians in standard seven-case order:

| Run | Direct | D+I 0 | D+I 1 | D+I 8 | Composite 0 | Composite 1 | Composite 8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| profile-on-a | 26.14 | 25.72 | 28.95 | 42.54 | 27.96 | 29.96 | 42.07 |
| profile-off-a | 30.03 | 32.00 | 40.13 | 56.51 | 41.04 | 49.02 | 72.05 |
| profile-off-b | 34.04 | 34.04 | 38.02 | 52.37 | 36.86 | 39.32 | 51.11 |
| profile-on-b | 36.97 | 36.00 | 41.70 | 55.97 | 33.67 | 36.94 | 50.45 |

**Measured priorities:**
- Direct transport is ~99.2% of summed compute-stage intervals at zero bounces,
  ~85–86% at one bounce, ~57–59% at eight (9 connections). Emitter preparation,
  envelope construction and direct reduction are tiny by comparison.
- Effective direct list contains **192–366 planar samples/frame**, all enumerated
  independently of connection count. At 256×224 this is approximately 11–21 million
  receiver/source pairs before participation/facing rejection (not visibility-ray
  count). Source count/scene changes contribute to scrolling p95 costs.
- CPU fields 1.6–2.9 ms, GPU setup ~0.4–0.6 ms normally. Submission-to-GPU generally
  ~0.1–0.6 ms deterministic; drawable ~0.01 ms and presentation queue ~0.001 ms.
- Eight-bounce sampled counters dramatically change `driver_ms`: ~38–49 ms ON
  versus ~0.07–0.09 ms OFF. Kernel intervals are elapsed scheduling/driver spans,
  not additive CPU work. Counter overhead cannot be quantified from these noisy
  runs: disabled timings are not consistently faster and drift is large.

Live ON/OFF at 9 connections:
- ON GPU medians 75.24/67.75/77.69/84.93/73.00/85.03/72.08 ms, throughput 19–29/s.
- OFF 66.38/59.22/68.18/106.35/62.77/68.05/116.37 ms, throughput 21–33/s.
- ON per-frame whole-GPU minus summed-stage median ~20–26 ms at 0/1 bounces;
  4.6–5.4 ms at eight. This residual includes render and GPU scheduling gaps,
  and is not automatically app encoder overhead.
- Eight-bounce ON submission-to-GPU ~42–50 ms, OFF ~0.13–0.15 ms. Driver spans
  likewise ~43–49 ms ON versus ~0.06–0.07 ms OFF. Counters alter scheduling.
- Queue/drawable medians ~0.01 ms in these runs: the older screenshot's huge
  drawable wait is not reproduced here. Live subsets differ, so PNGs aren't A/B.

**Native profiler captures:** available templates include Time Profiler and Metal
System Trace. `xctrace --launch` resolved the Debug bundle despite a Release
executable argument; discard those first launch captures as Release evidence.
Launch the executable with subprocess first, then use `--attach PID`. Exported
CPU stack binary paths confirm the attached trace loaded the Release framework.
Attached captures are 8 seconds each, cover early Direct/zero-bounce work rather
than the whole scrolling sequence, and are perturbed by profiler overhead.

CPU trace: 1050 one-ms running-thread samples across the process. Inclusive
weights (overlap, **not additive**): DrawRemasterFrame 42.5%, mesh Cache::update
23.9%, frame finalization 13.5%, profile-to-frame resolution 8.3%, Metal setup/
upload S9xPutImageMetal 6.5%. Emulation thread 87.2% of sampled active CPU. This
reveals CPU remaster/profile work outside the panel's scene-field measurement;
5 ms total overhead must account for that too. Waiting threads are excluded.

Metal trace shows Snes9x GPU intervals split/interleaved with **Warp (`stable`),
WindowServer, and Firefox GPU Helper**. Direct encoders have median three splits,
~58.75 ms summed execution intervals / ~61.00 ms elapsed span in this early-room
trace. Presentation vertex/fragment intervals summed ~0.66 ms but span ~61.93 ms
because stages can straddle compute: do not count that span as render cost.
Other-process duration sums overlap and are not GPU-utilization percentages.
This is direct evidence of concurrent GPU traffic, not proof of a particular
bandwidth/occupancy bottleneck. The stock Metal template reports shader timeline
disabled and no counter set; **register pressure, occupancy, cache misses, and
triangle-vs-DDA instruction costs remain unmeasured**. Next requires a shader
profiler/counter-enabled Instruments template or Xcode GPU frame capture under
a quieter desktop workload. Do not assume memory-bound versus arithmetic-bound.

Artifacts under the existing temporary parent:
`profile-on-a/`, `profile-off-a/`, `profile-off-b/`, `profile-on-b/`,
`profile-live-on/`, `profile-live-off/`, `remaster-release-cpu.trace`,
`remaster-release-metal.trace`, `release-cpu.xml`, `release-metal.xml`.
The first 15-second Metal launch capture timed out saving and is malformed;
the short launch capture and launch CPU capture used Debug. Use attached traces.

Reproduce (launch benchmark separately, attach to its PID):
```sh
xcrun xctrace record --template 'Time Profiler' --time-limit 8s --output cpu.trace --attach PID
xcrun xctrace record --template 'Metal System Trace' --time-limit 8s --output metal.trace --attach PID
xcrun xctrace export --input cpu.trace --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' --output cpu.xml
xcrun xctrace export --input metal.trace --xpath '/trace-toc/run[@number="1"]/data/table[@schema="metal-gpu-intervals"]' --output metal.xml
python3 tools/analyze_remaster_profile.py OUTPUT_DIRECTORY cpu.xml metal.xml
```
Analysis uses Python's conventional median; benchmark summaries select sorted
percentile samples, so even-sized medians can differ slightly. Next priorities:
disable stage counters for shipping-cost measurements, quieter GPU workload,
shader-level profiling of Direct, then profile/mesh CPU input resolution.

### Pinned-frame Direct work profile follow-up

The installed Instruments **Metal GPU Counters** instrument can be added with
`--instrument 'Metal GPU Counters'`, but its default **Performance Limiters**
profile is rejected on this M3 Max: "Selected counter profile is not supported
on target device". Exports `gpu-counter-info` and `gpu-shader-profiler-sample`
have **zero rows**, even though the TOC says shader timeline enabled. Public
`MTLDevice.counterSets` exposes only `timestamp / GPUTimestamp`. Consequently
these captures do not establish occupancy, register pressure, or cache misses.
The counter-disabled trace coalesces seed/preparation/Direct into one interval
group (~43.1 ms median sum / 43.6 ms span, four splits). Competing GPU work
remains visible; no quieter-desktop performance claim is made.

Added two opt-in deterministic benchmark helpers:
- `S9X_REMASTER_GPU_CAPTURE=/new/path/frame.gputrace`, together with
  `MTL_CAPTURE_ENABLED=1`: capture first measured Direct frame. Optional
  `S9X_REMASTER_GPU_CAPTURE_FRAME=60` selects a later absolute case frame index
  (warmup included). Capture begins before resource uploads/command creation,
  ends after GPU completion, and is restricted to deterministic benchmarks.
- `S9X_REMASTER_DIRECT_SNAPSHOT=/new/path/snapshot`: dump first measured batched
  Direct frame's immutable CPU GPU-input arrays, uniforms, full mesh, source list,
  and completed application's Direct half-float output. Directory must be new.
  Snapshot readback/writes perturb that frame; don't use it for shipping timing.

Single-frame `remaster-direct-frame.gputrace` was saved with resource contents,
but capture-enabled app teardown **crashed in CaptureMTLDevice deallocateResource /
CaptureMTLBuffer dealloc** during static resource destruction after all seven
cases and report output. This is an unresolved capture-layer teardown issue;
that run is not a successful benchmark and the document has not been replay-
validated in Xcode. Ordinary snapshot run exits successfully.

`tools/profile_remaster_direct.mm` loads the snapshot and rebuilds production
seed, 4x4 envelopes and prepared samples, then measures transport alone. Shader
source is compiled at runtime; diagnostic scratch is shared for readback rather
than production private. It builds isolated instrumented copies of visibility
helpers; shipping `shaders.metal` is unchanged. Local invocation counters avoid
global atomics. It requires counted float32 partials to exactly match ordinary
partials and, when `direct-result.bin` exists, requires reduced half-float Direct
to **exactly match the application's pinned GPU output**. Both checks passed.

Pinned slot-004 case0 frame30: **256x224, 288 samples, 36 batches**, shader FNV
`3af616679a99b3c1`, mesh FNV `86616fe468455c55`, source-list FNV `c8ba46622faac0d1`.
Counted work (same for 8x8 and 32x1 thread layouts):

| Work | Count |
| --- | ---: |
| visibility rays after facing/form-factor rejection | 12,718,513 |
| DDA while-loop iterations (including envelope skipping) | 554,481,813 |
| 4x4 envelope tests | 432,023,551 |
| mesh patch tests (origin included) | 207,432,088 |
| mesh height-interval early rejects | 122,606,271 |
| triangle intersection calls | 1,315,322,436 |
| empty-block iterations | 385,854,494 |
| mesh hits causing coverage attenuation | 4,339,082 |

Approximately **43.6 traversal iterations and 103.4 triangle calls per ray**.
59.1% of mesh calls reject on height range; 69.6% of traversal iterations enter
the empty-block path. Many calls remain after envelope skipping, and mesh hits
are only 2.1% of patch tests. These are exact work counts for one viewport/frame,
not complete-room geometry coverage or a whole scrolling-sequence average.

Four variants rotate order each round (4 warmups +12 measured each), no stage
counters; measured GPU medians in the verified 8x8 run:
- production transport **77.94 ms** (minimum75.26)
- visibility forced to1 **0.63 ms**
- mesh hit forced false, keeping traversal **35.18 ms**
- exact work-counted transport **79.17 ms**

Earlier repeat: 86.73/0.68/38.60/97.93 ms. 32x1 layout: 82.42/0.72/40.54/85.82 ms,
same exact results/counts; no demonstrated layout improvement. Disabled variants
alter shadowing, early exit, generated code/register allocation and dead-code
elimination. Their deltas **are not additive component timings** or valid lighting
modes. They nevertheless identify visibility/traversal/intersections as the
high-value next target, rather than emission preparation or form-factor math.

Next optimization experiment should reduce triangle candidate work and empty-
space traversal using exact geometry bounds/representation, then compare this
pinned result and full movement benchmarks. Larger envelope schemes already
tested slower; avoid simply repeating them. GPU instruction/cache hardware
classification is still unresolved; Xcode shader profiling requires manual
inspection of the captured document or a supported custom counter template.

Reproduce snapshot with stage counters disabled, then (no app benchmark running):
```sh
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra tools/profile_remaster_direct.mm \
  -framework Foundation -framework Metal -o /temporary/path/profile-remaster-direct
/temporary/path/profile-remaster-direct /path/to/snapshot
/temporary/path/profile-remaster-direct /path/to/snapshot 32x1
```
Local artifacts: `remaster-shader-counters.trace`, `shader-counters-toc.xml`,
`shader-counter-info.xml`, `shader-samples.xml`, `shader-counter-timeline.xml`,
`direct-snapshot-verified/`, `profile-direct-verified-run/`,
`direct-profile-8x8.log`, `direct-profile-32x1.log`, and the capture document above.

### Candidate and empty-space experiments

Reviewed geometry/endpoint requirements with room-height-expert before testing.
Side-face XY-plane interval rejection reduced pinned triangle calls from
1,315,322,436 to 971,545,432 (26.1%). Kept top/bottom fans and original side
triangle tests/order; reference path bypassed rejection. Pinned float32 partials
and application's half output stayed exact. Interleaved 12-sample medians:
48.74 ms filtered versus51.11 ms unfiltered (about4.6%).

However, the fixed margin `0.001 + abs(rayAxis)*2e-6` is not a universal numerical
proof: near-parallel Moller–Trumbore predicates and large-coordinate cancellation
can disagree with a cheap plane interval. Added fail-open bounds for coordinates,
Z, thickness and near-parallel rays. That variant retained exact output and
reduced calls to983,551,930, but **regressed**:59.17 ms versus55.24 ms unfiltered
in the same interleaved run. Branches/register pressure may explain the reversal;
hardware evidence is unavailable. Both variants were initially removed.

Cached 4x4 outgoing boundaries, retaining repeated additions rather than `4*delta`,
also matched pinned partials but measured59.72 versus59.17 ms in the same run.
Removed; no new acceleration resource or pipeline is retained. At that point the
production shader was restored exactly, and no movement A/B was run.

Retained a meaningful narrow-phase regression:65,536 batched randomized patch/ray
probes compare raw hit booleans against independent reference expansion. Includes
short intervals, zero XY components, near-side boundary cancellation, folded
heights, shells and wall closures.4213 reference hits, zero mismatches for tested
candidate versions. Final suites now1216 checks plus456 full-frame visibility
comparisons; raw probes are not a proof for unbounded floating-point inputs.

**BVH next-experiment constraints:** patch leaves can reject much larger groups
than per-triangle arithmetic, but eligibility must match DDA lanes/corner ties,
origin/final intervals and per-patch fractional-opacity attenuation order. Bounds
include shells/wallBase; leaves keep original intersection predicate. Traversal
storage overflow must fall back, not lose geometry. Build/refit and resources
must match each producing frame and in-flight slot. A simple unordered hardware
triangle first-hit query is not equivalent. No BVH implemented in this pass;
its build/traversal cost and exact boundary strategy remain to be measured.

**User-requested recovery:** restored the initial, faster fixed-margin side filter
as the default shader path for visual evaluation. The guarded variant and cached
block-boundary experiment remain removed. Independent reference bypass remains.
This accepts possible extreme-coordinate/near-parallel disagreement; it is not
a universal exactness claim. Fresh serial/batched suites pass1216/1216 each,
with65536 raw patch probes and zero mismatches. Pinned partial FNV remains
`f3e5d505d986f6f7`, application's half output stays exact, and triangle calls remain
971,545,432. Debug/Release builds and diff check pass. Latest unpaired transport
median53.52 ms is not a fresh A/B speedup measurement; the historical ~5% result
remains workload-dependent. Review by restarting the DerivedData Release app.

**Visual acceptance (October 1, 2026):** user reports the triangle reduction looks
good with no observed issues and accepts it as the default. It is no longer
experimental. Numerical limitations above remain documented; visual acceptance
does not establish universal floating-point equivalence.

### Arrival-block traversal follow-up

Reviewed with room-height-expert: existing traversal examines the first arriving
patch before its block envelope. Two prototypes checked the envelope earlier,
retaining repeated-addition DDA and origin handling. First duplicated the
block check at arrival with broadphase padding; second consolidated the check
through a pending-cell state and retained final-cell processing.

Both reproduced pinned float32 partials exactly, but neither improved transport:
first46.88 ms baseline versus48.11 ms prototype; consolidated48.38 versus49.27 ms
(12 interleaved measured rounds each, four warmup rounds). Removed both. These
are single immutable-frame diagnostics, not scrolling performance claims.

Next larger candidate remains an ordered patch BVH. It must reproduce grid-lane
eligibility and accumulated interval arithmetic while reducing patch reads;
ordinary unordered triangle first-hit traversal would change opacity semantics.
No BVH implemented or performance gain established by this follow-up.

### Ordered patch BVH prototype

Tested a profiling-only CPU-built XY spatial BVH, then removed the prototype
at the user's request after all measured variants regressed. The
application retains the visually accepted side filter and existing 4x4 traversal.
BVH nodes use 40-byte bounds/child records; split-sign depth-first order preserves
relative order of DDA-eligible cells. Reconstruct per-axis crossing times using
the original repeated additions, commit only eligible intervals, and run the
unchanged patch predicate with once-per-patch opacity. Grouped leaves use local
pixel DDA. Bounds include corners, center, thickness and wall base. Stack overflow
and outside origins fall back to production visibility.

Pinned256x224/288-source comparisons, rotating five variants with four warmup
and12 measured rounds (transport only; build/upload excluded):

| Maximum leaf footprint | Nodes | BVH median ms | Paired production ms |
| --- | ---: | ---: | ---: |
| 1x1 | 114687 | 168.160 | 52.831 |
| 4x4 | 8191 | 115.294 | 52.997 |
| 8x8 | 2047 | 83.533 | 52.624 |
| 16x16 | 511 | 71.494 | 56.696 |

After input pinning/finite-geometry validation and expanded negative-diagonal,
zero-length and near-tie probes,16x16 measured67.046 versus49.942 ms. ItsCPU
construction was1.069 ms,20,440 bytes/depth8, nodeFNV`1d87a623560dd9d5`, helper
FNV`4fd11ea7cd119531`. Earlier construction metrics included input reads and are
not runtime build-cost estimates. Timings drift, but every interleaved comparison
lost substantially; no production integration or scrolling A/B warranted.

All tested leaf sizes:65,536 visibility probes with zero mismatches, exact full
float32 Direct partials and exact application half output. Final expanded probes
compare visibility bits. This is bounded equivalence evidence, not universal
proof: slab padding `.001 + abs(ray)*2e-6` is empirical, and explicit interval-bit
and ordered-hit trace comparison plus broader synthetic materials remain future
validation before any integration. The measured benefit/cost of node visits,
speculative additions and patch reads has not been separately instrumented; their
roles in the slowdown remain hypotheses, not hardware-counter conclusions.

Removed the BVH shader helper, CPU builder, resources, differential probes, leaf
size override and fifth profiling variant. The profiler again runs its original
four variants; retained the input-pinning improvement and explicit skipped
application-output-check message. These findings are historical experimental
results, not an available application or profiling mode. Room-height-expert
reviewed design and implementation; no height metadata generated or approved.

### Sampled Direct first implementation

`S9X_REMASTER_DIRECT_SAMPLES=N` opts into stratified receiver-specific source
sampling, N1..4096; exhaustive default and debug-sphere/visibility/diagnostic
fallback remain. See `remaster/lighting.md` for exact-denominator estimator.
No temporal filtering. Benchmark reports record raw `direct_samples_override`
and fallback note; per-frame `direct_samples` continues to mean available
prepared emitter samples, not the requested sampling budget.

Pinned288-source frame, seven rotating variants/four warmup/12 measured rounds:

| Direct mode | Transport median ms | Relative RMS of 12-frame mean |
| --- | ---: | ---: |
| Exhaustive | 47.358 | reference |
| Sampled16 | 26.937 | 0.007782 |
| Sampled32 | 33.813 | 0.004878 |
| Sampled64 | 41.057 | 0.002050 |

Offline12-frame averaging is an estimator diagnostic, **not** live denoising.
Original exhaustive partials/app half output still exact. New tests cover one
emitter, fractional visibility, colored-source statistical mean, authored black,
fixed seeds, emission-depth energy and a dense opposite-facing receiver fixture
whose denominator exceeds0.95.1226 checks pass, plus456 visibility comparisons
and65536 patch probes. Debug/Release app and profiler compile successfully.

Production right-moving deterministic runs:30 warmup+180 measured frames per
case,9 indirect connections, stage counters disabled, identical profile SHA256
`5b5faa04df5fafd9d153e0455bb0255ccfc20f39205b5f3cd0df7790c612f4b9`.
Available source sequence456..1152 is identical between runs. User authored
emission extent on one torch animation frame; depth samples expand each emitter
to four samples. Existing profile changes were preserved.

| Case | Exhaustive wholeGPU median/p95 ms | Sampled16 median/p95 ms |
| --- | ---: | ---: |
| Direct | 60.10/303.46 | 29.27/39.95 |
| Direct+Indirect0 | 56.81/308.64 | 29.39/40.34 |
| Direct+Indirect1 | 57.69/307.98 | 32.42/44.20 |
| Direct+Indirect8 | 72.59/325.51 | 45.50/58.68 |
| Composite0 | 55.63/308.43 | 29.38/40.48 |
| Composite1 | 57.87/309.54 | 32.92/43.65 |
| Composite8 | 69.54/324.77 | 45.33/58.09 |

Both runs complete; RAM and original RGB555 hashes match across cases/runs.
Lighting images intentionally differ. Source-count strata for Direct:456 samples
(54frames)41.97 versus26.97 ms;768 (51frames)216.39 versus35.87 ms;1152
(9frames)393.29 versus39.40 ms. Scheduling/load drift still prevents interpreting
all elapsed differences as pure shader time. Pinned interleaving supports an
improvement but5ms is not reached. Initial300s baseline attempt timed out at case7
and is excluded. Artifacts: `sampled-direct-exhaustive-right-complete/` and
`sampled-direct-16-right/` under the session temp directory.

Launch review build with
`S9X_REMASTER_DIRECT_SAMPLES=16 /path/to/Snes9x.app/Contents/MacOS/Snes9x` after
quitting the running app. Sampling is opt-in pending visual noise review. Next
work: parallelize cheap source weighting/selection, investigate denoising/history
with dynamic-scene invalidation, and add a user-facing independent Direct budget.

### Default emitter patches and size control

Added default2 maximum emitter footprint with Scene Controls integer slider1..5,
numeric input, undo/live preview/profile persistence and captured replay setting.
Size1 is unclustered comparison. Profile schema15/frame22; old files default2.
`remaster/emitter_patches.h` partitions same-instance/tile-local-bin full emitting
rectangles, preserving color fragments, summed RGB and quarter-stratum depth area.
See lighting contract for compatibility and approximation. Room-height-expert
identified stepped-endpoint and exclusion-normal hazards; final rules require
matching flat support/normals. No height metadata generated or approved.

First strict draft produced no merges. Relaxed-height prototype reached232..552
sources and21.28 ms median, but was replaced after endpoint review. Final safe
rules produce296..960 sources versus456..1152 before clustering on identical
right-moving sequence. Final sampled16/9indirect connections/30warmup+180measured,
stage counters disabled, seven cases complete with matching RAM:
Direct25.65/38.14 ms median/p95;Direct+Indirect0/1/8:25.81/37.02,
29.33/41.49,41.67/56.12;Composite0/1/8:25.76/37.83,28.92/41.34,
41.91/56.28. Historical unclustered Direct29.27/39.95 is not an interleaved
causal speedup estimate. Artifact `cluster2-safe-final/`; looser prototype
`cluster2-sampled16-final/` is superseded.60FPS/5ms target remains unmet.

Portable tests verify max-footprint membership conservation1..5, holes, distinct
instances, color/depth/stepped-support/normal splits, profile/frame roundtrips and
legacy frame loading. Metal suites1230 checks,456 visibility comparisons and65536
patch probes; GPU aggregation singleton equivalence and sampled/exhaustive patch
agreement covered. Visual review of larger footprints/near-source lighting pending.

### Collapsed sampled depth records

Sampled Direct now uses one base record per emitter pixel/patch instead of four
positive-depth records. The existing four depth weights are evaluated as vectors;
source selection uses their mean and each selected connection conditionally
samples one of those depth positions. Visibility is deduplicated per source AND
stratum. Exact unoccluded area normalization and full RGB energy are retained.
Exhaustive/debug-sphere/visibility/GI diagnostic fallbacks keep expanded records.
`S9X_REMASTER_EXPANDED_SAMPLED_DEPTH=1` enables comparison with expanded sampling;
the report records `expanded_sampled_depth_override`.

Apple M3 Max Release paired right-moving runs, sampled16, patch size2,
9indirect connections,30warmup/180measured, stage counters off. Final artifacts
`depth-expanded-final/` and `depth-collapsed-final/`. Profile SHA256
`5b5faa04df5fafd9d153e0455bb0255ccfc20f39205b5f3cd0df7790c612f4b9`;
all seven cases have matching initial/original/RAM hashes across both runs, and
matching emitter-pixel/frame sequences. Prepared-record range296..960 becomes
140..318 (mixed planar/depth emitters mean total counts need not fall exactly4x).

Expanded -> collapsed whole-GPU median/p95 ms:
- Direct:26.28/38.16 ->26.06/36.35.
- Direct+Indirect0/1/8:26.24/37.83 ->26.28/36.81;
  29.37/42.35 ->29.64/40.53;42.32/55.97 ->42.41/56.30.
- Composite0/1/8:26.65/38.40 ->26.32/37.21;
  29.92/42.73 ->29.43/41.22;42.72/57.59 ->42.53/56.18.

Performance is largely unchanged; do not claim a meaningful end-to-end speedup
from this pair. Four factor calculations and expensive visibility remain.
Earlier scalar collapsed run Direct25.80 vs26.73 ms expanded also showed only
small changes.60FPS/5ms remains unmet. Frame images differ due to changed random
selection; live visual/noise review pending.

Metal suites1240/1240 serial and batched,456 visibility comparisons and65536
patch probes unchanged. New cases cover unoccluded exact depth energy, fixed-seed
determinism, fractional partial-stratum blockers and multi-seed means, mixed
planar/depth/color, depth-patch self exclusion, singleton exclusion, half-float
output and dense depth normalization clamp. Debug/Release builds pass.

### Stationary flat-torch animation investigation

User reported fast flat-emitter replay but periodic live slowdowns. Recent unified
logs contained profile-load/capture-open events, not per-frame timing; existing
`frame.s9xrmf` remained schema21 with depth2 torch metadata,96connections, so it
was not a matching flat-emitter input. Current saved profile was schema15,
patch5/14connections/zero authored depth. Stationary slot004 benchmark demonstrates
three repeating nine-frame phases. With sampled16, Direct prepared sources
204/144/282 correspond to emitter pixels288/306/366 and GPU medians approximately
23/15.5/25.7ms. CPU fields remain about1.5ms/mesh0.5ms; timestamped GPU preparation
about0.01ms, Direct transport dominates. Slowness persists throughout the phase,
not only its first frame. No evidence supports light destruction as primary cost.

Added producing-frame metrics `source_build_ms`, `depth_emitters`, and
`emitter_signature` (hex FNV over emitter location, RGB/intensity, shading and
endpoint mesh geometry; excludes transient instance IDs). Signature identifies
effective source phases, not universal animation assets; geometry/movement also
changes it. `S9X_REMASTER_FRAME_METRICS=1` logs those fields plus CPU fields/mesh/
setup, GPU, queue/drawable, drops. Log mode now collects CPU fields without needing
the metrics panel. Benchmark reports include same fields; analyzer groups phases
and compares changed/held frames. Timestamp instrumentation still perturbs timing.

Source-list/patch CPU work at patch5 measured1..2.4ms every frame, not just phase
changes. Removed repeated gamma pow decoding per candidate (predecode each
active pixel once), and stop rectangle scans immediately on incompatibility.
Preserves rectangle search order, patch membership and color thresholds.
Before/after `flat-torch-phase-metrics/` / `flat-torch-phase-optimized/` sampled16,
patch5/14connections,30warmup/60measured frames/case, counters off: median source
build Direct1.384->0.295ms, Composite0 1.787->0.365ms (about79% lower).
All source counts/signatures/depth counts match every measured frame; all seven
end-frame PNGs byte identical and state/original hashes match. All depth counts0.
Direct GPU22.58->22.35ms; CPU saving does not solve transport phase cost. Larger
Composite8 GPU change52.13->43.67ms includes scheduling drift, not this CPU edit.
Portable patch fixtures pass; Release/Debug builds and diff check validated.

Next useful GPU optimization targets source scans/visibility workload across
flame phases. Caching three source templates only addresses the smaller CPU
setup fraction; caching complete lighting needs receiver/blocker invalidation
and a deliberate strategy for independent per-frame sampling noise.
